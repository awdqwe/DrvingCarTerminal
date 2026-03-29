## 一、工程信息
# 名称: GUI_CLENT_G
# 平台: raspberry QT
# 网络通信: TCP client
# 数据格式: JSON 

## 二、通信消息详情
# 刷卡发送信息
    1、正常刷卡签到
        "{
            \"Action\": \"上车签到\",
            \"CardID\": \"69AB2A07\",
            \"Duration\": 0,
            \"device_id\": \"100000005B105A9F\",
            \"sign\": \"b72a30465e32fe71344505dcb15d51c5\",
            \"timestamp\": \"1773987562\",
            \"type\": \"card\"
        }"

    2、练习过程中周期性发送心跳
        "{
            \"CardID\": \"69AB2A07\",
            \"device_id\": \"100000005B105A9F\",
            \"sign\": \"c453a9d5810302b17007b07e060447c8\",
            \"timestamp\": \"1773987581\",
            \"type\": \"heartbeat\"
        }"

    3、正常刷卡签退
        "{
            \"Action\": \"下车签退\",
            \"CardID\": \"B3D10F07\",
            \"Duration\": 158,
            \"device_id\": \"100000005B105A9F\",
            \"sign\": \"2ca6b3022a27ac4dbd67c668349c8bcf\",
            \"timestamp\": \"1773990677\",
            \"type\": \"card\"
        }"
    3、