#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QString>
#include <QVector>
#include <QVector3D>
#include <QMatrix4x4>
#include <QQuaternion>

#include <memory>
#include "mesh.h"

class Model
{
public:
    QString name() const { return m_name; };
    void setName(const QString& name) {m_name = name;};
    std::vector<std::unique_ptr<Mesh>>& meshes(){return m_meshes;};
    //void setMesh(std::vector<std::unique_ptr<Mesh>> meshes);
    void setPosition(QVector3D position) {m_position = position;};
    QVector3D position() const { return m_position; };   // 读取位置（拾取/移动时用）
    // 旋转改用四元数（绕任意轴、无万向锁）。旧欧拉角版本见下方注释。
    QQuaternion rotation() const { return m_rotation; };
    void setRotation(const QQuaternion &q) { m_rotation = q; };
    // void setRotation(QVector3D rotation) {m_rotation = rotation;};   // 旧：欧拉角，已退役
    // 绕"世界空间"轴再转 angleDeg 度。四元数左乘 = 绕世界轴；右乘才是绕局部轴。
    void rotateAroundWorldAxis(const QVector3D &axis, float angleDeg)
    { m_rotation = QQuaternion::fromAxisAndAngle(axis, angleDeg) * m_rotation; }
    void setScale(QVector3D scale) {m_scale = scale;};
    QVector3D scale() const { return m_scale; };   // 读取缩放（属性面板显示用）
    // 局部空间 AABB：由顶点位置算出的最小/最大坐标（尚未经过 position/rotation/scale）
    QVector3D aabbMin() const { return m_aabbMin; }
    QVector3D aabbMax() const { return m_aabbMax; }
    void setAabb(const QVector3D &min, const QVector3D &max) { m_aabbMin = min; m_aabbMax = max; }
    std::unique_ptr<Mesh>& boxMesh() { return m_boxMesh; };
    // 三角面数据（局部空间位置，每 3 个 QVector3D = 1 个三角形），供 CPU 精确拾取用
    std::vector<QVector3D>& triangles() { return m_triangles; };
    QMatrix4x4 getMatrix();



private:
    QString m_name;
    std::vector<std::unique_ptr<Mesh>> m_meshes;
    //QMatrix4x4 m_matrix;
    QVector3D m_position = QVector3D(0, 0, 0);
    QQuaternion m_rotation;   // 旋转（四元数，默认构造 = 单位四元数 = 无旋转）
    // QVector3D m_rotation = QVector3D(0, 0, 0);  // 旧：欧拉角，已退役
    QVector3D m_scale = QVector3D(1, 1, 1);
    QVector3D m_aabbMin;
    QVector3D m_aabbMax;
    std::unique_ptr<Mesh> m_boxMesh;
    std::vector<QVector3D> m_triangles;   // 局部空间三角面（每 3 个顶点 = 1 个三角形）

};
