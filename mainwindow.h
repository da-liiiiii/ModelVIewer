#pragma once

#include <QMainWindow>
#include <QListWidgetItem>

class GLWidget;
class QListWidget;

// 主窗口：顶部工具栏 + 中央 OpenGL 画布
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    // “打开模型”按钮的槽：弹出文件框 → 交给 GLWidget 加载
    void openModel();
    void onResetCameraStatus();
    void onAddModel();
    void refreshModelList();               // 模型增删/重载后刷新列表内容
    void onGLSelectionChanged(int index);  // 视口点选 → 列表高亮对应项
    void onListSelectionChanged(QListWidgetItem *item);

private:
    GLWidget *m_glWidget = nullptr;
    QListWidget *m_modelList = nullptr;
};
