#include "devicebackend.h"
#include <QFile>
#include <QTimer>
#include <QTextStream>
#include <QCryptographicHash> // 密码学哈希类

// salt 盐值
static QString secretKey = "HelloWorldDrivingSchool@2026_Pi4B";

// 生成 sign
static QString makeSign(const QString &cardId, 
                        const QString &type, 
                        const QString &timestamp,
                        const QString &subject) {
    QString origin = cardId + type + timestamp + subject + secretKey;
    QByteArray signMd5 = QCryptographicHash::hash(origin.toUtf8(), QCryptographicHash::Md5).toHex();
    return QString(signMd5);
}

DeviceBackend::DeviceBackend(QObject *parent) : QObject(parent){
    d_deviceId = getCpuId(); // 初始化设备 ID 

    // 初始化心跳定时器
    QTimer *heartTimer = new QTimer(this);
    connect(heartTimer, &QTimer::timeout, this, &DeviceBackend::sendHeartbeat);
    // 启动心跳定时器，首次触发前延迟 500ms 以保证系统启动流程优先完成
    heartTimer->start(10000);
    sendHeartbeat(); // 启动时立即发送一次心跳，确保服务端尽快获得设备状态

    // 传入 this 指针
    m_rfidThread = new RfidThread(this);

    // 连接底层的信号到后端的槽
    connect(m_rfidThread, &RfidThread::networkStatusChanged, this, &DeviceBackend::onNetworkStatusChanged);
    connect(m_rfidThread, &RfidThread::serverAckReceived, this, &DeviceBackend::onServerAckReceived);
    connect(m_rfidThread, &RfidThread::cacheSizeChanged, this, &DeviceBackend::onCacheSizeChanged);
    connect(m_rfidThread, &RfidThread::cardScanned, this, &DeviceBackend::onCardScanned);
    connect(this, &DeviceBackend::sendToServer, m_rfidThread, &RfidThread::sendJson, Qt::QueuedConnection);
    // 将发卡/overlay 模式通知底层线程，使用队列连接保证线程安全
    connect(this, &DeviceBackend::issueModeChanged, m_rfidThread, &RfidThread::setIssueMode, Qt::QueuedConnection);
    connect(this, &DeviceBackend::overlayModeChanged, m_rfidThread, &RfidThread::setOverlayMode, Qt::QueuedConnection);
}

// 启动硬件刷卡与网络线程
void DeviceBackend::startHardwareThread(){ m_rfidThread->start();}
// 转发给 QML
void DeviceBackend::onNetworkStatusChanged(bool isOnline){ emit networkChanged(isOnline); }
void DeviceBackend::onCacheSizeChanged(int size){ emit cacheCountChanged(size); }
// 发卡注册
void DeviceBackend::setIssueMode(bool enable){
    d_issueMode = enable;
    emit issueModeChanged(enable);
}

// 覆盖层（选择科目）状态通知
void DeviceBackend::setOverlayMode(bool active){
    d_overlayMode = active;
    emit overlayModeChanged(active);
}

// 获取CPUID
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

// 发送打卡消息
void DeviceBackend::onCardScanned(const QString cardId, const QString action, int duration){
    emit cardProcessingStarted(action); // 发信号给 QML 进入「识别中」

    // 发卡注册模式不视为打卡 仅发送卡片信息
    if (d_issueMode) {
        d_lastCardId = ""; // 清空卡号缓存
        d_lastScanAction.clear(); // 清空最近一发刷卡动作
        QJsonObject obj;
        obj["type"] = "issue_card";
        obj["CardID"] = cardId;
        obj["device_id"] = d_deviceId;

        QString json = QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";

        emit sendToServer(json);

        return; // 终止发送打卡信息
    }

    d_lastScanAction = action; // 最近一发刷卡动作，用于 invalid 时判断是否回滚底层会话

    if(action == "下车签退") d_lastCardId = ""; // 清空缓存
     else d_lastCardId = cardId; // 缓存
    
    // 安全机制
    // 1)获取当前时间戳
    qint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();
    QString timestampStr = QString::number(currentTimestamp);

    // 2)生成原始签名 = cardId + Type + Timestamp + salt
    QString signOrigin = cardId + "card" + QString::number(currentTimestamp) + secretKey;

    // 3)加密签名
    QString sign = makeSign(cardId, "card", timestampStr, d_currentSubject);

    QJsonObject obj;
    obj["type"] = "card";
    obj["CardID"] = cardId;
    obj["Action"] = action;
    obj["Duration"] = duration;
    obj["Subject"] = d_currentSubject;
    obj["device_id"] = d_deviceId;
    obj["timestamp"] = timestampStr;
    obj["sign"] = sign;

    QJsonDocument doc(obj);

    // 最终 JSON
    QString json = doc.toJson(QJsonDocument::Compact) + "\n";
    emit sendToServer(json);
}

