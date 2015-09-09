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

add_library(mod_archive SHARED mod_archive.cpp)
target_link_libraries(mod_archive mod_presence qdjango-db ${QT_LIBRARIES})

add_library(mod_auth SHARED mod_auth.cpp)
target_link_libraries(mod_auth qdjango-db qdjango-http qxmpp ${QT_LIBRARIES} ${AUTH_LIBRARIES})

add_library(mod_auth_proxy SHARED mod_auth_proxy.cpp)
target_link_libraries(mod_auth_proxy qdjango-db qdjango-http qxmpp ${QT_LIBRARIES})

add_library(mod_bosh SHARED mod_bosh.cpp QXmppIncomingBosh.cpp)
target_link_libraries(mod_bosh qdjango-http qxmpp ${QT_LIBRARIES})

add_library(mod_diag SHARED mod_diag.cpp QXmppIncomingBosh.cpp)
target_link_libraries(mod_diag qdjango-db qdjango-http qxmpp qxmpp-extra ${QT_LIBRARIES})

add_library(mod_disco SHARED mod_disco.cpp)
target_link_libraries(mod_disco qxmpp ${QT_LIBRARIES})

add_library(mod_http SHARED mod_http.cpp QXmppIncomingBosh.cpp)
target_link_libraries(mod_http qdjango-http qxmpp ${QT_LIBRARIES})

add_library(mod_muc SHARED mod_muc.cpp)
target_link_libraries(mod_muc qdjango-db qxmpp ${QT_LIBRARIES})

add_library(mod_ping SHARED mod_ping.cpp)
target_link_libraries(mod_ping qxmpp ${QT_LIBRARIES})

add_library(mod_presence SHARED mod_presence.cpp)
target_link_libraries(mod_presence qxmpp ${QT_LIBRARIES})

add_library(mod_privacy SHARED mod_privacy.cpp)
target_link_libraries(mod_privacy mod_roster qxmpp ${QT_LIBRARIES})

add_library(mod_private SHARED mod_private.cpp)
target_link_libraries(mod_private qdjango-db qxmpp ${QT_LIBRARIES})

add_library(mod_proxy65 SHARED mod_proxy65.cpp)
target_link_libraries(mod_proxy65 qxmpp ${QT_LIBRARIES})

add_library(mod_roster SHARED mod_roster.cpp)
target_link_libraries(mod_roster mod_presence qdjango-db ${QT_LIBRARIES})

add_library(mod_stat SHARED mod_stat.cpp)
target_link_libraries(mod_stat mod_roster qdjango-http qxmpp ${QT_LIBRARIES})

add_library(mod_time SHARED mod_time.cpp)
target_link_libraries(mod_time qxmpp ${QT_LIBRARIES})

add_library(mod_turn SHARED mod_turn.cpp)
target_link_libraries(mod_turn qxmpp ${QT_LIBRARIES})

add_library(mod_vcard SHARED mod_vcard.cpp)
target_link_libraries(mod_vcard qdjango-db qxmpp ${QT_LIBRARIES})

add_library(mod_version SHARED mod_version.cpp)
target_link_libraries(mod_version qxmpp ${QT_LIBRARIES})

add_library(mod_wifirst SHARED mod_wifirst.cpp)
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
    mod_stat
    mod_time
    mod_turn
    mod_vcard
    mod_version
    mod_wifirst
    DESTINATION ${SERVER_PLUGIN_DIR})
