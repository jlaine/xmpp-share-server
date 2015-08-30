include(CheckIncludeFiles)

include_directories(${QDJANGO_INCLUDE_DIRS})
include_directories(${QXMPP_INCLUDE_DIRS})
link_directories(${QDJANGO_LIBRARY_DIRS})
link_directories(${QDXMPP_LIBRARY_DIRS})

add_subdirectory(qxmpp-extra)
add_subdirectory(plugins)

# Required libraries
set(QT_DONT_USE_QTGUI 1)
set(QT_USE_QTNETWORK 1)
set(QT_USE_QTSQL 1)
set(QT_USE_QTXML 1)
include(${QT_USE_FILE})

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/qxmpp-extra/shares)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/plugins)

add_executable(xmpp-share-client client.cpp)
target_link_libraries(xmpp-share-client qxmpp qxmpp-extra ${QT_QTSQL_LIBRARY})
install(TARGETS xmpp-share-client DESTINATION bin)

add_executable(xmpp-share-server server.cpp)
target_link_libraries(xmpp-share-server qdjango-db qxmpp ${QT_QTCORE_LIBRARY} ${QT_QTNETWORK_LIBRARY} ${QT_QTSQL_LIBRARY})
install(TARGETS xmpp-share-server DESTINATION bin)
