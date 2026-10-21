cmake_minimum_required(VERSION 3.16)
project(ModelViewer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 元对象/资源自动处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 生成 compile_commands.json（VSCode 智能提示/跳转用）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
find_package(OpenGL REQUIRED)

add_executable(ModelViewer
    main.cpp
    resources.qrc
    mainwindow.h
    mainwindow.cpp
    glwidget.h
    glwidget.cpp
    mesh.h
    mesh.cpp
    model.h
    model.cpp
    objloader.h
    objloader.cpp
    mtlloader.h
    mtlloader.cpp
)

target_link_libraries(ModelViewer PRIVATE
    Qt6::Widgets
    Qt6::OpenGLWidgets
    OpenGL::GL
)
