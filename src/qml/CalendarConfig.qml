/*
    SPDX-FileCopyrightText: 2026 Google Calendar Plasma plugin authors
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
import org.kde.plasma.googlecalendar 1.0

KCM.SimpleKCM {
    id: page

    // Expected API
    signal configurationChanged
    property bool unsavedChanges: settings.dirty

    readonly property var colorChoices: [
        "#3daee9", "#2ecc71", "#e67e22", "#9b59b6",
        "#e74c3c", "#16a085", "#f1c40f", "#e91e63"
    ]

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
        wideMode: true

        Kirigami.InlineMessage {
            id: errorMessage
            Layout.fillWidth: true
            Layout.preferredWidth: Kirigami.Units.gridUnit * 30
            visible: false
            type: Kirigami.InlineMessage.Type.Error
            showCloseButton: true
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.preferredWidth: Kirigami.Units.gridUnit * 30
            visible: !settings.walletAvailable && settings.walletStatus.length > 0
            type: Kirigami.InlineMessage.Type.Warning
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
                text: i18n("Paste the private iCal address of each Google Calendar. " +
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
                            placeholderText: i18n("Calendar name")
                            onEditingFinished: settings.setName(calendarRow.calendarId, text)
                        }

                        QQC2.ComboBox {
                            id: colorBox

                            Layout.preferredWidth: Kirigami.Units.gridUnit * 4
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

                    RowLayout {
                        Layout.leftMargin: Kirigami.Units.gridUnit * 2
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: calendarRow.calendarHasUrl ? "dialog-ok-apply" : "dialog-warning"
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: Kirigami.Units.iconSizes.small
                        }

                        QQC2.Label {
                            Layout.fillWidth: true
                            text: calendarRow.calendarHasUrl
                                  ? i18n("Private iCal address saved in KWallet")
                                  : i18n("No private iCal address set — this calendar is skipped")
                            opacity: 0.8
                            elide: Text.ElideRight
                        }

                        QQC2.Button {
                            text: i18n("Set Address…")
                            onClicked: {
                                urlDialog.calendarId = calendarRow.calendarId;
                                urlField.text = "";
                                urlDialog.open();
                            }
                        }

                        QQC2.Button {
                            visible: calendarRow.calendarHasUrl
                            text: i18n("Remove Address")
                            onClicked: settings.clearUrl(calendarRow.calendarId)
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
                onClicked: {
                    const id = settings.addCalendar();
                    urlDialog.calendarId = id;
                    urlField.text = "";
                    urlDialog.open();
                }
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

        QQC2.CheckBox {
            Kirigami.FormData.label: i18n("Testing:")
            text: i18n("Show demo events")
            checked: settings.demoMode
            onToggled: settings.demoMode = checked
        }

        QQC2.Label {
            Layout.fillWidth: true
            Layout.preferredWidth: Kirigami.Units.gridUnit * 30
            text: i18n("Demo mode ignores the configured calendars and shows a fixed set of sample events. " +
                       "It is only useful for checking that the plugin is loaded.")
            wrapMode: Text.WordWrap
            opacity: 0.8
        }
    }

    QQC2.Dialog {
        id: urlDialog

        property string calendarId

        modal: true
        title: i18n("Private iCal Address")
        width: Math.min(page.width - Kirigami.Units.gridUnit * 4, Kirigami.Units.gridUnit * 32)
        closePolicy: QQC2.Popup.CloseOnEscape

        contentItem: ColumnLayout {
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                Layout.fillWidth: true
                Layout.preferredWidth: Kirigami.Units.gridUnit * 28
                text: i18n("In Google Calendar open Settings → “<calendar>” → “Integrate calendar” and copy " +
                           "the “Secret address in iCal format”. It works without OAuth and gives read-only access.")
                wrapMode: Text.WordWrap
            }

            QQC2.TextField {
                id: urlField
                Layout.fillWidth: true
                placeholderText: "https://calendar.google.com/calendar/ical/…/basic.ics"
                echoMode: TextInput.Password
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText | Qt.ImhSensitiveData
                onAccepted: saveButton.clicked()
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: i18n("The address is stored in KWallet and is never shown again.")
                wrapMode: Text.WordWrap
                opacity: 0.8
            }
        }

        footer: QQC2.DialogButtonBox {
            QQC2.Button {
                text: i18n("Cancel")
                onClicked: {
                    urlField.text = "";
                    urlDialog.close();
                }
            }

            QQC2.Button {
                id: saveButton
                text: i18n("Save Address")
                enabled: urlField.text.length > 0
                onClicked: {
                    if (settings.setUrl(urlDialog.calendarId, urlField.text)) {
                        urlField.text = "";
                        urlDialog.close();
                    }
                }
            }
        }
    }
}
