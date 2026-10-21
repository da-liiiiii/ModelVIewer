#pragma once

#include <QMainWindow>

class GLWidget;

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

private:
    GLWidget *m_glWidget = nullptr;
};
