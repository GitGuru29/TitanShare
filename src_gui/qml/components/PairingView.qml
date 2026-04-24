import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: root
    color: "transparent"

    // ─── Glass Card ────────────────────────────────────────────────────
    Rectangle {
        id: glassCard
        anchors.fill: parent
        radius: 22

        // Deep navy glass base
        color: Qt.rgba(0.04, 0.07, 0.14, 0.82)
        border.color: Qt.rgba(1, 1, 1, 0.12)
        border.width: 1

        // Subtle top-edge highlight (like real glass)
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            radius: parent.radius
            color: Qt.rgba(1, 1, 1, 0.22)
        }
    }

    // ─── Content ───────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 0

        // ── Wi-Fi Discovery Icon ──────────────────────────────────────
        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 4
            width: 52
            height: 52

            // Pulsing outer rings (AirDrop-like)
            Repeater {
                model: 3
                delegate: Rectangle {
                    property int ringIdx: index
                    anchors.centerIn: parent
                    width: 28 + ringIdx * 18
                    height: width
                    radius: width / 2
                    color: "transparent"
                    border.color: Qt.rgba(0.04, 0.52, 1.0, 0.35 - ringIdx * 0.08)
                    border.width: 1.5

                    opacity: 0
                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        PauseAnimation { duration: ringIdx * 350 }
                        NumberAnimation { to: 1.0; duration: 500; easing.type: Easing.OutQuad }
                        NumberAnimation { to: 0.0; duration: 900; easing.type: Easing.InQuad }
                        PauseAnimation { duration: 200 }
                    }
                }
            }

            // Centre icon (phone + WiFi symbol via text)
            Text {
                anchors.centerIn: parent
                text: "📱"
                font.pixelSize: 24
            }
        }

        Item { Layout.preferredHeight: 18 }

        // ── Title ──────────────────────────────────────────────────────
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "TitanShare"
            font.pixelSize: 20
            font.weight: Font.Bold
            font.letterSpacing: 0.5
            color: "#ffffff"
        }

        Item { Layout.preferredHeight: 5 }

        // ── Subtitle ───────────────────────────────────────────────────
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Looking for devices nearby…"
            font.pixelSize: 12
            color: Qt.rgba(1, 1, 1, 0.5)
        }

        Item { Layout.preferredHeight: 28 }

        // ── Divider ────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Qt.rgba(1, 1, 1, 0.08)
        }

        Item { Layout.preferredHeight: 26 }

        // ── Device Name ────────────────────────────────────────────────
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "This device"
            font.pixelSize: 11
            color: Qt.rgba(1, 1, 1, 0.38)
            font.letterSpacing: 0.8
        }

        Item { Layout.preferredHeight: 6 }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: typeof deviceName !== "undefined" ? deviceName : "Linux PC"
            font.pixelSize: 15
            font.weight: Font.SemiBold
            color: "#ffffff"
        }

        Item { Layout.preferredHeight: 24 }

        // ── PIN Area ───────────────────────────────────────────────────
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "PAIRING CODE"
            font.pixelSize: 10
            font.letterSpacing: 2.5
            color: Qt.rgba(0.04, 0.52, 1.0, 0.85)
            font.weight: Font.Medium
        }

        Item { Layout.preferredHeight: 14 }

        // PIN digits — displayed as spaced individual blocks
        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8

            Repeater {
                id: pinDigits
                // Split pinCode into individual characters; pad/trim to 6
                property string safePin: {
                    var p = (typeof pinCode !== "undefined") ? pinCode : "------"
                    while (p.length < 6) p = p + "-"
                    return p.substring(0, 6)
                }
                model: 6

                delegate: Rectangle {
                    width: 40
                    height: 52
                    radius: 10
                    color: Qt.rgba(0.04, 0.52, 1.0, 0.12)
                    border.color: Qt.rgba(0.04, 0.52, 1.0, 0.45)
                    border.width: 1.5

                    // Shimmer effect when pin updates
                    SequentialAnimation on color {
                        id: digitAnim
                        running: false
                        ColorAnimation { to: Qt.rgba(0.04, 0.52, 1.0, 0.28); duration: 150 }
                        ColorAnimation { to: Qt.rgba(0.04, 0.52, 1.0, 0.12); duration: 400 }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: pinDigits.safePin.charAt(index)
                        font.pixelSize: 24
                        font.weight: Font.Bold
                        font.family: "SF Mono, Fira Code, Consolas, monospace"
                        color: "#ffffff"
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 20 }

        // ── Footer instruction ─────────────────────────────────────────
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            text: "Open TitanShare on your Android device.\nIt will find this computer automatically."
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            font.pixelSize: 11
            color: Qt.rgba(1, 1, 1, 0.38)
            lineHeight: 1.5
        }

        // ── Animated waiting dots ──────────────────────────────────────
        Item { Layout.preferredHeight: 16 }

        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8

            Repeater {
                model: 3
                delegate: Rectangle {
                    width: 6; height: 6; radius: 3
                    color: Qt.rgba(0.04, 0.52, 1.0, 0.9)
                    opacity: 0.25

                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        PauseAnimation { duration: index * 280 }
                        NumberAnimation { to: 1.0; duration: 380; easing.type: Easing.OutSine }
                        NumberAnimation { to: 0.25; duration: 480; easing.type: Easing.InSine }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }

    // ─── Watch for PIN refresh (re-trigger shimmer) ────────────────────
    property string _lastPin: ""
    onVisibleChanged: _lastPin = ""

    Timer {
        interval: 500
        running: true
        repeat: true
        onTriggered: {
            var cur = (typeof pinCode !== "undefined") ? pinCode : ""
            if (cur !== root._lastPin && cur !== "") {
                root._lastPin = cur
                // Flash all digit boxes
                for (var i = 0; i < pinDigits.count; i++) {
                    var item = pinDigits.itemAt(i)
                    if (item) {
                        var anim = item.children[0]  // digitAnim
                        if (anim && typeof anim.start === "function") anim.start()
                    }
                }
            }
        }
    }
}
