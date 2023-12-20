import QtQuick 2.6
import QtQuick.Window 2.2
import QtQuick.Controls 1.4
import qml.components.device 1.0
import qml.components.buttons 1.0
import qml.components.frames 1.0
import QtQuick.Layouts 1.3
import QtQuick.Dialogs 1.2

Window {
    visible: true
    id:window
    //    color: "black"
    title: qsTr("camApp")
    visibility:Window.Maximized

    property int m_brightness:10

    RowLayout{
        id:rowlayout
        width:parent.width
        height: parent.height

        MessageDialog
        {
            id:dialog
            title: "Notification"
            text: "Device connection lost."
//            onAccepted:
//            {
//                camera.device_enumerate()
//            }
            Component.onCompleted: visible = false
        }

        ColumnLayout{
            id:columnlayout1
            anchors.top: rowlayout.top
            anchors.topMargin: 20
            Layout.margins: 20


            ComboBox {
                implicitWidth: 250
                id: device_comboBox
                model: device
                textRole: "display"
                MouseArea
                {
                    anchors.fill: parent
                    onPressed:
                    {
                        if(pressed)
                        {
                            camera.device_enumerate()
                        }
                        mouse.accepted = false
                    }
                }
                onCurrentIndexChanged:
                {
                    camera.selectDevice(currentIndex)
                    sideBar.getTab(0).item.children[0].enabled = true
                }
                Component.onCompleted:
                {
                    camera.device_enumerate()
                }
            }

            TabView{
                id:sideBar
                implicitWidth: 250
                implicitHeight: 250

                Tab{
                    id:uvcContols
                    title:qsTr("UVC Control");
                    ColumnLayout{
                        id:tab1_combobox
                        ComboBox
                        {
                            implicitWidth: 250
                            id:format_combo_box
                            model: format
                            textRole: "display"
                            onCurrentIndexChanged:
                            {
                                camera.enumResolution(currentIndex)
                                resolutionCombobox.enabled = true
                            }
                            Component.onCompleted: enabled = false
                        }
                        ComboBox
                        {
                            implicitWidth: 250
                            id:resolutionCombobox
                            model: resolution
                            textRole: "display"
                            onCurrentIndexChanged:
                            {
                                camera.enumFps(currentIndex)
                                fpsCombobox.enabled = true
                            }
                           Component.onCompleted: enabled = false
                        }
                        ComboBox
                        {
                            implicitWidth: 250
                            id:fpsCombobox
                            model: fps
                            textRole: "display"
                            onCurrentIndexChanged:
                            {
                                camera.selectFps(currentIndex)
                            }
                           Component.onCompleted: enabled = false
                        }
                        GridLayout{
                            id:gridlayout1
                            columns: 3

                            Text{
                                id:text1
                                text:"Brightness"
                            }
                            Slider{
                                id:brightness
                                stepSize: 1
                                minimumValue: 0
                                maximumValue: m_brightness
                                onValueChanged:
                                {
                                    camera.set_brightness(brightness.value);
                                }
                            }
                            TextField{
                                id:textfield1
                                implicitWidth: 50
                                text: brightness.value;
                            }

                            Text{
                                id:text2
                                text:"Contrast"
                            }
                            Slider{
                                id:contrast
                                maximumValue: 100
                                stepSize: 1
                            }
                            TextField{
                                id:textfield2
                                implicitWidth: 50
                                text: contrast.value;
                            }

                            Text{
                                id:text3
                                text:"Exposure"
                            }
                            Slider{
                                id:exposure
                                maximumValue: 100
                                stepSize: 1
                            }
                            TextField{
                                id:textfield3
                                implicitWidth: 50
                                text: exposure.value;
                            }
                        }
                    }
                }
                Tab{
                    id:extensionControls
                    width: 200
                    title:qsTr("Extension control");

                    ColumnLayout{
                        id:columnlayout2

                        GroupBox{
                            id:group1

                            RowLayout{
                                id:rowlayout2
                                ExclusiveGroup{
                                    id:radio
                                }

                                RadioButton {
                                    id:radioButton1
                                    text: qsTr("Button1")
                                    exclusiveGroup: radio
                                    onClicked:
                                    {
                                        button.ret_button1()
                                    }
                                }

                                RadioButton {
                                    id:radioButton2
                                    text: qsTr("Button2")
                                    exclusiveGroup: radio
                                    onClicked:
                                    {
                                        button.ret_button2()
                                    }
                                }
                            }
                        }

                        Button{
                            id:button1
                            text: qsTr("Push Button")
                            onClicked:
                            {
                                button.ret_button()
                            }
                        }
                    }
                }
            }

        }
    }
    Control{
        id:button
    }
    Devices{
        id: camera
        //        width: 640
        //        height: 480
        //        onEmitsignal: {
        //            show_image.set_picture(img)
        //            rectangle.implicitHeight = img_height
        //            rectangle.implicitWidth = img_width
        //        }
        onEmitformat: {
            sideBar.getTab(0).item.children[0].currentIndex = index
        }
        onEmitresolution: {
            sideBar.getTab(0).item.children[1].currentIndex = index
        }
        onEmitfps: {
            sideBar.getTab(0).item.children[2].currentIndex = index
        }
        onEmit_max_brightness:
        {
            m_brightness=value;
        }
        onDevice_disconnected: {
            dialog.visible = true
            device_comboBox.currentIndex= 0
            sideBar.getTab(0).item.children[0].currentIndex = 0
            sideBar.getTab(0).item.children[1].currentIndex = 0
            sideBar.getTab(0).item.children[2].currentIndex = 0
            sideBar.getTab(0).item.children[0].enabled = false
             sideBar.getTab(0).item.children[1].enabled = false
             sideBar.getTab(0).item.children[2].enabled = false
        }
    }
}

