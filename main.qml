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

    // 状态机变量  // IDLE(空闲), TRAINING(练车中), RESULT(结算中), WARNING(临时警告)
    property string uiState:"IDLE"
    property int sessionSeconds: 0  // 练车秒数
    property string lastDuration: "00:00:00"
    property string currentStudent: "等待刷卡"  // 练车秒数
    // 练车中刷错卡显示 illegal 后，警告结束需回到 TRAINING，勿与空闲「请刷卡上车」混淆
    property bool resumeTrainingAfterWarning: false
    // 进入「识别中」前是否在练车（含连续刷卡时仍处于 RECOGNIZING 的情况）
    property bool pendingResumeTraining: false
    property int recognizingStartTime: 0
    property bool _hasPendingCardAck: false
    property string _pendCardId: ""
    property string _pendName: ""
    property string _pendAction: ""
    property int _pendDuration: 0

    readonly property int recognizingMinMs: 900 // 约 1 秒内展示「识别中区」以降低卡顿感

    // 工具函数 秒数转换
    function formatTime(sec) {
        var h = Math.floor(sec / 3600);
        var m = Math.floor((sec % 3600) / 60);
        var s = sec % 60;
        return (h < 10 ? "0" + h : h) + ":" + (m < 10 ? "0" + m : m) + ":" + (s < 10 ? "0" + s : s);
    }

    function applyShowCardInfo(cardId, name, action, duration) {
        recognizingFlushTimer.stop()
        _hasPendingCardAck = false

        var wasTrainingBeforeScan = pendingResumeTraining
        pendingResumeTraining = false

        if (action === "上车签到") {
            currentStudent = name
            uiState = "TRAINING"
            theoryEngine.setCurrentCard(cardId) // 绑定学员卡
            sessionSeconds = 0
        } else if (action === "下车签退") {
            currentStudent = name
            uiState = "RESULT"
            quizDialog.visible = false // 强制关闭答题
            lastDuration = formatTime(duration)
            welcomeMsg.text = "练车结束，本次有效练习时长:\n" + lastDuration
            resultTimer.restart()
        } else if (action === "enter_reg") {
            uiState = "REGISTRATION" // 进入注册模式
            currentStudent = "发卡中心"
        } else if (action === "exit_reg") {
            uiState = "IDLE"
            currentStudent = "等待刷卡"
        } else if (action === "issue") {
            // 发卡模式下刷了新卡
            uiState = "REGISTRATION"
            currentStudent = "检测到新卡"
            welcomeMsg.text = "卡号: " + cardId + "\n已发送卡号到管理端"
        } else if (action === "theory_ok") {
            uiState = "TRAINING"
            welcomeMsg.text = "理论成绩已提交成功！"
        } else if (action === "illegal") {
            resumeTrainingAfterWarning = wasTrainingBeforeScan
            if (!resumeTrainingAfterWarning)
                sessionSeconds = 0
            uiState = "WARNING"
            if (!resumeTrainingAfterWarning)
                currentStudent = "非法卡片" // 练车中刷错卡时保留原学员姓名
            welcomeMsg.text = "非学员卡或未注册卡片！"
            warningTimer.restart() // 启动UI回弹定时器
        }
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

        onCardProcessingStarted: {
            recognizingFlushTimer.stop()
            _hasPendingCardAck = false
            pendingResumeTraining = pendingResumeTraining || (uiState === "TRAINING")
            recognizingStartTime = Date.now()
            uiState = "RECOGNIZING"
        }

        // 接收刷卡结果信号（可能与 RECOGNIZING 错开，做最短展示时间）
        onShowCardInfo: function(cardId, name, action, duration) {
            if (uiState !== "RECOGNIZING") {
                applyShowCardInfo(cardId, name, action, duration)
                return
            }
            var elapsed = Date.now() - recognizingStartTime
            if (elapsed >= recognizingMinMs) {
                applyShowCardInfo(cardId, name, action, duration)
            } else {
                _hasPendingCardAck = true
                _pendCardId = cardId
                _pendName = name
                _pendAction = action
                _pendDuration = duration
                recognizingFlushTimer.interval = Math.max(1, recognizingMinMs - elapsed)
                recognizingFlushTimer.restart()
            }
        }

        // 接收缓存信号
        onCacheCountChanged: function(count) {
            cacheLabel.text = "当前离线待补发数据: "  + count + "条"
            // 存在离线信号 红 
            cacheLabel.color = count > 0 ? "#FF1744" : "#FFCA28"
        }
    }

    // 题库管理器
    TheoryManager {
        id: theoryEngine
        onQuizFinished: {
            backend.uploadTheoryResult(theoryEngine.getResult())
            quizDialog.visible = false
            welcomeMsg.text = "答题结束，得分: " + score
            uiState = "TRAINING"
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
    // 定时器 警告回弹
    Timer {
        id: warningTimer
        interval: 5000 // 警告显示 5 秒
        onTriggered: {
            if (resumeTrainingAfterWarning) {
                uiState = "TRAINING"
                welcomeMsg.text = "正在记录您的学时..." // illegal 分支曾直接赋值，需恢复练车提示
                resumeTrainingAfterWarning = false
            } else {
                uiState = "IDLE"
            }
        }
    }

    // 识别中区：服务端已回包但未满最短展示时间时延后应用结果
    Timer {
        id: recognizingFlushTimer
        repeat: false
        onTriggered: {
            if (_hasPendingCardAck)
                applyShowCardInfo(_pendCardId, _pendName, _pendAction, _pendDuration)
        }
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
                text: "1970-01-01 00:00:00"
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
            border.width: (uiState === "TRAINING" || uiState === "RECOGNIZING") ? 8 : 4
            border.color: {
                if(uiState==="IDLE") return "#30363D"
                if(uiState==="TRAINING") return "#58A6FF"
                if(uiState==="RECOGNIZING") return "#D29922"
                if(uiState==="RESULT") return "#3FB950"
                if(uiState==="REGISTRATION") return "#BF40BF"
                return '#e8463e'
            }

            // 练车状态的动画
            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: uiState === "TRAINING" || uiState === "RECOGNIZING"
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
                    if(uiState === "RECOGNIZING") return "识别中…"
                    if(uiState === "TRAINING") return "当前学员: " + currentStudent
                    if(uiState === "RESULT") return currentStudent + ",已完成练习"
                    if(uiState === "WARNING") return "身份验证失败"
                    if(uiState === "REGISTRATION") return "模式：系统注册发卡"
                    return "等待中"
                }
                font.pixelSize: (uiState === "IDLE" || uiState === "RECOGNIZING") ? 42 : 32
                font.bold: true
                color: {
                    if (uiState === "WARNING") return "#F85149"
                    if (uiState === "RECOGNIZING") return "#D29922"
                    return "#FFFFFF"
                }
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Item {
                width: parent.width
                height: uiState === "RECOGNIZING" ? 56 : 0
                BusyIndicator {
                    anchors.centerIn: parent
                    width: 56
                    height: 56
                    running: uiState === "RECOGNIZING"
                }
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
                    if(uiState === "RECOGNIZING") return "已读取卡片，正在与服务器验证…"
                    if(uiState === "TRAINING") return "正在记录您的学时..."
                    if(uiState === "RESULT") return "练车结束，本次有效练习时长:\n" + lastDuration
                    if(uiState === "WARNING") return "非法拦截：非本车学员或未注册卡片！"
                    return "未知状态"
                }
                font.pixelSize: uiState === "RESULT" ? 28 : 20
                color: uiState === "RESULT" ? "#3FB950" : "#8B949E"
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }
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
            theoryEngine.reset()
            var ok = theoryEngine.loadQuestions("questions.json")
            if (theoryEngine.totalQuestions === 0) {
                if (!ok) {
                    welcomeMsg.text = "无法加载题库，请检查题库文件"
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
                Button {
                    text: "提交并退出"
                    onClicked: {
                        var result = theoryEngine.getResult()
                        backend.uploadTheoryResult(result)
                        quizDialog.visible = false
                    }
                }
            }
        }
    }
}
