cmake_minimum_required(VERSION 3.21)

# RC enables compiling the Windows .rc (executable icon) via windres.
project(PinIt VERSION 1.0.0 LANGUAGES CXX RC)

# --- C++ / Qt setup -------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)   # process Q_OBJECT classes
set(CMAKE_AUTORCC ON)   # compile .qrc resources
set(CMAKE_AUTOUIC ON)   # compile .ui files (none yet, harmless)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

# Let the resource compiler find icon.ico (referenced relatively from app.rc).
# include_directories feeds both the C++ and RC compilers using each tool's
# correct flag (-I for windres/MinGW, /I for rc.exe/MSVC) — portable across CI.
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/resources")

# --- Executable -----------------------------------------------------------
# The WIN32 flag makes this a GUI app (no console window pops up on launch).
qt_add_executable(PinIt WIN32
    src/main.cpp
    src/winpin.h            src/winpin.cpp
    src/persistence.h       src/persistence.cpp
    src/pinmanager.h        src/pinmanager.cpp
    src/globalhotkey.h      src/globalhotkey.cpp
    src/mainwindow.h        src/mainwindow.cpp
    resources/resources.qrc
    resources/app.rc
)

target_link_libraries(PinIt PRIVATE Qt6::Widgets)

# Win32 APIs we call directly: window mgmt, DWM, registry (autostart).
if(WIN32)
    target_link_libraries(PinIt PRIVATE user32 dwmapi advapi32)
endif()