// 发送心跳
void DeviceBackend::sendHeartbeat(){
    if(!d_lastCardId.isEmpty() && !d_issueMode) {
        if (d_issueMode) return;

        qint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();
        QString timestampStr = QString::number(currentTimestamp);
        QString cardId = d_lastCardId.isEmpty() ? "" : d_lastCardId;
        QString sign = makeSign(cardId, "heartbeat", timestampStr, d_currentSubject);
        
        QJsonObject hb;
        hb["type"] = "heartbeat";
        hb["CardID"] = cardId;
        hb["device_id"] = d_deviceId;
        hb["timestamp"] = timestampStr;
        hb["sign"] = sign; 

        emit sendToServer(QJsonDocument(hb).toJson(QJsonDocument::Compact) + "\n");
    }
}

// 解析服务端回应包(得到卡的基本信息并显示到UI)
void DeviceBackend::onServerAckReceived(const QString &jsonReply){
    QJsonDocument doc = QJsonDocument::fromJson(jsonReply.toUtf8());
    if (doc.isObject()) {
        QJsonObject ackObj = doc.object();
        QString type = ackObj.value("type").toString();
        QString status = ackObj.value("status").toString();

        if (status == "success") {
            // 收到回应
            QString cardId = ackObj.value("CardID").toString();
            QString name = ackObj.value("name").toString();
            QString action = ackObj.value("action").toString();
            int duration = ackObj.value("duration").toInt();

            emit showCardInfo(cardId, name, action, duration);
            d_currentStudentName = name;

        } else if (status == "invalid") {
            // 收到身份校验失败回应
            d_lastCardId = ""; // 清空缓存
            // 仅「上车签到」被服务端拒绝时回滚 RFID 层占用的卡位，否则未注册卡会阻塞合法学员刷卡
            if (d_lastScanAction == QStringLiteral("上车签到"))
                m_rfidThread->invalidatePendingSession();
            emit showCardInfo("", "未知访客", "illegal", 0);

        } else if (status == "theory_ok") {
            // 收到理论成绩被记录的回应
            QString cardId = ackObj.value("CardID").toString();
            QString name = d_currentStudentName; // 从刷卡签到的回应包中获得姓名
            // 发信号给 QML 显示理论成绩成功
            emit showCardInfo(cardId, name, "theory_ok", 0); // 理论练习结束 与时长无关

        } else if (type == "issue_reply") {
            // 收到发卡模式下的回应
            QString cardId = ackObj.value("CardID").toString();

            if (status == "new") {
                emit showCardInfo(cardId, "新卡待注册", "issue", 0);
            } else if (status == "exists") {
                QString name = ackObj.value("name").toString();
                emit showCardInfo(cardId, name, "已存在", 0);
            }
        } else if (type == "control") {
            // 收到开启发卡模式消息
            QString cmd = ackObj.value("cmd").toString();

            if (cmd == "enter_issue_mode") {
                setIssueMode(true);
                emit showCardInfo("", "发卡中心", "enter_reg", 0);
            } else if (cmd == "exit_issue_mode") {
                setIssueMode(false);
                emit showCardInfo("", "", "exit_reg", 0);
            }
        }
    }
}

// 发送理论成绩消息
void DeviceBackend::uploadTheoryResult(const QJsonObject &result){
    QJsonObject obj = result;
    qint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();
    QString timestampStr = QString::number(currentTimestamp);

    if (!obj.contains("CardID")) {
        obj["CardID"] = d_lastCardId; // 在刷卡时保存
    }
    if (!obj.contains("type")) {
        obj["type"] = "theory";
    }
    QString sign = makeSign(obj["CardID"].toString(), obj["type"].toString(), timestampStr, d_currentSubject);

    obj["device_id"] = d_deviceId;
    obj["timestamp"] = timestampStr;
    obj["sign"] = sign;
    
    QJsonDocument doc(obj);

    // 最终 JSON
    QString json = doc.toJson(QJsonDocument::Compact) + "\n";
    emit sendToServer(json);
}

// 发送预约消息
void DeviceBackend::sendAppointment(QString dateStr) {
    qint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();
    QString timestampStr = QString::number(currentTimestamp);
    QString sign = makeSign(d_lastCardId, "appointment", timestampStr, d_currentSubject);

    QJsonObject obj;
    obj["type"] = "appointment";
    obj["CardID"] = d_lastCardId;
    obj["Subject"] = d_currentSubject;
    obj["Date"] = dateStr;
    obj["device_id"] = d_deviceId;
    obj["timestamp"] = timestampStr;
    obj["sign"] = sign;

    emit sendToServer(QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n");
}