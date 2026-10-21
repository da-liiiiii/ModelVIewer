#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QString>
#include <QVector>
#include <QVector3D>

#include <memory>

// Mesh：封装一段"交错顶点数据"（每顶点 8 个 float = 位置 xyz + 法线 xyz + UV xy）
// 的 VAO/VBO，并负责绘制。它不关心数据来自哪个文件格式。
//
// 注意：QOpenGLBuffer/VAO 不可拷贝也不可移动（Q_DISABLE_COPY），所以 Mesh 本身
// 也不能直接放进 QVector<Mesh>，使用时要套一层 std::unique_ptr<Mesh>。
class Mesh
{
public:
    // 上传交错顶点数据到 GPU。data 长度必须是 8 的整数倍。
    // 调用时 OpenGL 上下文必须已激活（在 initializeGL 或 makeCurrent 之后调用）。
    void upload(QOpenGLFunctions_3_3_Core *gl, const QVector<float> &data);

    // 释放 GPU 资源（重复上传前会先调用它，避免泄漏）
    void destroy();

    // 绘制所有三角形（glDrawArrays，非索引）
    void draw(QOpenGLFunctions_3_3_Core *gl, GLenum m_enum);

    int vertexCount() const { return m_vertexCount; }
    bool isEmpty() const { return m_vertexCount == 0; }

    // 该网格的漫反射颜色（阶段 3 起用它给不同材质上色，绘制时设为 uniform）
    QVector3D diffuseColor() const { return m_diffuseColor; }
    void setDiffuseColor(const QVector3D &c) { m_diffuseColor = c; }

    // 加载漫反射贴图（阶段 4）。返回 false 表示图片文件打不开/无效。
    // 调用时 OpenGL 上下文必须已激活。
    bool loadTexture(const QString &path);

    bool hasTexture() const { return m_texture != nullptr; }

private:
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo;
    int m_vertexCount = 0;
    QVector3D m_diffuseColor{0.8f, 0.8f, 0.8f}; // 默认浅灰（无材质时的兜底颜色）
    std::unique_ptr<QOpenGLTexture> m_texture;  // 漫反射贴图（空 = 无贴图）
};
