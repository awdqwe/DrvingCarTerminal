#ifndef TERMINALWINDOW_H
#define TERMINALWINDOW_H

#include <QWidget>
#include <QLabel>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
class DeviceBackend;
// 硬件子线程类：专门负责死循环刷卡和网络通信
class RfidThread : public QThread {
    Q_OBJECT
public:
    RfidThread(DeviceBackend *backend = nullptr);
    void run() override; // 线程主函数

signals:
    // 跨线程通信的“信号”
    void cardScanned(QString cardId, QString action, int duration);
    void networkStatusChanged(bool connected);
    void serverAckReceived(const QString &jsonReply);
    void cacheSizeChanged(int size); // 上报离线缓存队列大小

private:
    // backend 指针用于使用获取CPUID函数
    DeviceBackend *m_backend;
};

// // ==========================================
// // 2. 界面主类：负责触摸屏显示
// // ==========================================
// class TerminalWindow : public QWidget {
//     Q_OBJECT
// public:
//     TerminalWindow(QWidget *parent = nullptr);
//     ~TerminalWindow();
    

// private slots:
//     // void onCardScanned(QString cardId, QString action, int duration);
//     // void onNetworkStatusChanged(bool connected);
//     // void updateTime(); // 更新时钟
//     // void onServerAckReceived(const QString &jsonReply); // 处理服务器返回

// private:
//     QLabel *timeLabel;
//     QLabel *statusLabel;
//     QLabel *cardLabel;
//     QLabel *netLabel;
//     RfidThread *workerThread;
//     QLabel *nameLabel;
// };

#endif // TERMINALWINDOW_H