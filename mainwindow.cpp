#include "mainwindow.h"

#include <QAction>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QListWidget>
#include <QMenu>
#include <QStatusBar>
#include <QToolBar>

#include "glwidget.h"
#include "model.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("ModelViewer");
    resize(1200, 800);

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

    // 右侧“属性”面板：显示/编辑选中模型的位置、旋转、缩放
    QDockWidget *propDock = new QDockWidget("属性", this);
    m_propertyPanel = new QWidget(propDock);
    QFormLayout *form = new QFormLayout(m_propertyPanel);

    // 小工具：建一个统一配置的数值输入框（范围、步长、小数位、关闭逐字符触发）
    auto makeSpin = [](double min, double max, double step, int decimals)
    {
        QDoubleSpinBox *s = new QDoubleSpinBox;
        s->setRange(min, max);
        s->setSingleStep(step);
        s->setDecimals(decimals);
        s->setKeyboardTracking(false);   // 回车/失焦/箭头才发 valueChanged，避免输入一半就写回
        return s;
    };

    const char *axisNames[3] = { "X", "Y", "Z" };
    for (int i = 0; i < 3; ++i)
    {
        m_posSpin[i] = makeSpin(-1e6, 1e6, 0.1, 2);
        form->addRow(QString("位置 %1").arg(axisNames[i]), m_posSpin[i]);
    }
    for (int i = 0; i < 3; ++i)
    {
        m_rotSpin[i] = makeSpin(-1e6, 1e6, 1.0, 1);   // 旋转显示成欧拉角（度）
        form->addRow(QString("旋转 %1").arg(axisNames[i]), m_rotSpin[i]);
    }
    for (int i = 0; i < 3; ++i)
    {
        m_scaleSpin[i] = makeSpin(-1e6, 1e6, 0.1, 2);
        form->addRow(QString("缩放 %1").arg(axisNames[i]), m_scaleSpin[i]);
    }

    propDock->setWidget(m_propertyPanel);
    addDockWidget(Qt::RightDockWidgetArea, propDock);

    // 9 个输入框改值 → 写回选中模型（valueChanged 有重载，用 QOverload 指定 double 版本）
    for (int i = 0; i < 3; ++i)
    {
        connect(m_posSpin[i], QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &MainWindow::onTransformEdited);
        connect(m_rotSpin[i], QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &MainWindow::onTransformEdited);
        connect(m_scaleSpin[i], QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &MainWindow::onTransformEdited);
    }

    // 模型增删/重载 → 刷新列表
    connect(m_glWidget, &GLWidget::modelsChanged, this, &MainWindow::refreshModelList);
    // 视口点选模型 → 列表高亮对应项
    connect(m_glWidget, &GLWidget::selectionChanged, this, &MainWindow::onGLSelectionChanged);
    // 视口拖拽/Gizmo 旋转改变变换 → 属性面板跟着刷新（第 6 步：反向同步）
    connect(m_glWidget, &GLWidget::modelTransformEdited, this, &MainWindow::refreshPropertyPanel);
    connect(m_modelList, &QListWidget::itemClicked, this, &MainWindow::onListSelectionChanged);
    // 右键菜单：删除模型
    m_modelList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_modelList, &QListWidget::customContextMenuRequested,
            this, &MainWindow::onModelListContextMenu);
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
    refreshPropertyPanel();   // 模型增删后选中可能失效，属性面板也要刷新
}

void MainWindow::onGLSelectionChanged(int index)
{
    m_modelList->setCurrentRow(index);
    refreshPropertyPanel();   // 选中变了，属性面板跟着刷新
}

void MainWindow::onListSelectionChanged(QListWidgetItem *item)
{
    if(!item)
        return;
    m_glWidget->changeSelection(m_modelList->row(item));
    refreshPropertyPanel();   // 选中变了，属性面板跟着刷新
}

void MainWindow::refreshPropertyPanel()
{
    m_syncingPanel = true;   // 挡住下面 setValue 触发的 valueChanged，避免"读值"又被当成"改值"写回

    Model *m = m_glWidget->selectedModel();
    if (!m)
    {
        for (int i = 0; i < 3; ++i)
        {
            m_posSpin[i]->clear();
            m_rotSpin[i]->clear();
            m_scaleSpin[i]->clear();
        }
        m_propertyPanel->setEnabled(false);   // 没选中：整块面板置灰
        m_syncingPanel = false;
        return;
    }

    m_propertyPanel->setEnabled(true);

    const QVector3D p = m->position();
    const QVector3D r = m->rotation().toEulerAngles();  // 四元数 → 欧拉角（度）
    const QVector3D s = m->scale();

    m_posSpin[0]->setValue(p.x()); m_posSpin[1]->setValue(p.y()); m_posSpin[2]->setValue(p.z());
    m_rotSpin[0]->setValue(r.x()); m_rotSpin[1]->setValue(r.y()); m_rotSpin[2]->setValue(r.z());
    m_scaleSpin[0]->setValue(s.x()); m_scaleSpin[1]->setValue(s.y()); m_scaleSpin[2]->setValue(s.z());

    m_syncingPanel = false;
}

void MainWindow::onTransformEdited()
{
    if (m_syncingPanel)
        return;   // 正在刷新面板，忽略（避免 setValue 引起的写回）

    Model *m = m_glWidget->selectedModel();
    if (!m)
        return;

    m->setPosition(QVector3D(m_posSpin[0]->value(), m_posSpin[1]->value(), m_posSpin[2]->value()));
    // 旋转：面板上是欧拉角（度），转回四元数再存
    m->setRotation(QQuaternion::fromEulerAngles(
        QVector3D(m_rotSpin[0]->value(), m_rotSpin[1]->value(), m_rotSpin[2]->value())));
    m->setScale(QVector3D(m_scaleSpin[0]->value(), m_scaleSpin[1]->value(), m_scaleSpin[2]->value()));

    m_glWidget->update();   // 变换改了，请求重绘
}

void MainWindow::onModelListContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_modelList->itemAt(pos);
    if (!item)
        return;   // 点在列表空白处：不弹菜单

    QMenu menu(this);
    QAction *delAction = menu.addAction("删除模型");
    QAction *chosen = menu.exec(m_modelList->viewport()->mapToGlobal(pos));
    if (chosen == delAction)
        m_glWidget->removeModel(m_modelList->row(item));
}
