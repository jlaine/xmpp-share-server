include(CheckIncludeFiles)

include_directories(${QDJANGO_INCLUDE_DIRS})
include_directories(${QXMPP_INCLUDE_DIRS})
link_directories(${QDJANGO_LIBRARY_DIRS})
link_directories(${QDXMPP_LIBRARY_DIRS})

add_subdirectory(qxmpp-extra)
add_subdirectory(plugins)

# Required libraries
set(QT_LIBRARIES Qt5::Network Qt5::Sql Qt5::Xml)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/plugins)

add_executable(xmpp-share-server server.cpp)
target_link_libraries(xmpp-share-server qdjango-db qxmpp Qt5::Network Qt5::Sql)
install(TARGETS xmpp-share-server DESTINATION bin)
