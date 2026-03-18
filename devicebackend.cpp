#include "devicebackend.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>

DeviceBackend::DeviceBackend(QObject *parent) : QObject(parent){
    // 初始化设备 ID 
    m_deviceId = getCpuId();
    // 传入 this 指针
    m_rfidThread = new RfidThread(this);

    // 连接底层的信号到后端的槽函数
    connect(m_rfidThread, &RfidThread::networkStatusChanged, this, &DeviceBackend::onNetworkStatusChanged);
    connect(m_rfidThread, &RfidThread::serverAckReceived, this, &DeviceBackend::onServerAckReceived);
    // connect(m_rfidThread, &RfidThread::cacheSizeChanged, this, &DeviceBackend::onOfflineCacheUpdated);
    connect(m_rfidThread, &RfidThread::cacheSizeChanged, this, &DeviceBackend::onCacheSizeChanged);
}

// 启动硬件刷卡与网络线程
void DeviceBackend::startHardwareThread(){ m_rfidThread->start();}
// 转发给 QML
void DeviceBackend::onNetworkStatusChanged(bool isOnline){ emit networkChanged(isOnline); }
// 转发给 QML
void DeviceBackend::onCacheSizeChanged(int size){ emit cacheCountChanged(size); }

QString DeviceBackend::getCpuId(){
    QFile file("/proc/cpuinfo");
    if(file.open(QIODevice::ReadOnly | QIODevice::Text)){
        QTextStream in(&file);
        QString l;
        while(in.readLineInto(&l)){
            // 开头字符串对比
            if(l.startsWith("Serial")){
                QStringList parts = l.split(":");
                if(parts.size() > 1){
                    return parts.at(1).trimmed().toUpper();
                }
            }
        }

    }
    return "DEV-offline-mode";
}

void DeviceBackend::onServerAckReceived(const QString &jsonReply){
    // 期望服务器发回来的 JSON {"type":"ack","status":"success","name":"张三","action":"上车签到"}
    // 解析
    QJsonDocument doc = QJsonDocument::fromJson(jsonReply.toUtf8());
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        QString name = obj.value("name").toString();
        QString action = obj.value("action").toString();
        int duration = obj.value("duration").toInt(); // 如果 PC 传了时长就解析
        
        // 发送给 QML 渲染
        emit showCardInfo(name, action, duration);
    }
}

// void DeviceBackend::uploadTheoryResult(const QString &cardId, int score, int total) {
//     // 构造 JSON
//     QJsonObject obj;
//     obj["CardID"] = cardId;
//     obj["Action"] = QString("理论练习");
//     obj["Score"] = score;
//     obj["Total"] = total;
//     obj["device_id"] = m_deviceId;

//     QJsonDocument doc(obj);
//     QString jsonStr = doc.toJson(QJsonDocument::Compact) + "\n";

//     // RfidThread 的 socket 是私有的
//     // 建议：在 RfidThread 中增加一个信号/槽，或者简单的做法是让 DeviceBackend 发射一个信号给 RfidThread
//     // 为了快速实现，我们可以直接通过信号通知 RfidThread 发送
//     emit requestNetworkSend(jsonStr); 
// }