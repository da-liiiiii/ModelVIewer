#include <QApplication>
#include <QFile>
#include <QSurfaceFormat>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 加载全局样式表（QSS）：样式已通过 resources.qrc 编译进可执行文件
    QFile styleFile(":/style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text))
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));

    // 请求 OpenGL 3.3 Core Profile 上下文
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);

    MainWindow window;
    window.show();

    return app.exec();
}
