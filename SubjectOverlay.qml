// 选择界面
import QtQuick 2.14
import QtQuick.Controls 2.14

Rectangle {
    id: root
    anchors.fill: parent
    color: "#EE0D1117"   // 半透明背景
    visible: false       // 由外部控制
    z: 2000

    signal subjectSelected(string subject)   // 科目选择信号

    Column {
        anchors.centerIn: parent
        spacing: 30
        
        Label {
            text: "请选择本次练习科目"
            font.pixelSize: 32
            font.bold: true
            color: "#FFFFFF"
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Row {
            spacing: 20
            
            Repeater {
                model: ["科目一", "科目二", "科目三", "科目四"]
                delegate: Button {
                    width: 180; height: 100
                    contentItem: Text {
                        text: modelData
                        color: "#FFFFFF"; font.pixelSize: 24; font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.pressed ? "#1F6FEB" : "#238636"
                        radius: 12
                        border.color: "#30363D"
                    }
                    onClicked: {
                        root.subjectSelected(modelData)   // 发射信号
                    }
                }
            }
        }
    }
}