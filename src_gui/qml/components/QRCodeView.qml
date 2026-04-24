import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "transparent"
    
    // Apple-style blurry glass card (fallback to solid #2A2A2E)
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0.15, 0.15, 0.16, 0.85)
        radius: 18
        border.color: Qt.rgba(1, 1, 1, 0.1)
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 30
        spacing: 20

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Pair Device"
            font.pixelSize: 22
            font.weight: Font.DemiBold
            font.family: "Inter, -apple-system, system-ui, sans-serif"
            color: "#ffffff"
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Scan with TitanShare on Android"
            font.pixelSize: 13
            font.family: "Inter, -apple-system, system-ui, sans-serif"
            color: "#98989d"
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                anchors.centerIn: parent
                width: 240
                height: 240
                radius: 12
                color: "#ffffff"
                
                // Pulsing glow behind QR
                Rectangle {
                    id: glowRect
                    anchors.fill: parent
                    radius: parent.radius
                    color: "transparent"
                    border.color: "#0a84ff"
                    border.width: 2
                    opacity: 0.8
                    scale: 1.0
                    
                    SequentialAnimation {
                        running: true
                        loops: Animation.Infinite
                        ParallelAnimation {
                            NumberAnimation { target: glowRect; property: "opacity"; to: 0.2; duration: 1500; easing.type: Easing.InOutQuad }
                            NumberAnimation { target: glowRect; property: "scale"; to: 1.04; duration: 1500; easing.type: Easing.InOutQuad }
                        }
                        ParallelAnimation {
                            NumberAnimation { target: glowRect; property: "opacity"; to: 0.8; duration: 1500; easing.type: Easing.InOutQuad }
                            NumberAnimation { target: glowRect; property: "scale"; to: 1.0; duration: 1500; easing.type: Easing.InOutQuad }
                        }
                    }
                }

                Image {
                    id: qrImage
                    anchors.centerIn: parent
                    anchors.margins: 10
                    width: 220
                    height: 220
                    fillMode: Image.PreserveAspectFit
                    cache: false
                    asynchronous: true
                    source: "file://" + qrImagePath
                    
                    // Smooth fade in on load
                    opacity: 0
                    Behavior on opacity { NumberAnimation { duration: 300 } }
                    onStatusChanged: {
                        if (status === Image.Ready) {
                            opacity = 1.0;
                        } else {
                            opacity = 0.0;
                        }
                    }
                }
            }
        }
        
        // Refresh timer for QR code changes
        Timer {
            interval: 2000
            running: true
            repeat: true
            onTriggered: {
                // Force reload image by attaching a timestamp
                qrImage.source = "file://" + qrImagePath + "?t=" + new Date().getTime();
            }
        }
    }
}
