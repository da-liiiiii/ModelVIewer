#pragma once

#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPoint>
#include <QString>
#include <QVector>
#include <memory>
#include <vector>

#include "mesh.h"
#include "mtlloader.h"
#include "model.h"

class QMouseEvent;
class QWheelEvent;

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

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // 鼠标交互：左键拖拽旋转、滚轮缩放
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
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
    QVector<Material> m_materials; // 最近一次加载的材质表（阶段 4 查贴图用）
    QMatrix4x4 m_projection;
    QMatrix4x4 view;

    // 按材质名查材质；查不到返回 nullptr
    const Material *findMaterial(const QString &name) const;

    // 把屏幕像素坐标转成世界空间射线（拾取用）。
    // 返回射线原点（近平面点），outDirection 输出方向（已归一化）。
    QVector3D screenToWorldRay(const QPoint &screenPos, QVector3D &outDirection) const;

    // 轨道相机参数
    float m_yaw = 0.0f;       // 左右环绕角（度），绕 Y 轴
    float m_pitch = 0.0f;     // 上下俯仰角（度），绕 X 轴
    float m_distance = 5.0f;  // 相机到模型中心的距离
    QVector3D m_target;       //轨道中心

    // 光照参数
    QVector3D m_lightPos{2.0f, 3.0f, 4.0f};    // 点光源位置（世界空间）
    QVector3D m_lightColor{1.0f, 1.0f, 1.0f};  // 光源颜色（白）

    QPoint m_lastMousePos;    // 上次鼠标位置，用于计算拖动增量
};
