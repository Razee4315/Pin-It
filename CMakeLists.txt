cmake_minimum_required(VERSION 3.21)

# RC enables compiling the Windows .rc (executable icon) via windres.
project(PinIt VERSION 2.0.0 LANGUAGES CXX RC)

# --- C++ / Qt setup -------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)   # process Q_OBJECT classes
set(CMAKE_AUTORCC ON)   # compile .qrc resources
set(CMAKE_AUTOUIC ON)   # compile .ui files (none yet, harmless)

# Network is used only for QLocalServer/QLocalSocket (single-instance IPC);
# windeployqt already bundles Qt6Network.dll transitively, so it adds no weight.
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
qt_standard_project_setup()

# Let the resource compiler find icon.ico (referenced relatively from app.rc).
# include_directories feeds both the C++ and RC compilers using each tool's
# correct flag (-I for windres/MinGW, /I for rc.exe/MSVC) — portable across CI.
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/resources")

# --- Version: single source of truth is project(VERSION) above ------------
# Generate version.h (used by the About dialog and the .rc VERSIONINFO) and
# version.iss (consumed by the Inno Setup installer) so the version is defined
# in exactly one place.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h" @ONLY)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/installer/version.iss.in"
               "${CMAKE_CURRENT_SOURCE_DIR}/installer/version.iss" @ONLY)

# --- Executable -----------------------------------------------------------
# The WIN32 flag makes this a GUI app (no console window pops up on launch).
qt_add_executable(PinIt WIN32
    src/main.cpp
    src/winpin.h            src/winpin.cpp
    src/persistence.h       src/persistence.cpp
    src/pinmanager.h        src/pinmanager.cpp
    src/globalhotkey.h      src/globalhotkey.cpp
    src/mainwindow.h        src/mainwindow.cpp
    src/logging.h           src/logging.cpp
    src/shortcuts.h         src/shortcuts.cpp
    src/shortcutsdialog.h   src/shortcutsdialog.cpp
    resources/resources.qrc
    resources/app.rc
)

target_link_libraries(PinIt PRIVATE Qt6::Widgets Qt6::Network)

# Win32 APIs we call directly: window mgmt, DWM, registry (autostart).
if(WIN32)
    target_link_libraries(PinIt PRIVATE user32 dwmapi advapi32)
endif()

# Treat the compiler as a code reviewer: build with broad warnings on.
if(MSVC)
    target_compile_options(PinIt PRIVATE /W4)
else()
    target_compile_options(PinIt PRIVATE -Wall -Wextra)
endif()

# --- Tests ----------------------------------------------------------------
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(pinit_tests
    tests/test_pinit.cpp
    src/winpin.h     src/winpin.cpp
    src/shortcuts.h  src/shortcuts.cpp
)
target_include_directories(pinit_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(pinit_tests PRIVATE Qt6::Test Qt6::Core)
if(WIN32)
    target_link_libraries(pinit_tests PRIVATE user32)
endif()
add_test(NAME pinit_tests COMMAND pinit_tests)
