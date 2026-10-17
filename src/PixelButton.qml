// Chunky pixel button for the retro UI. No Material styling.
import QtQuick

Rectangle {
    id: root
    property alias text: label.text
    property color accent: "#ffd60a"
    signal clicked

    color: "#101014"
    border.color: accent
    border.width: 3
    // Hard pixel shadow, no blur.
    Rectangle {
        x: 5; y: 5
        width: parent.width; height: parent.height
        color: "#000000"
        z: -1
    }

    property int pad: 14
    implicitWidth: label.implicitWidth + pad * 2
    implicitHeight: label.implicitHeight + pad

    Text {
        id: label
        anchors.centerIn: parent
        font.bold: true
        font.pixelSize: 17
        font.letterSpacing: 2
        color: accent
        text: "START"
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onPressed: root.y += 2
        onReleased: root.y -= 2
        onClicked: root.clicked()
        onEntered: root.color = "#1c1c22"
        onExited: root.color = "#101014"
    }
}
