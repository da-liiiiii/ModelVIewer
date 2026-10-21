#include "mainwindow.h"

#include <QAction>
#include <QDockWidget>
#include <QFileDialog>
#include <QListWidget>
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
    QAction *resetAction = new QAction("重置视角", this);
    connect(resetAction, &QAction::triggered, this, &MainWindow::onResetCameraStatus);


    QToolBar *toolBar = addToolBar("主工具栏");
    
    toolBar->addAction(openAction);
    toolBar->addAction(addAction);
    toolBar->addAction(resetAction);

    // 左侧“模型列表”停靠面板（第 1 步：只读，点击选中留到下一步）
    QDockWidget *listDock = new QDockWidget("模型列表", this);
    m_modelList = new QListWidget(listDock);
    listDock->setWidget(m_modelList);
    addDockWidget(Qt::LeftDockWidgetArea, listDock);

    // 模型增删/重载 → 刷新列表
    connect(m_glWidget, &GLWidget::modelsChanged, this, &MainWindow::refreshModelList);
    // 视口点选模型 → 列表高亮对应项
    connect(m_glWidget, &GLWidget::selectionChanged, this, &MainWindow::onGLSelectionChanged);
    connect(m_modelList, &QListWidget::itemClicked, this, &MainWindow::onListSelectionChanged);
    refreshModelList(); // 启动先刷一次（此时默认立方体还没建，列表为空正常）

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

void MainWindow::refreshModelList()
{
    m_modelList->clear();
    m_modelList->addItems(m_glWidget->modelNames());
    m_modelList->setCurrentRow(m_glWidget->selectedIndex()); // 重建后恢复高亮
}

void MainWindow::onGLSelectionChanged(int index)
{
    m_modelList->setCurrentRow(index);
}

void MainWindow::onListSelectionChanged(QListWidgetItem *item)
{
    if(!item)
        return;
    m_glWidget->changeSelection(m_modelList->row(item));
}
