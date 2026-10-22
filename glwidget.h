#pragma once

#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>
#include <vector>

#include "mesh.h"
#include "mtlloader.h"
#include "model.h"

class QMouseEvent;
class QWheelEvent;

// 旋转 Gizmo 的一根轴：一个单位半径圆环 Mesh + 它代表的世界轴 + 显示颜色。
// 画的时候用 uM 平移到模型中心并缩放，所以这里只存"单位半径"的几何。
struct GizmoAxis
{
    QVector3D axis;               // 世界空间单位轴（绕它旋转）
    QVector3D color;              // 显示颜色（X红 / Y绿 / Z蓝）
    std::unique_ptr<Mesh> mesh;   // 圆环（GL_LINES）
};

class GLWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit GLWidget(QWidget *parent = nullptr);
    ~GLWidget() override;

    // 加载 OBJ 模型（可在运行时反复调用，例如点击“打开模型”按钮后）
    bool loadModel(const QString &path);
    void resetCameraStatus();
    bool addModel(const QString &path);

    // 供主窗口“模型列表”面板读取模型名（第 1 步：只读列表）
    QStringList modelNames() const;
    int selectedIndex() const { return m_selectedIndex; }
    void changeSelection(int index);
    void removeModel(int index);   // 删除指定下标的模型（供主窗口"模型列表"右键删除）
    Model *selectedModel() const;  // 当前选中模型（供右侧"属性"面板读写变换）；无选中返回 nullptr

signals:
    void modelsChanged();             // 模型增删/重载后发出 → 主窗口刷新列表
    void selectionChanged(int index); // 选中变化（-1 = 取消选中）
    void modelTransformEdited();      // 视口拖拽/Gizmo 旋转改了选中模型变换 → 主窗口刷新属性面板

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // 鼠标交互：左键拖拽旋转、滚轮缩放
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QOpenGLShaderProgram m_program;
    // 模型可能含多种材质 → 多个子网格。QOpenGLBuffer/VAO 不可拷贝，
    // 所以用 unique_ptr 持有 Mesh。这里用 std::vector 而不是 QVector：
    // QVector 是隐式共享容器，非 const 访问会触发“写时复制”需要拷贝元素，
    // 而 unique_ptr 不可拷贝 → 编译错误；std::vector 无隐式共享，天然支持 move-only。
    //std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::vector<std::unique_ptr<Model>> m_models;
    int m_selectedIndex = -1; // 当前选中的模型下标；-1 = 未选中
    int m_activeGizmoAxis = -1; // 正在拖拽的 Gizmo 轴（0/1/2）；-1 = 没在拖 Gizmo
    QVector<Material> m_materials; // 最近一次加载的材质表（阶段 4 查贴图用）
    QMatrix4x4 m_projection;
    QMatrix4x4 view;

    // 按材质名查材质；查不到返回 nullptr
    const Material *findMaterial(const QString &name) const;

    // 把屏幕像素坐标转成世界空间射线（拾取用）。
    // 返回射线原点（近平面点），outDirection 输出方向（已归一化）。
    QVector3D screenToWorldRay(const QPoint &screenPos, QVector3D &outDirection) const;

    // 把世界坐标投影到屏幕像素（用当前 view/projection）。相机背后的点返回"很远"。
    QPointF worldToScreen(const QVector3D &worldPos) const;

    // 拾取旋转 Gizmo：返回鼠标点中的轴（0=X/1=Y/2=Z），没点中返回 -1。
    int pickGizmoAxis(const QPoint &screenPos) const;

    // 轨道相机参数
    float m_yaw = 0.0f;       // 左右环绕角（度），绕 Y 轴
    float m_pitch = 0.0f;     // 上下俯仰角（度），绕 X 轴
    float m_distance = 5.0f;  // 相机到模型中心的距离
    QVector3D m_target;       //轨道中心

    // 光照参数
    QVector3D m_lightPos{2.0f, 3.0f, 4.0f};    // 点光源位置（世界空间）
    QVector3D m_lightColor{1.0f, 1.0f, 1.0f};  // 光源颜色（白）

    QPoint m_lastMousePos;    // 上次鼠标位置，用于计算拖动增量

    GizmoAxis m_gizmoAxes[3]; // 旋转 Gizmo 的三根轴（X/Y/Z），initializeGL 里建好

    std::unique_ptr<Mesh> m_gridMesh; // 地面网格（XZ 平面线框，initializeGL 里建好）
};
