// One gamepad control, drawn instead of written: a stick, a d-pad, a trigger
// or a face button. Same flat language as KeyCap (dark face, 2px border, the
// colour of the whole row in `face`), so the pad reads at a glance next to the
// keyboard keys.
import QtQuick

Item {
    id: root
    property string kind: "stick"   // stick | dpad | trigger | face
    property string label: ""       // "L2", "R2", "X"
    property color face: "#f2f4f8"
    property color body: "#14141a"
    property int unit: 26

    implicitWidth: kind === "trigger" ? Math.round(unit * 1.45) : unit
    implicitHeight: unit

    // Stick: the ring with its nub in the middle.
    Rectangle {
        anchors.fill: parent
        visible: root.kind === "stick"
        radius: width / 2
        color: root.body
        border.color: root.face
        border.width: 2
        Rectangle {
            anchors.centerIn: parent
            width: Math.round(parent.width * 0.35)
            height: width
            radius: width / 2
            color: root.face
        }
    }

    // D-pad: a plus, drawn as a face-coloured cross with the body knocked out
    // of it so the arms read as four separate directions.
    Item {
        anchors.fill: parent
        visible: root.kind === "dpad"
        Rectangle {
            anchors.centerIn: parent
            width: Math.round(parent.width * 0.38)
            height: parent.height
            color: root.face
        }
        Rectangle {
            anchors.centerIn: parent
            width: parent.width
            height: Math.round(parent.height * 0.38)
            color: root.face
        }
        Rectangle {
            anchors.centerIn: parent
            width: Math.round(parent.width * 0.38) - 4
            height: parent.height - 4
            color: root.body
        }
        Rectangle {
            anchors.centerIn: parent
            width: parent.width - 4
            height: Math.round(parent.height * 0.38) - 4
            color: root.body
        }
    }

    // Trigger: the shoulder button, told apart by its wider shape and legend.
    Rectangle {
        anchors.fill: parent
        visible: root.kind === "trigger"
        radius: 3
        color: root.body
        border.color: root.face
        border.width: 2
        Text {
            anchors.centerIn: parent
            text: root.label
            font.bold: true
            font.pixelSize: Math.max(8, Math.round(root.unit * 0.4))
            color: root.face
        }
    }

    // Face button: the round one, in the west position (X, Square on a pad).
    Rectangle {
        anchors.fill: parent
        visible: root.kind === "face"
        radius: width / 2
        color: root.body
        border.color: root.face
        border.width: 2
        Text {
            anchors.centerIn: parent
            text: root.label
            font.bold: true
            font.pixelSize: Math.max(9, Math.round(root.unit * 0.46))
            color: root.face
        }
    }
}
