set(QT_DONT_USE_QTGUI 1)
set(QT_USE_QTNETWORK 1)
set(QT_USE_QTSQL 1)
set(QT_USE_QTXML 1)
include(${QT_USE_FILE})

# needed for win32
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)
add_definitions(-DQT_NO_DEBUG)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../qxmpp-extra/diagnostics)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../qxmpp-extra/shares)

qt4_wrap_cpp(mod_archive_MOC_SOURCES mod_archive.h)
add_library(mod_archive SHARED mod_archive.cpp ${mod_archive_MOC_SOURCES})
target_link_libraries(mod_archive mod_presence qdjango-db ${QT_LIBRARIES})

qt4_wrap_cpp(mod_auth_MOC_SOURCES mod_auth.h)
add_library(mod_auth SHARED mod_auth.cpp ${mod_auth_MOC_SOURCES})
target_link_libraries(mod_auth qdjango-db qdjango-http qxmpp ${QT_LIBRARIES} ${AUTH_LIBRARIES})

qt4_wrap_cpp(mod_auth_proxy_MOC_SOURCES mod_auth_proxy.h)
add_library(mod_auth_proxy SHARED mod_auth_proxy.cpp ${mod_auth_proxy_MOC_SOURCES})
target_link_libraries(mod_auth_proxy qdjango-db qdjango-http qxmpp ${QT_LIBRARIES})

qt4_wrap_cpp(mod_bosh_MOC_SOURCES mod_bosh.h QXmppIncomingBosh.h)
add_library(mod_bosh SHARED mod_bosh.cpp QXmppIncomingBosh.cpp ${mod_bosh_MOC_SOURCES})
target_link_libraries(mod_bosh qdjango-http qxmpp ${QT_LIBRARIES})

qt4_wrap_cpp(mod_diag_MOC_SOURCES mod_diag.h QXmppIncomingBosh.h)
add_library(mod_diag SHARED mod_diag.cpp QXmppIncomingBosh.cpp ${mod_diag_MOC_SOURCES})
target_link_libraries(mod_diag qdjango-db qdjango-http qxmpp qxmpp-extra ${QT_LIBRARIES})

qt4_wrap_cpp(mod_disco_MOC_SOURCES mod_disco.h)
add_library(mod_disco SHARED mod_disco.cpp ${mod_disco_MOC_SOURCES})
target_link_libraries(mod_disco qxmpp ${QT_LIBRARIES})

qt4_wrap_cpp(mod_http_MOC_SOURCES mod_http.h QXmppIncomingBosh.h)
add_library(mod_http SHARED mod_http.cpp QXmppIncomingBosh.cpp ${mod_http_MOC_SOURCES})
target_link_libraries(mod_http qdjango-http qxmpp ${QT_LIBRARIES})

qt4_wrap_cpp(mod_muc_MOC_SOURCES mod_muc.h)
add_library(mod_muc SHARED mod_muc.cpp ${mod_muc_MOC_SOURCES})
target_link_libraries(mod_muc qdjango-db qxmpp ${QT_LIBRARIES})

qt4_wrap_cpp(mod_ping_MOC_SOURCES mod_ping.h)
add_library(mod_ping SHARED mod_ping.cpp ${mod_ping_MOC_SOURCES})
target_link_libraries(mod_ping qxmpp ${QT_LIBRARIES})

qt4_wrap_cpp(mod_presence_MOC_SOURCES mod_presence.h)
add_library(mod_presence SHARED mod_presence.cpp ${mod_presence_MOC_SOURCES})
target_link_libraries(mod_presence qxmpp ${QT_LIBRARIES})

qt4_wrap_cpp(mod_privacy_MOC_SOURCES mod_privacy.h)
add_library(mod_privacy SHARED mod_privacy.cpp ${mod_privacy_MOC_SOURCES})
target_link_libraries(mod_privacy mod_roster qxmpp ${QT_LIBRARIES})

