cmake_minimum_required(VERSION 3.16)
project(titanshare-daemon VERSION 2.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Find Dependencies ───────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(LIBQRENCODE REQUIRED libqrencode)
pkg_check_modules(LIBPNG REQUIRED libpng)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)
pkg_check_modules(DBUS REQUIRED dbus-1)

# nlohmann/json - header only
find_path(NLOHMANN_JSON_INCLUDE nlohmann/json.hpp
    PATHS /usr/include /usr/local/include
)
if(NOT NLOHMANN_JSON_INCLUDE)
    # Fallback to bundled third_party copy
    if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/nlohmann/json.hpp")
        set(NLOHMANN_JSON_INCLUDE "${CMAKE_SOURCE_DIR}/third_party")
        message(STATUS "Using bundled nlohmann/json from third_party/")
    else()
        message(FATAL_ERROR "nlohmann/json.hpp not found. Install: sudo pacman -S nlohmann-json")
    endif()
endif()

# ─── Source Files ─────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/utils/logger.cpp
    src/utils/network.cpp
    src/utils/process.cpp
    src/server/tcp_server.cpp
    src/server/client_session.cpp
    src/auth/session_manager.cpp
    src/commands/command_dispatcher.cpp
    src/commands/power_commands.cpp
    src/commands/volume_commands.cpp
    src/commands/lock_commands.cpp
    src/commands/system_info.cpp
    src/file_transfer/file_receiver.cpp
    src/input/virtual_input.cpp
    src/qr/qr_generator.cpp
    src/notifications/notification_bridge.cpp
)

# ─── Build Target ─────────────────────────────────────────────────────
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${NLOHMANN_JSON_INCLUDE}
    ${LIBQRENCODE_INCLUDE_DIRS}
    ${LIBPNG_INCLUDE_DIRS}
    ${SYSTEMD_INCLUDE_DIRS}
    ${DBUS_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    ${LIBQRENCODE_LIBRARIES}
    ${LIBPNG_LIBRARIES}
    ${SYSTEMD_LIBRARIES}
    ${DBUS_LIBRARIES}
)

# ─── Install ──────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES systemd/titanshare.service DESTINATION lib/systemd/system)
install(FILES config/titanshare.conf DESTINATION /etc/titanshare)

# ─── GUI Target ───────────────────────────────────────────────────────
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Gui Qml Quick OPTIONAL_COMPONENTS)

if(Qt6_FOUND)
    message(STATUS "Qt6 found. Building titanshare-gui.")
    
    set(GUI_SOURCES
        src_gui/main.cpp
        src_gui/titanshare_gui.qrc
    )
    
    add_executable(titanshare-gui ${GUI_SOURCES})
    
    target_link_libraries(titanshare-gui PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
    )
    
    install(TARGETS titanshare-gui DESTINATION bin)
else()
    message(WARNING "Qt6 not found. The GUI target (titanshare-gui) will not be built.")
endif()
