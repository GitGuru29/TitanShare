import QtQuick
import QtQuick.Layouts

Item {
    id: root

    // ── Properties injected from C++ context ──────────────────────────────
    property string pinCode:    typeof AppModel !== "undefined" ? AppModel.pinCode    : "------"
    property string deviceName: typeof AppModel !== "undefined" ? AppModel.deviceName : "Linux PC"

    // ── Top: Discover animation + branding ────────────────────────────────
    Column {
        id: topSection
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 0

        // ── Pulsing radar icon ─────────────────────────────────────────────
        Item {
            width: 100; height: 100
            anchors.horizontalCenter: parent.horizontalCenter

            // Rings — 3 expanding pulses
            Repeater {
                model: 3
                delegate: Rectangle {
                    property int idx: index
                    anchors.centerIn: parent
                    width: 40 + idx * 24
                    height: width
                    radius: width / 2
                    color: "transparent"
                    border.color: "#0A84FF"
                    border.width: 1.5
                    opacity: 0

                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        PauseAnimation  { duration: idx * 500 }
                        NumberAnimation { to: 0.7; duration: 400; easing.type: Easing.OutCubic }
                        NumberAnimation { to: 0.0; duration: 900; easing.type: Easing.InCubic }
                        PauseAnimation  { duration: 200 }
                    }

                    SequentialAnimation on scale {
                        loops: Animation.Infinite
                        PauseAnimation  { duration: idx * 500 }
                        NumberAnimation { from: 0.85; to: 1.4; duration: 1300; easing.type: Easing.OutCubic }
                    }
                }
            }

            // Centre icon circle
            Rectangle {
                anchors.centerIn: parent
                width: 52; height: 52; radius: 26
                color: "#1A0A84FF"
                border.color: "#660A84FF"
                border.width: 1.5

                Text {
                    anchors.centerIn: parent
                    text: "⌁"
                    font.pixelSize: 26
                    color: "#0A84FF"
                }
            }
        }

        Item { width: 1; height: 16 }

        // ── App name ───────────────────────────────────────────────────────
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "TitanShare"
            color: "#FFFFFF"
            font.pixelSize: 26
            font.weight: 700
            font.letterSpacing: -0.3
        }

        Item { width: 1; height: 6 }

        // ── Status badge ───────────────────────────────────────────────────
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: statusRow.implicitWidth + 20
            height: 24; radius: 12
            color: "#1430D158"
            border.color: "#3330D158"
            border.width: 1

            Row {
                id: statusRow
                anchors.centerIn: parent
                spacing: 6

                Rectangle {
                    width: 7; height: 7; radius: 4
                    color: "#30D158"
                    anchors.verticalCenter: parent.verticalCenter

                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 900 }
                        NumberAnimation { to: 1.0; duration: 900 }
                    }
                }

                Text {
                    text: "Discoverable on LAN"
                    color: "#30D158"
                    font.pixelSize: 12
                    font.weight: 500
                }
            }
        }
    }

    // ── Divider ───────────────────────────────────────────────────────────
    Rectangle {
        anchors.top: topSection.bottom
        anchors.topMargin: 28
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 80
        height: 1
        color: "#14FFFFFF"
    }

    // ── Middle: device + PIN ──────────────────────────────────────────────
    Column {
        id: midSection
        anchors.top: topSection.bottom
        anchors.topMargin: 46
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 64
        spacing: 0

        // ── "This device" section ──────────────────────────────────────────
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "THIS DEVICE"
            color: "#4DFFFFFF"
            font.pixelSize: 10
            font.weight: 600
            font.letterSpacing: 2.0
        }

        Item { width: 1; height: 8 }

        // Hostname card
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            height: 48; radius: 12
            color: "#0DFFFFFF"
            border.color: "#14FFFFFF"
            border.width: 1

            Row {
                anchors.centerIn: parent
                spacing: 10

                Text {
                    text: "💻"
                    font.pixelSize: 18
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: root.deviceName
                    color: "#FFFFFF"
                    font.pixelSize: 16
                    font.weight: 600
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Item { width: 1; height: 28 }

        // ── PIN Section ────────────────────────────────────────────────────
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "PAIRING CODE"
            color: "#800A84FF"
            font.pixelSize: 10
            font.weight: 600
            font.letterSpacing: 2.5
        }

        Item { width: 1; height: 14 }

        // Instructions
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Enter this code in the TitanShare app on your phone"
            color: "#66FFFFFF"
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            width: parent.width
        }

        Item { width: 1; height: 20 }

        // ── PIN digit row ──────────────────────────────────────────────────
        Row {
            id: pinRow
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10

            // Derive safe 6-char string from pinCode
            property string digits: {
                var p = root.pinCode
                if (!p || p.length === 0) p = "------"
                while (p.length < 6) p += "-"
                return p.substring(0, 6)
            }

            Repeater {
                model: 6

                delegate: Rectangle {
                    width: 52; height: 68; radius: 12
                    color: "#1A0A84FF"
                    border.color: "#990A84FF"
                    border.width: 1.5

                    // Blue inner glow at top
                    Rectangle {
                        anchors.top: parent.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 32; height: 2; radius: 1
                        color: "#660A84FF"
                    }

                    Text {
                        anchors.centerIn: parent
                        text: pinRow.digits.charAt(index)
                        color: "#FFFFFF"
                        font.pixelSize: 30
                        font.weight: 700
                        font.family: "SF Mono, Fira Code, JetBrains Mono, Consolas, monospace"
                    }
                }
            }
        }

        Item { width: 1; height: 10 }

        // Refresh hint
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "↺  Auto-refreshes every 5 minutes"
            color: "#33FFFFFF"
            font.pixelSize: 11
        }
    }

    // ── Bottom: animated waiting indicator ────────────────────────────────
    Column {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 28
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 10

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Open TitanShare on your Android — it will find this PC automatically"
            color: "#40FFFFFF"
            font.pixelSize: 11
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            width: root.width - 80
        }

        // Animated dots
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8

            Repeater {
                model: 3
                delegate: Rectangle {
                    width: 6; height: 6; radius: 3
                    color: "#0A84FF"
                    opacity: 0.25

                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        PauseAnimation  { duration: index * 250 }
                        NumberAnimation { to: 1.0;  duration: 350; easing.type: Easing.OutSine }
                        NumberAnimation { to: 0.25; duration: 450; easing.type: Easing.InSine }
                    }
                }
            }
        }
    }
}
