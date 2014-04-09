import QtQuick 2.2

Rectangle {
	objectName: 'foo'
	width: 200
	height: 200
	color: "red"

	Rectangle {
		objectName: 'bar'
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.verticalCenter: parent.verticalCenter
		width: 100
		height: 100
		color: "green"

		MouseArea {
			anchors.fill: parent
			onPressed: { parent.color = "blue" }
			onReleased: { parent.color = "green"; parent.clicked("bro") }
		}

		signal clicked(var txt)
	}

	function scramble(txt) {
		console.log("md5(\"" + txt + "\") = " + Qt.md5(txt))
	}
}