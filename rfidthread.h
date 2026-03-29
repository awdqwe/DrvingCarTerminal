#ifndef RFIDTHREAD_H
#define RFIDTHREAD_H

#include <QThread>
#include <QMutex>
#include <QJsonDocument>
#include <QJsonObject>
#include <vector>

class DeviceBackend;

// 硬件子线程类：专门负责死循环刷卡和网络通信
class RfidThread : public QThread {
    Q_OBJECT
public:
    RfidThread(DeviceBackend *backend = nullptr);
    void run() override; // 线程主函数

signals:
    // 跨线程通信的信号
    void cardScanned(const QString cardId, const  QString action, int duration);
    void networkStatusChanged(bool connected);
    void serverAckReceived(const QString &jsonReply);
    void cacheSizeChanged(int size); // 上报离线缓存队列大小

private:
    // backend 指针用于使用获取CPUID函数
    DeviceBackend *r_backend;

    int r_sock = -1;
    bool r_isConnected = false;
    // 缓存队列
    std::vector<QString> r_offlineCache;

    QMutex r_sessionMutex; // 会话锁
    QString r_currentStudentCard; // 当前学生卡号
    qint64 r_sessionStartTime = 0; // 会话开始时间

public slots:
    void sendJson(const QString &json);
    void invalidatePendingSession(); // 失效当前的会话
};

#endif // RFIDTHREAD_H
