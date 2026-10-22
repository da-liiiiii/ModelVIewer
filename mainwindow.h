#pragma once

#include <QMainWindow>
#include <QListWidgetItem>
#include <QPoint>

class GLWidget;
class QListWidget;
class QDoubleSpinBox;

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
    void onModelListContextMenu(const QPoint &pos);   // 列表右键菜单（删除模型）
    void refreshPropertyPanel();          // 选中/模型变化后，把变换读进右侧属性面板
    void onTransformEdited();             // 属性面板改值 → 写回选中模型并重绘

private:
    GLWidget *m_glWidget = nullptr;
    QListWidget *m_modelList = nullptr;
    QWidget *m_propertyPanel = nullptr;                                    // 右侧"属性"面板容器
    QDoubleSpinBox *m_posSpin[3] = {nullptr, nullptr, nullptr};             // 位置 X/Y/Z
    QDoubleSpinBox *m_rotSpin[3] = {nullptr, nullptr, nullptr};             // 旋转 X/Y/Z（欧拉角，度）
    QDoubleSpinBox *m_scaleSpin[3] = {nullptr, nullptr, nullptr};           // 缩放 X/Y/Z
    bool m_syncingPanel = false;   // 把模型值写进面板时为 true，挡住 valueChanged 回写
};
