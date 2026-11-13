cmake_minimum_required(VERSION 3.22)

project(icalcalendar VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# ECM gives us KDEInstallDirs (KDE_INSTALL_PLUGINDIR, KDE_INSTALL_QMLDIR, ...)
find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_MODULE_PATH})
include(KDEInstallDirs)

# Qt 6 plugin/QML directories on Fedora live in <libdir>/qt6/{plugins,qml}.
# Match the system layout so a user-local install lands where Qt looks.
set(KDE_INSTALL_PLUGINDIR "${KDE_INSTALL_LIBDIR}/qt6/plugins")
set(KDE_INSTALL_QMLDIR "${KDE_INSTALL_LIBDIR}/qt6/qml")

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Qml Concurrent)
# EventData::setLocation() appeared in KF6 6.27.
find_package(KF6Declarative 6.27 REQUIRED COMPONENTS CalendarEvents)
find_package(KF6Wallet REQUIRED)
find_package(LibIcal REQUIRED)

# ---------------------------------------------------------------------------
# Shared, non-UI code: settings, secret storage, feed fetching, iCalendar.
# ---------------------------------------------------------------------------
add_library(icalcalendar_core STATIC
    src/types.h
    src/types.cpp
    src/icalparser.h
    src/icalparser.cpp
    src/settings.h
    src/settings.cpp
    src/secretstore.h
    src/secretstore.cpp
    src/feedfetcher.h
    src/feedfetcher.cpp
)
set_target_properties(icalcalendar_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    # This static core is embedded in both add-on DSOs; hidden visibility
    # prevents them from exporting/interposing the same symbols, which
    # caused crashes during unload.
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
target_include_directories(icalcalendar_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(icalcalendar_core
    PUBLIC
        Qt6::Core
        Qt6::Network
        ical
        KF6::Wallet
)

# ---------------------------------------------------------------------------
# The Plasma calendar addon.
#
# Deliberately a MODULE library named "icalcalendar" with no "lib" prefix:
# KPluginMetaData derives the plugin id from the file name, and
# EventPluginsManager loads plugins with
#   QPluginLoader("plasmacalendarplugins/" + pluginId)
# so the installed file must be
#   <plugindir>/plasmacalendarplugins/icalcalendar.so
# ---------------------------------------------------------------------------
add_library(icalcalendar MODULE
    src/addon/icalcalendaraddon.h
    src/addon/icalcalendaraddon.cpp
    src/addon/icalcalendaraddon.json
)
target_link_libraries(icalcalendar
    PRIVATE
        icalcalendar_core
        Qt6::Core
        Qt6::Concurrent
        KF6::CalendarEvents
)
set_target_properties(icalcalendar PROPERTIES
    PREFIX ""
    OUTPUT_NAME "icalcalendar"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plasmacalendarplugins"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

install(TARGETS icalcalendar
    LIBRARY DESTINATION "${KDE_INSTALL_PLUGINDIR}/plasmacalendarplugins"
)

# The metadata points at "icalcalendar/ICalCalendarConfig.qml"; Plasma
# resolves that relative to the directory containing the .so.
install(FILES src/qml/CalendarConfig.qml
    RENAME ICalCalendarConfig.qml
    DESTINATION "${KDE_INSTALL_PLUGINDIR}/plasmacalendarplugins/icalcalendar"
)

# ---------------------------------------------------------------------------
# User-local QML module backing the config page.
#
# The page needs C++ (KWallet access and the config file), and the addon
# object itself is private to EventPluginsManager, so the sanctioned approach
# is a separate QML module: the .so keeps the default "lib" prefix because
# that is what QQmlExtensionPlugin looks for.
# ---------------------------------------------------------------------------
add_library(icalcalendarsettings MODULE
    src/qml/qmlextension.cpp
    src/qml/calendarlistmodel.h
    src/qml/calendarlistmodel.cpp
    src/qml/calendarsettings.h
    src/qml/calendarsettings.cpp
)
target_link_libraries(icalcalendarsettings
    PRIVATE
        icalcalendar_core
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
)
set_target_properties(icalcalendarsettings PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/org/kde/plasma/icalcalendar"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

set(ICALCALENDAR_QML_DIR "${CMAKE_BINARY_DIR}/qml/org/kde/plasma/icalcalendar")
file(WRITE "${ICALCALENDAR_QML_DIR}/qmldir"
"module org.kde.plasma.icalcalendar
plugin icalcalendarsettings
")

install(TARGETS icalcalendarsettings
    LIBRARY DESTINATION "${KDE_INSTALL_QMLDIR}/org/kde/plasma/icalcalendar"
)
install(FILES "${ICALCALENDAR_QML_DIR}/qmldir"
    DESTINATION "${KDE_INSTALL_QMLDIR}/org/kde/plasma/icalcalendar"
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
option(BUILD_TESTS "Build the test programs" ON)
if(BUILD_TESTS)
    enable_testing()

    add_executable(ical_parser_test tests/ical_parser_test.cpp)
    target_link_libraries(ical_parser_test PRIVATE icalcalendar_core Qt6::Core)
    add_test(NAME ical_parser_test COMMAND ical_parser_test)

    add_executable(feed_test tests/feed_test.cpp)
    target_link_libraries(feed_test PRIVATE icalcalendar_core Qt6::Core Qt6::Network)
    add_test(NAME feed_test COMMAND feed_test)

    add_executable(config_settings_test
        tests/config_settings_test.cpp
        src/qml/calendarlistmodel.cpp
    )
    target_include_directories(config_settings_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/qml")
    target_link_libraries(config_settings_test
        PRIVATE
            icalcalendar_core
            Qt6::Core
    )
    add_test(NAME config_settings_test COMMAND config_settings_test)
endif()

message(STATUS "Plugin install dir: ${KDE_INSTALL_PLUGINDIR}/plasmacalendarplugins")
message(STATUS "QML module install dir: ${KDE_INSTALL_QMLDIR}/org/kde/plasma/icalcalendar")
