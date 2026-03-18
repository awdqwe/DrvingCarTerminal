import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import DeviceBackend 1.0 
import TheoryModule 1.0

Window {
    visible: true
    width: 1024
    height: 600
    title: qsTr("车载智能终端")
    color: "#0D1117" // 背景 黑
    // 全屏运行
    // visibility: Window.FullScreen 

    // 状态机变量
    property string uiState:"IDLE" // IDLE 空闲  TRAINING(练车中), RESULT(结算中)
    property int sessionSeconds: 0  // 练车秒数
    property string lastDuration: "00:00:00"
    property string currentStudent: "等待刷卡"  // 练车秒数

    // 工具函数 秒数转换
    function formatTime(sec) {
        var h = Math.floor(sec / 3600);
        var m = Math.floor((sec % 3600) / 60);
        var s = sec % 60;
        return (h < 10 ? "0" + h : h) + ":" + (m < 10 ? "0" + m : m) + ":" + (s < 10 ? "0" + s : s);
    }

    DeviceBackend {
        id: backend
        
        // 刚加载时启动底层硬件线程
        Component.onCompleted: backend.startHardwareThread()

        // 接收网络状态改变信号
        onNetworkChanged: {
            networkIndicator.color = isOnline ? "#00E676" : "#FF1744"
            networkText.text = isOnline ? "云端已连接" : "离线模式 (本地缓存已启用)"
        }

        // 接收刷卡结果信号
        onShowCardInfo: {
            currentStudent = name
            
            if (action === "上车签到") {
                uiState = "TRAINING"
                sessionSeconds = 0
                resultTimer.stop()
            } else if (action === "下车签退") {
                uiState = "RESULT"
                lastDuration = formatTime(duration)
                welcomeMsg.text = "练车结束，本次有效练习时长:\n" + lastDuration
                resultTimer.restart() // 10秒后恢复空闲状态
            } else {
                uiState = "ERROR"
                welcomeMsg.text = "非本车学员或未注册卡片！"
                resultTimer.restart()
            }
        }

        // 接收缓存信号
        onCacheCountChanged: {
            cacheLabel.text = "当前离线待补发数据: "  + size + "条"
            // 存在离线信号 红 
            cacheLabel.color = size > 0 ? "#FF1744" : "#FFCA28"
        }
    }

    // 题库管理器
    TheoryManager {
        id: theoryEngine
        onQuizFinished: {
            quizDialog.visible = false
            welcomeMsg.text = "答题结束，得分: " + score
            console.log("最终得分: " + score)
        }
    }

    // 定时器 练习秒表
    Timer {
        id: trainingTimer
        interval: 1000 // 1s
        running: uiState === "TRAINING" // 只在练车状态才运行
        repeat: true
        onTriggered: sessionSeconds++
    }

    // 定时器 系统时钟
    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: clockText.text = Qt.formatDateTime(new Date(), "yyyy-MM-dd  hh:mm:ss")
    }

    // 定时器 结算后恢复空间
    Timer {
        id: resultTimer
        interval: 8000 // 结算画面展示 8 秒
        onTriggered: uiState = "IDLE"
    }
    // -------- UI 布局 -----------------------------------------------------------

    // 1 顶部状态栏
    Rectangle {
        id: topBar
        width: parent.width
        height: 60
        color: "#161B22"
    
        RowLayout {
            anchors.fill: parent
            anchors.margins: 15
        
            Label {
                text: "驾校车载智能终端 | 设备号:" + backend.deviceId
                font.pixelSize: 22
                font.bold: true
                color: "#C9D1D9"
            }

            Item { Layout.fillWidth: true } // 弹簧

            // 右上角时钟
            Label {
                id: clockText
                text: "1000-01-01 12:00:00"
                font.pixelSize: 20
                font.family: "Monospace"
                color: "#8B949E"
            }
        }
    }

    // 2 中央核心显示区
    Item {
        anchors.top: topBar.bottom
        anchors.bottom: bottomBar.top
        width: parent.width

        // 背景外
        Rectangle {
            id: dashboardRing
            width: 400
            height: 400
            radius: 200
            anchors.centerIn: parent
            color: "transparent"
            border.width: uiState === "TRAINING" ? 8 : 4
            border.color: {
                if(uiState==="IDLE") return "#30363D"
                if(uiState==="TRAINING") return "#58A6FF"
                if(uiState==="RESULT") return "#3FB950"
                return '#e8463e'
            }

            // 练车状态的动画
            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: uiState === "TRAINING"
                NumberAnimation {
                    to: 0.4
                    duration:1000
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    to: 1.0
                    duration:1000
                    easing.type: Easing.InOutQuad
                }
            }
        }

        // 中央显示文字
        Column {
            anchors.centerIn: parent
            spacing: 20
            // 学员姓名
            Label {
                text: {
                    if(uiState === "IDLE") return "请刷卡上车"
                    if(uiState === "TRAINING") return "当前学员: " + currentStudent
                    if(uiState === "RESULT") return currentStudent + ",已完成练习"
                    return "非法拦截"
                }
                font.pixelSize: uiState === "IDLE" ? 42:32
                font.bold: true
                color: uiState === "ERROR" ? "#F85149":"#FFFFFF"
                anchors.horizontalCenter: parent.horizontalCenter
            }

            // 显示数字时间 仅练习时显示
            Label {
                text: formatTime(sessionSeconds)
                font.pixelSize: 85
                font.family: "Monospace"
                font.bold: true
                color: "#58A6FF"
                anchors.horizontalCenter: parent.horizontalCenter
                visible: uiState === "TRAINING"
            }

            // 结算/提示信息
            Label {
                id: welcomeMsg
                text: {
                    if(uiState === "IDLE") return "将 IC 卡靠近感应区，支持断网离线打卡"
                    if(uiState === "TRAINING") return "正在记录您的学时..."
                    if(uiState === "RESULT") return "练车结束，本次有效练习时长:\n" + lastDuration
                    return "非法拦截" // 结算时在 backend 逻辑中赋值
                }
                font.pixelSize: uiState === "RESULT" ? 28 : 20
                color: uiState === "RESULT" ? "#3FB950" : "#8B949E"
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    // 3 交互区
    Button {
        anchors.right: parent.right
        anchors.bottom: bottomBar.top
        anchors.margins: 20
        width: 140
        height: 50
        visible: uiState === "TRAINING"
        background: Rectangle { 
            color: "#238636"
            radius: 8 
            border.color: "#2EA043"
        }
        contentItem: Text {
            text: "教练评价"
            color: "#ffffff"
            font.pixelSize: 18
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        onClicked: {
            // TODO 触摸屏互动：这里可以后续增加弹窗
            welcomeMsg.text = "系统提示：教练给您点了一个赞！"
        }
    }

    // 打开练习题按钮（仅练车中可见）
    Button {
        anchors.right: parent.right
        anchors.bottom: bottomBar.top
        anchors.margins: 20
        width: 140
        height: 50
        x: parent.width - 170
        visible: uiState === "TRAINING"
        background: Rectangle { color: "#0366D6"; radius: 8 }
        contentItem: Text { text: "练习题"; color: "#ffffff"; font.pixelSize: 18; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        onClicked: {
            if (theoryEngine.totalQuestions === 0) {
                var ok = theoryEngine.loadQuestions("questions.json")
                if (!ok) {
                    welcomeMsg.text = "无法加载题库，请检查 题库文件"
                    return
                }
            }
            quizDialog.visible = true
        }
    }

    // 4 底部状态栏
    Rectangle {
        id: bottomBar
        width: parent.width
        height: 45
        anchors.bottom: parent.bottom
        color: "#161B22"
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 15
            
            // 网络状态指示灯
            Rectangle { width: 12; height: 12; radius: 6; id: networkIndicator; color: "#FF1744" }
            Label { id: networkText; text: "网络连接中..."; font.pixelSize: 15; color: "#8B949E" }
            
            Item { Layout.fillWidth: true } // 弹簧
            
            Label {
                id: cacheLabel
                text: "当前离线待补发数据: 0 条" 
                font.pixelSize: 15
                color: "#FFCA28"
            }
        }
    }

    // 答题弹窗
    Rectangle {
        id: quizDialog
        width: 760
        height: 460
        color: "#0B1220"
        radius: 10
        border.color: "#30363D"
        border.width: 2
        anchors.centerIn: parent
        visible: false
        z: 999

        Column {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            Row {
                spacing: 10
                Label { text: "练习答题"; font.pixelSize: 22; color: "#FFFFFF" }
                Item { Layout.fillWidth: true }
                Label { text: (theoryEngine.currentIndex < theoryEngine.totalQuestions ? (theoryEngine.currentIndex+1) : theoryEngine.totalQuestions) + "/" + theoryEngine.totalQuestions; color: "#8B949E" }
                Button {
                    text: "关闭"
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle { 
                        color: "#F85149"
                        radius: 4
                    }
                    onClicked: quizDialog.visible = false
                }
            }

            Rectangle { height: 1; color: "#22272B"; width: parent.width }

            // 问题文本
            Label {
                id: qLabel
                text: theoryEngine.currentQuestion
                wrapMode: Text.WordWrap
                font.pixelSize: 20
                color: "#E6EDF3"
                width: parent.width - 40
            }

            // 选项列表
            Column {
                id: optionsBox
                spacing: 10
                width: parent.width - 40

                Repeater {
                    model: theoryEngine.currentOptions
                    delegate: Button {
                        width: parent.width
                        height: 44
                        text: modelData
                        contentItem: Text {
                            text: parent.text
                            color: "#FFFFFF"
                            font.pixelSize: 16
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                        }
                        background: Rectangle { color: "#161B22"; radius: 6; border.color: "#2F363D" }
                        onClicked: {
                            theoryEngine.submitAnswer(index)
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }

            Row {
                spacing: 10
                anchors.horizontalCenter: parent.horizontalCenter
                Button { text: "重置"; onClicked: { theoryEngine.reset(); } }
                Button { text: "提交并退出"; onClicked: { quizDialog.visible = false } }
            }
        }
    }
}
