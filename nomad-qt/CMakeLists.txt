cmake_minimum_required(VERSION 3.21)
project(Nomad VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Gui OpenGL)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(Nomad main.cpp)

qt_add_qml_module(Nomad
    URI Nomad
    VERSION 1.0
    QML_FILES
        Main.qml
        Icon.qml
        IconButton.qml
        NomadMenu.qml
        DarkItem.qml
    SOURCES
        viewport.h
        viewport.cpp
        scene.h
        shaders.h
        meshes.h
        meshes.cpp
)

target_link_libraries(Nomad PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Gui Qt6::OpenGL)

if(ANDROID)
    set_target_properties(Nomad PROPERTIES
        QT_ANDROID_PACKAGE_NAME "com.nomad.animator"
        QT_ANDROID_VERSION_CODE 1
        QT_ANDROID_VERSION_NAME "1.0"
        QT_ANDROID_TARGET_SDK_VERSION 34)
    # Manifesto gerado pelo workflow a partir do template do Qt (orientacao em paisagem)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/android/AndroidManifest.xml")
        set_target_properties(Nomad PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    endif()
endif()
