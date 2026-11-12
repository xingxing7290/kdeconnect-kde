find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
find_package(KF6BreezeIcons ${KF_MIN_VERSION} REQUIRED)
ecm_add_test(panelweathertest.cpp LINK_LIBRARIES kdeconnectpanelweather Qt::Test)
ecm_add_test(paneldialsuitest.cpp LINK_LIBRARIES kdeconnectpanelactions Qt::Quick Qt::QuickControls2 Qt::Test)
set_property(TEST paneldialsuitest APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
if(WIN32)
    add_executable(panelweatherintegration panelweatherintegration.cpp)
    target_link_libraries(panelweatherintegration PRIVATE kdeconnectpanelweather)
    set_property(TEST paneldialsuitest APPEND PROPERTY ENVIRONMENT "QT_QPA_FONTDIR=C:/Windows/Fonts")
endif()
ecm_add_test(panelassetstest.cpp LINK_LIBRARIES kdeconnectpanelactions Qt::Test)
ecm_add_test(panelassetsuitest.cpp LINK_LIBRARIES kdeconnectvolume kdeconnectpanelactions Qt::Quick Qt::QuickControls2 Qt::Test)
set_property(TEST panelassetsuitest APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(panelbuttonuitest.cpp LINK_LIBRARIES kdeconnectvolume kdeconnectpanelactions KF6::BreezeIcons Qt::Quick Qt::QuickControls2 Qt::Test)
qt_add_resources(panelbutton_test_icons ${CMAKE_SOURCE_DIR}/icons/custom_icons.qrc)
target_sources(panelbuttonuitest PRIVATE ${panelbutton_test_icons})
set_property(TEST panelbuttonuitest APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
if(WIN32)
    set_property(TEST panelbuttonuitest APPEND PROPERTY ENVIRONMENT "QT_QPA_FONTDIR=C:/Windows/Fonts")
endif()
ecm_add_test(volumecontrollertest.cpp LINK_LIBRARIES kdeconnectvolume Qt::Test)
ecm_add_test(volumeuitest.cpp LINK_LIBRARIES kdeconnectvolume kdeconnectpanelactions Qt::Quick Qt::QuickControls2 Qt::Test)
set_property(TEST volumeuitest APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(paneldocumenttest.cpp LINK_LIBRARIES kdeconnectpanel Qt::Test)
ecm_add_test(panelstoretest.cpp LINK_LIBRARIES kdeconnectpanel Qt::Test)
ecm_add_test(panelpeertest.cpp LINK_LIBRARIES kdeconnectpanel Qt::Test)
ecm_add_test(panelforegroundtest.cpp LINK_LIBRARIES kdeconnectpanel Qt::Test)
ecm_add_test(panelruntimetest.cpp LINK_LIBRARIES kdeconnectpanel Qt::Test)
if(WIN32)
    add_executable(panelassetsintegration panelassetsintegration.cpp)
    target_link_libraries(panelassetsintegration PRIVATE kdeconnectpanelactions)
    add_executable(volumeintegration volumeintegration.cpp)
    target_link_libraries(volumeintegration PRIVATE kdeconnectvolume)
    # Requires a desktop session and foreground activation permission; run manually.
    add_executable(panelforegroundintegration panelforegroundintegration.cpp)
    target_link_libraries(panelforegroundintegration PRIVATE kdeconnectpanel Qt::Gui)
endif()
ecm_add_test(panelactionstest.cpp LINK_LIBRARIES kdeconnectpanelactions Qt::Test)
ecm_add_test(paneleditortest.cpp LINK_LIBRARIES kdeconnectpanelactions Qt::Test)

set(kdeconnect_libraries
    kdeconnectcore
    kdeconnectdbusinterfaces
    kdeconnectmodels
    kdeconnectsmshelper
    kdeconnectversion
    KF6::GuiAddons
    KF6::I18n
    KF6::KIOWidgets
    KF6::Notifications
    Qt::DBus
    Qt::Network
    KF6::People
    Qt::Qml
    Qt::Test
)

ecm_add_test(notificationstest.cpp LINK_LIBRARIES ${kdeconnect_libraries})
ecm_add_test(pluginloadtest.cpp LINK_LIBRARIES ${kdeconnect_libraries})
ecm_add_test(sendfiletest.cpp LINK_LIBRARIES ${kdeconnect_libraries})
target_sources(sendfiletest PRIVATE testdevice.cpp)
ecm_add_test(smshelpertest.cpp LINK_LIBRARIES ${kdeconnect_libraries})
ecm_add_test(lanratelimittest.cpp LINK_LIBRARIES ${kdeconnect_libraries})
ecm_add_test(lanipv6test.cpp LINK_LIBRARIES ${kdeconnect_libraries})
if (WIN32 OR APPLE)
    ecm_add_test(mdnstest.cpp LINK_LIBRARIES ${kdeconnect_libraries})
endif()

# Do not touch the clipboard of the running session (and avoid a KSystemClipboard deadlock on exit under Wayland)
set_property(TEST notificationstest pluginloadtest APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
