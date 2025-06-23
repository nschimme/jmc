cmake_minimum_required(VERSION 3.16)
project(JMCQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)
qt_standard_project_setup()

# Enable C++17 if not already default (redundant with above but good practice)
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(NOT CMAKE_CXX_STANDARD_REQUIRED)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()


# Automatically run moc, uic, and rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Define the executable
add_executable(JMCQt
    src/main.cpp
    src/application.cpp
    src/application.h
    src/mainwindow.cpp
    src/mainwindow.h
    src/profilemanager.h
    src/profilemanager.cpp
    src/ansiviewwidget.h
    src/ansiviewwidget.cpp
    src/inputbarwidget.h
    src/inputbarwidget.cpp
)

# Link against Qt libraries
target_link_libraries(JMCQt PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

# If you have resources (icons, etc.)
# qt_add_resources(JMCQt "resources.qrc")


# For macOS, create an application bundle
if(APPLE)
    set_target_properties(JMCQt PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist" # You might need to create this
    )
endif()

# Installation (optional, for packaging)
# install(TARGETS JMCQt
#     BUNDLE DESTINATION .  # For macOS
#     RUNTIME DESTINATION bin # For Windows/Linux
# )

# For MSVC, ensure correct runtime library if needed (usually handled by Qt)
# if(MSVC)
#   target_compile_options(JMCQt PRIVATE "/MD$<$<CONFIG:Debug>:d>")
# endif()

message(STATUS "JMCQt project configured for Qt ${Qt6_VERSION}")
