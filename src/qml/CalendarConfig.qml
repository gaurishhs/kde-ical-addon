/*
    SPDX-FileCopyrightText: 2026 iCal Calendar Plasma plugin authors
    SPDX-License-Identifier: GPL-2.0-or-later

    Configuration page shown by the Digital Clock applet for this plugin.

    It follows the same contract as the upstream calendar plugins:
      - signal configurationChanged
      - property unsavedChanges
      - function saveConfig()
*/

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.plasma.icalcalendar 1.0

KCM.SimpleKCM {
    id: page

    // Expected API
    signal configurationChanged
    property bool unsavedChanges: settings.dirty

    readonly property var colorChoices: [
        "#3daee9", "#2ecc71", "#e67e22", "#9b59b6",
        "#e74c3c", "#16a085", "#f1c40f", "#e91e63"
    ]

    // Narrow windows stack labels above fields and actions below the input.
    readonly property bool narrowLayout: width < Kirigami.Units.gridUnit * 30

    // Expected API
    function saveConfig() {
        settings.saveConfig();
        unsavedChanges = false;
    }

    CalendarSettings {
        id: settings

        onDirtyChanged: {
            if (settings.dirty) {
                page.configurationChanged();
            }
        }
        onError: (message) => {
            errorMessage.text = message;
            errorMessage.visible = true;
        }
    }

    Kirigami.FormLayout {
        // wideMode is intentionally left at its default, which switches
        // automatically based on the page width (Kirigami's wideImplicitWidth).

        Kirigami.InlineMessage {
            id: errorMessage
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            visible: false
            type: Kirigami.MessageType.Error
            showCloseButton: true
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            visible: !settings.walletAvailable && settings.walletStatus.length > 0
            type: Kirigami.MessageType.Warning
            text: settings.walletStatus
        }

        QQC2.Label {
            Kirigami.FormData.isSection: true
            text: i18n("Calendars")
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: i18n("Give each calendar a display name and paste its private or public iCal address. " +
                           "The addresses are stored in KWallet and are never written to a configuration file.")
                wrapMode: Text.WordWrap
                opacity: 0.8
            }

            Repeater {
                model: settings.calendars

                delegate: ColumnLayout {
                    id: calendarRow

                    required property string calendarId
                    required property string calendarName
                    required property string calendarColor
                    required property bool calendarEnabled
                    required property bool calendarHasUrl

                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.CheckBox {
                            checked: calendarRow.calendarEnabled
                            onToggled: settings.setEnabled(calendarRow.calendarId, checked)

                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: i18n("Show events from this calendar")
                        }

                        QQC2.TextField {
                            Layout.fillWidth: true
                            text: calendarRow.calendarName
                            placeholderText: i18n("Display name")
                            Accessible.name: i18n("Calendar display name")
                            onEditingFinished: settings.setName(calendarRow.calendarId, text)

                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: i18n("Name shown for this calendar")
                        }

                        QQC2.ComboBox {
                            id: colorBox

                            Layout.preferredWidth: page.narrowLayout
                                ? Kirigami.Units.gridUnit * 3
                                : Kirigami.Units.gridUnit * 4
                            model: page.colorChoices
                            currentIndex: Math.max(0, page.colorChoices.indexOf(calendarRow.calendarColor))
                            onActivated: settings.setColor(calendarRow.calendarId, page.colorChoices[currentIndex])

                            contentItem: Rectangle {
                                implicitWidth: Kirigami.Units.gridUnit * 2
                                implicitHeight: Kirigami.Units.gridUnit
                                radius: height / 2
                                color: page.colorChoices[colorBox.currentIndex]
                                border.color: Kirigami.Theme.textColor
                                border.width: 1
                            }

                            delegate: QQC2.ItemDelegate {
                                width: colorBox.width
                                contentItem: Rectangle {
                                    implicitHeight: Kirigami.Units.gridUnit
                                    radius: height / 2
                                    color: page.colorChoices[index]
                                    border.color: Kirigami.Theme.textColor
                                    border.width: 1
                                }
                                onClicked: {
                                    colorBox.currentIndex = index;
                                    settings.setColor(calendarRow.calendarId, page.colorChoices[index]);
                                }
                            }

                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: i18n("Colour used for this calendar's events")
                        }

                        QQC2.ToolButton {
                            icon.name: "delete"
                            onClicked: settings.removeCalendar(calendarRow.calendarId)

                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: i18n("Remove this calendar")
                        }
                    }

                    ColumnLayout {
                        Layout.leftMargin: Kirigami.Units.gridUnit * 2
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Icon {
                                source: calendarRow.calendarHasUrl ? "dialog-ok-apply" : "dialog-warning"
                                implicitWidth: Kirigami.Units.iconSizes.small
                                implicitHeight: Kirigami.Units.iconSizes.small

                                HoverHandler { id: statusHover }
                                QQC2.ToolTip.visible: statusHover.hovered
                                QQC2.ToolTip.text: calendarRow.calendarHasUrl
                                    ? i18n("An address is saved in KWallet.")
                                    : i18n("No address set — this calendar is skipped.")
                            }

                            QQC2.TextField {
                                id: addressField
                                Layout.fillWidth: true
                                placeholderText: i18n("Paste the iCal address (https://…/basic.ics)")
                                echoMode: TextInput.Password
                                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText | Qt.ImhSensitiveData
                                onAccepted: saveAddress()

                                function saveAddress() {
                                    if (text.length === 0) {
                                        return;
                                    }
                                    if (settings.setUrl(calendarRow.calendarId, text)) {
                                        text = "";
                                    }
                                }

                                QQC2.ToolTip.visible: hovered
                                QQC2.ToolTip.text: calendarRow.calendarHasUrl
                                    ? i18n("An address is already saved. Paste a new one to replace it.")
                                    : i18n("The address is stored in KWallet and is never shown again.")
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Item { Layout.fillWidth: true }

                            QQC2.Button {
                                text: i18n("Save")
                                enabled: addressField.text.length > 0
                                onClicked: addressField.saveAddress()
                            }

                            QQC2.Label {
                                visible: calendarRow.calendarHasUrl
                                text: i18n("Saved")
                                opacity: 0.7
                            }

                            QQC2.Button {
                                visible: calendarRow.calendarHasUrl
                                text: i18n("Remove")
                                onClicked: settings.clearUrl(calendarRow.calendarId)

                                QQC2.ToolTip.visible: hovered
                                QQC2.ToolTip.text: i18n("Forget the saved address")
                            }
                        }
                    }

                    Kirigami.Separator {
                        Layout.fillWidth: true
                        Layout.topMargin: Kirigami.Units.smallSpacing
                    }
                }
            }

            QQC2.Button {
                text: i18n("Add Calendar")
                icon.name: "list-add"
                onClicked: settings.addCalendar()
            }
        }

        QQC2.SpinBox {
            Kirigami.FormData.label: i18n("Refresh every:")
            from: 1
            to: 1440
            value: settings.refreshMinutes
            textFromValue: (value) => i18np("%1 minute", "%1 minutes", value)
            onValueModified: settings.refreshMinutes = value
        }

    }

}
