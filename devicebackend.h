#ifndef DEVICEBACKEND_H
#define DEVICEBACKEND_H

#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include "rfidthread.h" 

class DeviceBackend : public QObject{
    Q_OBJECT
    // 只读属性 QML 启动时自动读
    Q_PROPERTY(QString deviceId READ deviceId CONSTANT);
public:
    explicit DeviceBackend(QObject *parent = nullptr);
    Q_INVOKABLE void startHardwareThread(); // QML 启动线程
    Q_INVOKABLE void setIssueMode(bool enable); // QML 切换到发卡注册界面
    Q_INVOKABLE void setOverlayMode(bool active); // QML 通知覆盖层（选择科目）状态
    Q_INVOKABLE void sendAppointment(QString dateStr); // 预约发送

    Q_INVOKABLE void setCurrentSubject(QString sub) { d_currentSubject = sub; } // QML 切换科目
    Q_INVOKABLE QString currentSubject() { return d_currentSubject; } // QML 获取当前科目
    Q_INVOKABLE void uploadTheoryResult(const QJsonObject &result); // 上传分数
    

    // 工具函数 读设备 ID
    QString deviceId() const { return d_deviceId; }
    QString getCpuId(); // 内部获取硬件 ID 逻辑

signals:
    void networkChanged(bool isOnline);
    /// 已读到卡片并即将发往服务端，供 QML 立即进入「识别中」降低 perceived latency
    void cardProcessingStarted(QString scanAction);
    void showCardInfo(QString cardId, QString name, QString action, int duration);
    void cacheCountChanged(int count);
    void sendToServer(const QString &json);
    void issueModeChanged(bool enabled); // QML 切换到注册界面
    void overlayModeChanged(bool active);
    void invalidCardDetected();          // QML 强制停止计时并弹窗告警

private slots:
    void onNetworkStatusChanged(bool isOnline);
    void onServerAckReceived(const QString &jsonReply);
    void onCacheSizeChanged(int size);
    void onCardScanned(const QString cardId, const QString action, int duration);
    void sendHeartbeat(); // 定时发送心跳包

private:
    RfidThread *m_rfidThread;
    QString d_deviceId;
    QString d_lastCardId;
    QString d_lastScanAction; // 最近一发刷卡动作，用于 invalid 时判断是否回滚底层会话
    QString d_currentStudentName;
    QString d_currentSubject = "";

    // 发卡注册模式
    bool d_issueMode = false;
    bool d_overlayMode = false;

};

#endif // DEVICEBACKEND_H
