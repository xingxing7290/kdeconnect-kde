find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)

ecm_add_test(paneldocumenttest.cpp LINK_LIBRARIES kdeconnectpanel Qt::Test)
ecm_add_test(panelstoretest.cpp LINK_LIBRARIES kdeconnectpanel Qt::Test)
ecm_add_test(panelpeertest.cpp LINK_LIBRARIES kdeconnectpanel Qt::Test)
ecm_add_test(panelforegroundtest.cpp LINK_LIBRARIES kdeconnectpanel Qt::Test)
ecm_add_test(panelruntimetest.cpp LINK_LIBRARIES kdeconnectpanel Qt::Test)
if(WIN32)
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
