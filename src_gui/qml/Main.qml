import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "components"

ApplicationWindow {
    id: window
    width: 480
    height: 620
    minimumWidth: 480
    minimumHeight: 620
    visible: true
    title: "TitanShare"
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"

    // ── Root container ────────────────────────────────────────────────────
    Rectangle {
        id: root
        anchors.fill: parent
        radius: 20
        clip: true

        // Rich deep-navy gradient background
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: "#0D1728" }
            GradientStop { position: 0.5; color: "#0A1220" }
            GradientStop { position: 1.0; color: "#060C18" }
        }

        // Subtle top edge highlight (glass rim)
        Rectangle {
            z: 10
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 60
            height: 1
            color: "#33FFFFFF"
            radius: 1
        }

        // ── Window border ─────────────────────────────────────────────────
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.color: "#1AFFFFFF"
            border.width: 1
            z: 20
        }

        // ── Ambient glow blobs ────────────────────────────────────────────
        Rectangle {
            x: -40; y: 40
            width: 280; height: 280; radius: 140
            color: "#120A84FF"
            antialiasing: true
        }
        Rectangle {
            x: parent.width - 160; y: parent.height - 200
            width: 220; height: 220; radius: 110
            color: "#0DBF5AF2"
            antialiasing: true
        }

        // ── Drag area (behind everything) ─────────────────────────────────
        MouseArea {
            anchors.fill: parent
            property point startPos
            onPressed:           startPos = Qt.point(mouseX, mouseY)
            onPositionChanged:   {
                window.x += mouseX - startPos.x
                window.y += mouseY - startPos.y
            }
        }

        // ── Traffic lights ────────────────────────────────────────────────
        Item {
            x: 20; y: 20
            width: 54; height: 16
            z: 50

            Rectangle {
                id: btnClose
                x: 0; y: 1; width: 13; height: 13; radius: 7
                color: "#FF5F56"
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: Qt.quit()
                }
            }
            Rectangle {
                id: btnMin
                x: 21; y: 1; width: 13; height: 13; radius: 7
                color: "#FFBD2E"
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: window.showMinimized()
                }
            }
            Rectangle {
                x: 42; y: 1; width: 13; height: 13; radius: 7
                color: "#27C93F"
            }
        }

        // ── Content ───────────────────────────────────────────────────────
        PairingView {
            anchors.top: parent.top
            anchors.topMargin: 52
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 0
        }
    }
}
