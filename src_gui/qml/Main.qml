import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Effects
import "components"

ApplicationWindow {
    id: window
    width: 560
    height: 560
    visible: true
    title: "TitanShare"

    // Frameless, transparent — glass effect is all in QML
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"

    // ─── Background wallpaper blur simulation ──────────────────────────
    //     (deep gradient since we can't blur the actual desktop on all compositors)
    Rectangle {
        id: bgGradient
        anchors.fill: parent
        anchors.margins: 40
        radius: 24
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: Qt.rgba(0.04, 0.06, 0.14, 0.96) }
            GradientStop { position: 1.0; color: Qt.rgba(0.02, 0.04, 0.10, 0.98) }
        }
        border.color: Qt.rgba(1, 1, 1, 0.10)
        border.width: 1
    }

    // ─── Ambient glow orbs ─────────────────────────────────────────────
    Rectangle {
        anchors.centerIn: bgGradient
        anchors.horizontalCenterOffset: -80
        anchors.verticalCenterOffset: -60
        width: 260; height: 260; radius: 130
        color: Qt.rgba(0.04, 0.36, 0.88, 0.07)
        visible: true
        z: -1
    }
    Rectangle {
        anchors.centerIn: bgGradient
        anchors.horizontalCenterOffset: 90
        anchors.verticalCenterOffset: 80
        width: 200; height: 200; radius: 100
        color: Qt.rgba(0.55, 0.18, 0.95, 0.05)
        visible: true
        z: -1
    }

    // ─── Window dragging ───────────────────────────────────────────────
    MouseArea {
        id: dragArea
        anchors.fill: bgGradient
        property point startPos: Qt.point(0, 0)
        onPressed: (mouse) => { startPos = Qt.point(mouse.x, mouse.y) }
        onPositionChanged: (mouse) => {
            let delta = Qt.point(mouse.x - startPos.x, mouse.y - startPos.y)
            window.x += delta.x
            window.y += delta.y
        }
    }

    // ─── Traffic light controls ────────────────────────────────────────
    Item {
        id: trafficLights
        anchors.top: bgGradient.top
        anchors.left: bgGradient.left
        anchors.margins: 18
        width: 54; height: 14
        z: 10

        property bool isHovered: trafficHover.containsMouse
        MouseArea { id: trafficHover; anchors.fill: parent; anchors.margins: -10; hoverEnabled: true }

        Row {
            spacing: 8
            Rectangle {
                width: 14; height: 14; radius: 7
                color: trafficHover.pressed ? "#bf4c45" : "#FF5F56"
                border.color: Qt.rgba(0,0,0,0.1); border.width: 1
                Text { anchors.centerIn: parent; text: "✕"; color: Qt.rgba(0,0,0,0.6)
                       font.pixelSize: 9; font.bold: true; visible: trafficLights.isHovered }
                MouseArea { anchors.fill: parent; onClicked: Qt.quit() }
            }
            Rectangle {
                width: 14; height: 14; radius: 7
                color: trafficHover.pressed ? "#bf8e22" : "#FFBD2E"
                border.color: Qt.rgba(0,0,0,0.1); border.width: 1
                Text { anchors.centerIn: parent; text: "─"; color: Qt.rgba(0,0,0,0.6)
                       font.pixelSize: 10; font.bold: true; visible: trafficLights.isHovered }
                MouseArea { anchors.fill: parent; onClicked: window.showMinimized() }
            }
            Rectangle {
                width: 14; height: 14; radius: 7; color: "#27C93F"
                border.color: Qt.rgba(0,0,0,0.1); border.width: 1
                Text { anchors.centerIn: parent; text: "⤢"; color: Qt.rgba(0,0,0,0.6)
                       font.pixelSize: 9; visible: trafficLights.isHovered }
            }
        }
    }

    // ─── Main Pairing Panel ────────────────────────────────────────────
    Item {
        anchors.fill: bgGradient
        anchors.topMargin: 44

        PairingView {
            anchors.centerIn: parent
            width: parent.width - 48
            height: parent.height - 16
        }
    }

    // ─── Drop shadow ───────────────────────────────────────────────────
    MultiEffect {
        source: bgGradient
        anchors.fill: bgGradient
        shadowEnabled: true
        shadowColor: Qt.rgba(0, 0, 0, 0.55)
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 16
        shadowBlur: 1.0
    }
}