qt4_wrap_cpp(mod_private_MOC_SOURCES mod_private.h)
add_library(mod_private SHARED mod_private.cpp ${mod_private_MOC_SOURCES})
target_link_libraries(mod_private qdjango-db qxmpp ${QT_LIBRARIES})

qt4_wrap_cpp(mod_proxy65_MOC_SOURCES mod_proxy65.h)
add_library(mod_proxy65 SHARED mod_proxy65.cpp ${mod_proxy65_MOC_SOURCES})
target_link_libraries(mod_proxy65 qxmpp ${QT_LIBRARIES})

qt4_wrap_cpp(mod_roster_MOC_SOURCES mod_roster.h)
add_library(mod_roster SHARED mod_roster.cpp ${mod_roster_MOC_SOURCES})
target_link_libraries(mod_roster mod_presence qdjango-db ${QT_LIBRARIES})

qt4_wrap_cpp(mod_share_MOC_SOURCES mod_share.h)
add_library(mod_share SHARED mod_share.cpp ${mod_share_MOC_SOURCES})
target_link_libraries(mod_share qxmpp qxmpp-extra ${QT_LIBRARIES})

qt4_wrap_cpp(mod_stat_MOC_SOURCES mod_stat.h)
add_library(mod_stat SHARED mod_stat.cpp ${mod_stat_MOC_SOURCES})
target_link_libraries(mod_stat mod_roster qdjango-http qxmpp ${QT_LIBRARIES})

qt4_wrap_cpp(mod_time_MOC_SOURCES mod_time.h)
add_library(mod_time SHARED mod_time.cpp ${mod_time_MOC_SOURCES})
target_link_libraries(mod_time qxmpp ${QT_LIBRARIES})

qt4_wrap_cpp(mod_turn_MOC_SOURCES mod_turn.h)
add_library(mod_turn SHARED mod_turn.cpp ${mod_turn_MOC_SOURCES})
target_link_libraries(mod_turn qxmpp ${QT_LIBRARIES})

qt4_wrap_cpp(mod_vcard_MOC_SOURCES mod_vcard.h)
add_library(mod_vcard SHARED mod_vcard.cpp ${mod_vcard_MOC_SOURCES})
target_link_libraries(mod_vcard qdjango-db qxmpp ${QT_LIBRARIES})

qt4_wrap_cpp(mod_version_MOC_SOURCES mod_version.h)
add_library(mod_version SHARED mod_version.cpp ${mod_version_MOC_SOURCES})
target_link_libraries(mod_version qxmpp ${QT_LIBRARIES})

qt4_wrap_cpp(mod_wifirst_MOC_SOURCES mod_wifirst.h)
add_library(mod_wifirst SHARED mod_wifirst.cpp ${mod_wifirst_MOC_SOURCES})
target_link_libraries(mod_wifirst mod_vcard ${QT_LIBRARIES})

install(FILES
    mod_archive.pluginspec
    mod_auth.pluginspec
    mod_auth_proxy.pluginspec
    mod_bosh.pluginspec
    mod_diag.pluginspec
    mod_disco.pluginspec
    mod_http.pluginspec
    mod_muc.pluginspec
    mod_ping.pluginspec
    mod_presence.pluginspec
    mod_privacy.pluginspec
    mod_private.pluginspec
    mod_proxy65.pluginspec
    mod_roster.pluginspec
    mod_share.pluginspec
    mod_stat.pluginspec
    mod_time.pluginspec
    mod_turn.pluginspec
    mod_vcard.pluginspec
    mod_version.pluginspec
    mod_wifirst.pluginspec
    DESTINATION ${SERVER_PLUGIN_DIR})

install(TARGETS
    mod_archive
    mod_auth
    mod_auth_proxy
    mod_bosh
    mod_diag
    mod_disco
    mod_http
    mod_muc
    mod_ping
    mod_presence
    mod_privacy
    mod_private
    mod_proxy65
    mod_roster
    mod_share
    mod_stat
    mod_time
    mod_turn
    mod_vcard
    mod_version
    mod_wifirst
    DESTINATION ${SERVER_PLUGIN_DIR})
