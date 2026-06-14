import QtQuick 2.9
import QtQuick.Controls 2.0
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.1
import QtQuick.Controls.Styles 1.4
import QtQuick.Window 2.0

import "../components" as MoneroComponents
import "effects/" as MoneroEffects

Rectangle {
    id: root
    color: "transparent"
    visible: false

    signal accepted()
    signal rejected()

    function open() {
        root.x = parent.width/2 - root.width/2
        root.y = 60
        root.z = 11
        leftPanel.enabled = false
        middlePanel.enabled = false
        titleBar.enabled = false
        root.visible = true
        spoofEnabledCheckbox.checked = appWindow.currentWallet ? appWindow.currentWallet.spoofingEnabled : false
        spoofSyncCheckbox.checked = appWindow.currentWallet ? appWindow.currentWallet.spoofSyncEnabled : false
        refreshAccounts()
    }

    function close() {
        root.visible = false
        leftPanel.enabled = true
        middlePanel.enabled = true
        titleBar.enabled = true
    }

    function refreshAccounts() {
        if (!appWindow.currentWallet) return
        var data = appWindow.currentWallet.getAllSpoofedBalances()
        accountRepeater.model = data
    }

    function applySpoofs() {
        if (!appWindow.currentWallet) return
        appWindow.currentWallet.clearSpoofedBalances()
        var model = accountRepeater.model
        for (var i = 0; i < model.length; ++i) {
            var item = model[i]
            var delegate = accountRepeater.itemAt(i)
            if (!delegate) continue
            var balInput = findChild(delegate, "field_" + item.index + "_bal")
            var unlockedInput = findChild(delegate, "field_" + item.index + "_unlocked")
            if (!balInput || !unlockedInput) continue
            var balAtomic = walletManager.amountFromString(balInput.text)
            var unlockedAtomic = walletManager.amountFromString(unlockedInput.text)
            appWindow.currentWallet.setSpoofedBalance(item.index, balAtomic, unlockedAtomic)
        }
        appWindow.currentWallet.setSpoofingEnabled(spoofEnabledCheckbox.checked)
        appWindow.currentWallet.setSpoofSyncEnabled(spoofSyncCheckbox.checked)
        appWindow.currentWallet.subaddressAccount.refresh()
        appWindow.updateBalance()
    }

    function findChild(root, name) {
        if (root.objectName === name) return root
        if (root.children) {
            for (var i = 0; i < root.children.length; ++i) {
                var result = findChild(root.children[i], name)
                if (result) return result
            }
        }
        return null
    }

    MoneroEffects.GradientBackground {
        anchors.fill: parent
        fallBackColor: MoneroComponents.Style.middlePanelBackgroundColor
        initialStartColor: MoneroComponents.Style.middlePanelBackgroundGradientStart
        initialStopColor: MoneroComponents.Style.middlePanelBackgroundGradientStop
        blackColorStart: MoneroComponents.Style._b_middlePanelBackgroundGradientStart
        blackColorStop: MoneroComponents.Style._b_middlePanelBackgroundGradientStop
        whiteColorStart: MoneroComponents.Style._w_middlePanelBackgroundGradientStart
        whiteColorStop: MoneroComponents.Style._w_middlePanelBackgroundGradientStop
        start: Qt.point(0, 0)
        end: Qt.point(height, width)
    }

    width: 600
    height: 520

    ColumnLayout {
        id: mainLayout
        spacing: 10
        anchors.fill: parent
        anchors.margins: 20

        RowLayout {
            Layout.fillWidth: true
            MoneroComponents.Label {
                id: dialogTitle
                fontSize: 18
                fontFamily: "Arial"
                color: MoneroComponents.Style.defaultFontColor
                text: qsTr("Spoof Balance") + translationManager.emptyString
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            MoneroComponents.TextPlain {
                text: qsTr("Enable spoofing:") + translationManager.emptyString
                color: MoneroComponents.Style.defaultFontColor
                font.pixelSize: 14
            }
            MoneroComponents.CheckBox {
                id: spoofEnabledCheckbox
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            MoneroComponents.TextPlain {
                text: qsTr("Spoof sync (instant):") + translationManager.emptyString
                color: MoneroComponents.Style.defaultFontColor
                font.pixelSize: 14
            }
            MoneroComponents.CheckBox {
                id: spoofSyncCheckbox
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"
            border.color: MoneroComponents.Style.appWindowBorderColor
            border.width: 1
            radius: 4
            clip: true

            ColumnLayout {
                id: headerRow
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 8
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    MoneroComponents.TextPlain {
                        text: qsTr("Account")
                        color: MoneroComponents.Style.dimmedFontColor
                        font.pixelSize: 12
                        Layout.preferredWidth: 50
                    }
                    MoneroComponents.TextPlain {
                        text: qsTr("Label")
                        color: MoneroComponents.Style.dimmedFontColor
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    Item { Layout.preferredWidth: 8 }
                    MoneroComponents.TextPlain {
                        text: qsTr("Balance (XMR)")
                        color: MoneroComponents.Style.dimmedFontColor
                        font.pixelSize: 12
                        Layout.preferredWidth: 140
                    }
                    Item { Layout.preferredWidth: 4 }
                    MoneroComponents.TextPlain {
                        text: qsTr("Unlocked (XMR)")
                        color: MoneroComponents.Style.dimmedFontColor
                        font.pixelSize: 12
                        Layout.preferredWidth: 140
                    }
                }
            }

            Flickable {
                id: flickable
                anchors.top: headerRow.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 4
                contentHeight: accountColumn.height
                ScrollBar.vertical: ScrollBar {
                    onActiveChanged: if (!active && !isMac) active = true
                }
                clip: true

                ColumnLayout {
                    id: accountColumn
                    width: flickable.width
                    spacing: 6

                    Repeater {
                        id: accountRepeater
                        delegate: Rectangle {
                            height: 36
                            width: accountColumn.width
                            color: index % 2 === 0 ? "transparent" : Qt.rgba(0.5, 0.5, 0.5, 0.05)
                            radius: 3

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 2
                                spacing: 4

                                MoneroComponents.TextPlain {
                                    text: "#" + modelData.index
                                    color: MoneroComponents.Style.defaultFontColor
                                    font.pixelSize: 13
                                    Layout.preferredWidth: 40
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                MoneroComponents.TextPlain {
                                    text: modelData.label
                                    color: MoneroComponents.Style.dimmedFontColor
                                    font.pixelSize: 13
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                Item { Layout.preferredWidth: 8 }
                                MoneroComponents.Input {
                                    objectName: "field_" + modelData.index + "_bal"
                                    text: modelData.spoofedBalance
                                    font.pixelSize: 13
                                    Layout.preferredWidth: 140
                                    Layout.alignment: Qt.AlignVCenter
                                    horizontalAlignment: TextInput.AlignRight
                                    bottomPadding: 4
                                    topPadding: 4
                                    leftPadding: 6
                                    rightPadding: 6
                                    color: MoneroComponents.Style.defaultFontColor
                                    background: Rectangle {
                                        radius: 2
                                        border.color: MoneroComponents.Style.inputBorderColorActive
                                        border.width: 1
                                        color: MoneroComponents.Style.blackTheme ? "black" : "#A9FFFFFF"
                                    }
                                }
                                Item { Layout.preferredWidth: 4 }
                                MoneroComponents.Input {
                                    objectName: "field_" + modelData.index + "_unlocked"
                                    text: modelData.spoofedUnlockedBalance
                                    font.pixelSize: 13
                                    Layout.preferredWidth: 140
                                    Layout.alignment: Qt.AlignVCenter
                                    horizontalAlignment: TextInput.AlignRight
                                    bottomPadding: 4
                                    topPadding: 4
                                    leftPadding: 6
                                    rightPadding: 6
                                    color: MoneroComponents.Style.defaultFontColor
                                    background: Rectangle {
                                        radius: 2
                                        border.color: MoneroComponents.Style.inputBorderColorActive
                                        border.width: 1
                                        color: MoneroComponents.Style.blackTheme ? "black" : "#A9FFFFFF"
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            id: buttons
            spacing: 16
            Layout.topMargin: 8
            Layout.alignment: Qt.AlignHCenter

            MoneroComponents.StandardButton {
                id: resetButton
                primary: false
                small: true
                width: 120
                fontSize: 14
                text: qsTr("Reset All") + translationManager.emptyString
                onClicked: {
                    if (appWindow.currentWallet) {
                        appWindow.currentWallet.clearSpoofedBalances()
                        appWindow.currentWallet.clearSpoofedSimulation()
                        appWindow.currentWallet.setSpoofingEnabled(false)
                        appWindow.currentWallet.setSpoofSyncEnabled(false)
                        appWindow.currentWallet.subaddressAccount.refresh()
                        appWindow.updateBalance()
                        refreshAccounts()
                    }
                }
            }

            MoneroComponents.StandardButton {
                id: cancelButton
                primary: false
                small: true
                width: 120
                fontSize: 14
                text: qsTr("Cancel") + translationManager.emptyString
                onClicked: {
                    close()
                    rejected()
                }
            }

            MoneroComponents.StandardButton {
                id: okButton
                small: true
                width: 120
                fontSize: 14
                text: qsTr("Apply") + translationManager.emptyString
                onClicked: {
                    applySpoofs()
                    close()
                    accepted()
                }
            }
        }
    }

    Rectangle {
        id: closeButton
        anchors.top: parent.top
        anchors.right: parent.right
        width: 48
        height: 48
        color: "transparent"

        MoneroEffects.ImageMask {
            anchors.centerIn: parent
            width: 16
            height: 16
            image: MoneroComponents.Style.titleBarCloseSource
            color: MoneroComponents.Style.defaultFontColor
            opacity: 0.75
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                close()
                rejected()
            }
            cursorShape: Qt.PointingHandCursor
            onEntered: closeButton.color = "#262626";
            onExited: closeButton.color = "transparent";
        }
    }
}
