#ifndef DEVICEBACKEND_H
#define DEVICEBACKEND_H

#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include "terminalwindow.h" 

class DeviceBackend : public QObject{
    Q_OBJECT
    // 只读属性 QML 启动时自动读
    Q_PROPERTY(QString deviceId READ deviceId CONSTANT);
public:
    explicit DeviceBackend(QObject *parent = nullptr);
    Q_INVOKABLE void startHardwareThread();
    // Q_INVOKABLE void uploadTheoryResult(const QString &cardId, int score, int total); // 上传分数

    // 工具函数 读设备 ID
    QString deviceId() const { return m_deviceId; }
    QString getCpuId(); // 内部获取硬件 ID 逻辑

signals:
    // 发送给 QML 界面的信号
    void networkChanged(bool isOnline);
    void showCardInfo(QString name, QString action, int duration);
    void cacheCountChanged(int count);

private slots:
    // 接收来自 RfidThread 的信号
    void onNetworkStatusChanged(bool isOnline);
    void onServerAckReceived(const QString &jsonReply);
    void onCacheSizeChanged(int size);

private:
    RfidThread *m_rfidThread;
    QString m_deviceId;
};

#endif // DEVICEBACKEND_H