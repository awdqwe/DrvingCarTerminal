#include "rfidthread.h"
#include "MFRC522.h"
#include <QDateTime>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include "devicebackend.h"

using namespace std;

RfidThread::RfidThread(DeviceBackend *backend) : r_backend(backend) {}
// 硬件线程 断网重连与离线容灾 (Edge Cache)
void RfidThread::run() {
    MFRC522 mfrc;
    mfrc.PCD_Init();

    const string server_ip = "10.129.101.194"; // 服务器(电脑端) IP
    int port = 8888;

    // while (1) {
    while(!isInterruptionRequested()){
        // 1. 网络保护 断网自动重连机制
        if (!r_isConnected) {
            if (r_sock != -1) { close(r_sock); r_sock = -1; }
            r_sock = socket(AF_INET, SOCK_STREAM, 0);
            
            struct sockaddr_in server;
            server.sin_addr.s_addr = inet_addr(server_ip.c_str());
            server.sin_family = AF_INET;
            server.sin_port = htons(port);

            // 连接 PC 端
            if (::connect(r_sock, (struct sockaddr *)&server, sizeof(server)) == 0) {
                r_isConnected = true;
                emit networkStatusChanged(true); // 通知 UI 变绿灯

                // 断点续传 网络恢复了，检查有没有积压的离线数据
                if (!r_offlineCache.empty()) {
                    for (const QString& cachedJson : r_offlineCache) {
                        QByteArray payload = cachedJson.toUtf8();
                        // 补发数据
                        send(r_sock, payload.constData(), payload.size(), MSG_NOSIGNAL);
                        usleep(100000); // 延时 0.1 秒 防 TCP 粘包
                    }
                    r_offlineCache.clear(); // 补发完毕，清空缓存
                    emit cacheSizeChanged(0); // 清空缓存后通知 UI 变为 0
                }
            } else {
                emit networkStatusChanged(false); // 通知 UI 变红灯
            }
        }

        // 2. 优先处理网络回传 防止刷卡拦截
        if (r_isConnected && r_sock != -1) {
            char recvBuf[256];
            // 保持非阻塞读取
            int bytesRead = recv(r_sock, recvBuf, sizeof(recvBuf) - 1, MSG_DONTWAIT);
            if (bytesRead > 0) {
                recvBuf[bytesRead] = '\0';
                QString serverReply = QString::fromUtf8(recvBuf);
                emit serverAckReceived(serverReply);
            } else if (bytesRead == 0) {
                r_isConnected = false;
            }
        }
        
        // 3. 硬件轮询 (如果没有卡 进入下一次循环)
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

        {
            QMutexLocker locker(&r_sessionMutex);
            if (r_currentStudentCard.isEmpty()) {
                r_currentStudentCard = cardId;
                r_sessionStartTime = QDateTime::currentSecsSinceEpoch();
                actionStr = "上车签到";
            } else if (r_currentStudentCard == cardId) {
                duration = static_cast<int>(QDateTime::currentSecsSinceEpoch() - r_sessionStartTime);
                actionStr = "下车签退";
                r_currentStudentCard.clear();
                r_sessionStartTime = 0;
            } else {
                actionStr = "非法拦截";
            }
        }

        emit cardScanned(cardId, actionStr, duration);

        sleep(2); // 防抖
    }
}

void RfidThread::invalidatePendingSession(){
    QMutexLocker locker(&r_sessionMutex);
    r_currentStudentCard.clear();
    r_sessionStartTime = 0;
}

void RfidThread::sendJson(const QString &json){
    QByteArray payload = json.toUtf8();

    if (r_isConnected && r_sock != -1) {
        ssize_t sentBytes = send(r_sock, payload.constData(), payload.size(), MSG_NOSIGNAL);

        if (sentBytes <= 0) {
            // 发送失败 → 转离线
            r_isConnected = false;
            close(r_sock);
            r_sock = -1;

            emit networkStatusChanged(false);

            r_offlineCache.push_back(json);
            emit cacheSizeChanged(r_offlineCache.size());
        }
    } else {
        // 离线缓存
        r_offlineCache.push_back(json);
        emit cacheSizeChanged(r_offlineCache.size());
    }
}
