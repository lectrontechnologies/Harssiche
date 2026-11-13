cmake_minimum_required(VERSION 3.21)
project(Nomad VERSION 0.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Gui)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(Nomad main.cpp)

qt_add_qml_module(Nomad
    URI Nomad
    VERSION 1.0
    QML_FILES
        Main.qml
        Timeline.qml
        Glyph.qml
        SceneOutliner.qml
        TlButton.qml
        TlIcon.qml
        HitMask.qml
        RdField.qml
        RdSeg.qml
        RenderDialog.qml
        TbCell.qml
        TopBars.qml
    RESOURCES
        icons/tb_cube.png
        icons/tb_undo.png
        icons/tb_redo.png
        icons/tb_menu.png
        icons/rd_camera.png
        icons/rd_video.png
        icons/rd_link.png
        icons/rd_close.png
        icons/rd_check.png
        icons/logo.png
    SOURCES
        engineview.h
        engineview.cpp
        vkrender.h
        vkrender.cpp
        engine.cpp
        engine_api.h
        engine_shim.h
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/ImGuizmo.cpp
)

# ImGui (so para os gizmos) + ImGuizmo; as listas de desenho vao para o renderizador Vulkan
target_include_directories(Nomad PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

# Bibliotecas do NDK usadas pelo motor (as mesmas do projeto antigo)
target_link_libraries(Nomad PRIVATE
    Qt6::Quick Qt6::Gui
    android log z mediandk)

if(ANDROID)
    set_target_properties(Nomad PROPERTIES
        QT_ANDROID_PACKAGE_NAME "com.nomad.animator"
        QT_ANDROID_VERSION_CODE 2
        QT_ANDROID_VERSION_NAME "1.1"
        QT_ANDROID_TARGET_SDK_VERSION 34)
    # Manifesto e recursos gerados pelo workflow (paisagem, tela cheia, permissoes)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/android/AndroidManifest.xml")
        set_target_properties(Nomad PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    endif()
endif()
