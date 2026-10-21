#include "mainwindow.h"

#include <QAction>
#include <QFileDialog>
#include <QStatusBar>
#include <QToolBar>

#include "glwidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("ModelViewer");
    resize(900, 680);

    // 中央区域：OpenGL 画布（占据窗口剩下所有空间）
    m_glWidget = new GLWidget(this);
    setCentralWidget(m_glWidget);

    // 工具栏：“打开模型”按钮
    QAction *openAction = new QAction("打开模型", this);
    connect(openAction, &QAction::triggered, this, &MainWindow::openModel);
    QAction *addAction = new QAction("添加模型", this);
    connect(addAction, &QAction::triggered, this, &MainWindow::onAddModel);
    QAction *resetAction = new QAction("复位", this);
    connect(resetAction, &QAction::triggered, this, &MainWindow::onResetCameraStatus);


    QToolBar *toolBar = addToolBar("主工具栏");
    
    toolBar->addAction(openAction);
    toolBar->addAction(addAction);
    toolBar->addAction(resetAction);

    statusBar()->showMessage("就绪");
}

void MainWindow::openModel()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "打开模型", QString(), "OBJ 模型 (*.obj);;所有文件 (*)");

    if (path.isEmpty())
        return; // 用户点了“取消”

    statusBar()->showMessage(path);
    m_glWidget->loadModel(path);
}

void MainWindow::onAddModel()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "打开模型", QString(), "OBJ 模型 (*.obj);;所有文件 (*)");

    if (path.isEmpty())
        return; // 用户点了“取消”

    statusBar()->showMessage(path);
    m_glWidget->addModel(path);
}

void MainWindow::onResetCameraStatus()
{
    m_glWidget->resetCameraStatus();
}
