#include "terminalwindow.h"
#include "MFRC522.h"
#include <QVBoxLayout>
#include <QTimer>
#include <QDateTime>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <iomanip>
#include <vector> // C++ 标准库向量，用于做离线缓存队列
#include <QDebug>
#include <QCryptographicHash> // 密码学哈希类
#include "devicebackend.h"
using namespace std;

// salt 盐值
QString secretKey = "HelloWorldDrivingSchool@2026_Pi4B";
RfidThread::RfidThread(DeviceBackend *backend) : m_backend(backend) {}
// 硬件线程 断网重连与离线容灾 (Edge Cache)
void RfidThread::run() {
    MFRC522 mfrc;
    mfrc.PCD_Init();

    string server_ip = "10.129.101.194"; // 电脑 IP
    int port = 8888;
    int sock = -1;
    bool isConnected = false;

    QString currentStudentCard = "";
    qint64 startTime = 0;

    // 【论文核心：边缘计算离线缓存队列】
    std::vector<QString> offlineCache; 

    while (1) {
        // 1. 网络保护 断网自动重连机制
        if (!isConnected) {
            if (sock != -1) { close(sock); sock = -1; }
            sock = socket(AF_INET, SOCK_STREAM, 0);
            
            struct sockaddr_in server;
            server.sin_addr.s_addr = inet_addr(server_ip.c_str());
            server.sin_family = AF_INET;
            server.sin_port = htons(port);

            // 尝试连接 PC 端
            if (::connect(sock, (struct sockaddr *)&server, sizeof(server)) == 0) {
                isConnected = true;
                emit networkStatusChanged(true); // 通知 UI 变绿灯

                // 断点续传 网络恢复了，检查有没有积压的离线数据
                if (!offlineCache.empty()) {
                    for (const QString& cachedJson : offlineCache) {
                        QByteArray payload = cachedJson.toUtf8();
                        // 补发数据
                        send(sock, payload.constData(), payload.size(), MSG_NOSIGNAL);
                        usleep(100000); // 延时 0.1 秒，防止 TCP 粘包
                    }
                    offlineCache.clear(); // 补发完毕，清空缓存
                    emit cacheSizeChanged(0); // 清空缓存后通知 UI 变为 0
                }
            } else {
                emit networkStatusChanged(false); // 通知 UI 变红灯
            }
        }

        // 2. 优先处理网络回传 防止刷卡拦截
        if (isConnected && sock != -1) {
            char recvBuf[256];
            // 保持非阻塞读取
            int bytesRead = recv(sock, recvBuf, sizeof(recvBuf) - 1, MSG_DONTWAIT);
            if (bytesRead > 0) {
                recvBuf[bytesRead] = '\0';
                QString serverReply = QString::fromUtf8(recvBuf);
                emit serverAckReceived(serverReply);
            } else if (bytesRead == 0) {
                isConnected = false;
            }
        }
        
        // 3. 硬件轮询 (如果没有卡，立刻进入下一次循环)
        if (!mfrc.PICC_IsNewCardPresent() || !mfrc.PICC_ReadCardSerial()) {
            usleep(50000); 
            continue;
        }

        // 4. 业务逻辑 (解析卡号与状态机)
        stringstream ss;
        for (int i = 0; i < mfrc.uid.size; i++) {
            ss << hex << uppercase << setfill('0') << setw(2) << (int)mfrc.uid.uidByte[i];
        }
        QString cardId = QString::fromStdString(ss.str());

        QString actionStr;
        int duration = 0;

        if (currentStudentCard.isEmpty()) {
            currentStudentCard = cardId;
            startTime = QDateTime::currentSecsSinceEpoch();
            actionStr = "上车签到";
        } else if (currentStudentCard == cardId) {
            duration = QDateTime::currentSecsSinceEpoch() - startTime;
            actionStr = "下车签退";
            currentStudentCard = ""; 
        } else {
            actionStr = "非法拦截";
        }

        emit cardScanned(cardId, actionStr, duration);

        // 5. 数据发送与容灾落盘
        if (actionStr != "非法拦截") {
            // 安全机制
            // 1)获取当前时间戳
            qint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();
            // 2)生成原始签名 = cardId + Act + Duration + Timestamp + salt
            QString signOrigin = cardId + actionStr + QString::number(duration) + QString::number(currentTimestamp) + secretKey;
            // 3)加密签名(生成 32 位密文) = signOrigin + MD5 算法
            QByteArray signMd5 = QCryptographicHash::hash(signOrigin.toUtf8(), QCryptographicHash::Md5).toHex();

            const QString json = QString("{\"CardID\":\"%1\", \"Action\":\"%2\", \"Duration\":%3, \"device_id\":\"%4\", \"timestamp\":\"%5\", \"sign\":\"%6\"}\n").arg(cardId, actionStr).arg(duration).arg(m_backend->getCpuId()).arg(currentTimestamp).arg(QString(signMd5));
            
            // 发送失败或长时间收不到回复时强制断开
            if (isConnected) {
                const QByteArray payload = json.toUtf8();
                // MSG_NOSIGNAL 防止进程崩溃(防止 Linux 因为网络断开而杀掉程序)
                ssize_t sentBytes = send(sock, payload.constData(), payload.size(), MSG_NOSIGNAL);
                if (sentBytes <= 0) {
                    // 连接已经断开
                    isConnected = false;
                    close(sock);
                    sock = -1;
                    emit networkStatusChanged(false); // 直接变红
                    offlineCache.push_back(json); // 缓存
                    emit cacheSizeChanged(offlineCache.size());
                }
            } else {
                // 没网，直接存入离线缓存
                offlineCache.push_back(json); // 存入缓存
                emit cacheSizeChanged(offlineCache.size());
            }
        }

        sleep(2); // 防抖
    }
}

// 下面是 UI 界面部分（已废弃，界面交给 QML）
// TerminalWindow::TerminalWindow(QWidget *parent) : QWidget(parent) {}
// TerminalWindow::~TerminalWindow() {}
// void TerminalWindow::updateTime() {}
// void TerminalWindow::onCardScanned(QString cardId, QString action, int duration){}
// void TerminalWindow::onNetworkStatusChanged(bool connected) {}
// void TerminalWindow::onServerAckReceived(const QString &jsonReply) {}