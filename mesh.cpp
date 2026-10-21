#include "mesh.h"

#include <QImage>

void Mesh::upload(QOpenGLFunctions_3_3_Core *gl, const QVector<float> &data)
{
    // 先释放旧资源（若有），避免重复上传时泄漏
    destroy();

    m_vertexCount = data.size() / 8; // 每顶点 8 个 float

    m_vao.create();
    m_vao.bind();

    m_vbo.create();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw); // 数据上传一次、画多次
    m_vbo.allocate(data.constData(), data.size() * int(sizeof(float)));

    // 位置属性（location 0）：从第 0 个字节开始，取 3 个，步长 8 个 float
    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                              reinterpret_cast<void *>(0));

    // 法线属性（location 1）：从第 3 个 float 开始
    gl->glEnableVertexAttribArray(1);
    gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                              reinterpret_cast<void *>(3 * sizeof(float)));

    // UV 属性（location 2）：从第 6 个 float 开始，取 2 个
    gl->glEnableVertexAttribArray(2);
    gl->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                              reinterpret_cast<void *>(6 * sizeof(float)));

    m_vao.release();
    m_vbo.release();
}

void Mesh::destroy()
{
    m_vbo.destroy();
    m_vao.destroy();
    // 注意：这里绝不能 m_texture.reset()。
    // 因为 upload() 开头会先调用 destroy() 清理旧的 VBO/VAO，
    // 而 loadModel 的顺序是“先 loadTexture() 再 upload()”，
    // 若 destroy() 把贴图也清掉，刚加载的贴图就会在 upload() 里被误删，
    // 结果 hasTexture()==false → 回退到材质色 Kd（白色）→ 整个模型纯白。
    // 贴图的生命周期由 unique_ptr 成员自己管理，Mesh 析构时自动释放即可。
    // m_texture.reset(); // 阶段 4 引入的错误写法，已注释
    m_vertexCount = 0;
}

void Mesh::draw(QOpenGLFunctions_3_3_Core *gl, GLenum m_enum)
{
    if (m_vertexCount == 0)
        return;

    // 有贴图就绑定到纹理单元 0（shader 里 sampler2D uTex 的值已被设为 0）
    if (m_texture)
        m_texture->bind(0);

    m_vao.bind();
    gl->glDrawArrays(m_enum, 0, m_vertexCount);
    m_vao.release();
}

bool Mesh::loadTexture(const QString &path)
{
    QImage img(path);
    if (img.isNull())
        return false;

    // 统一转成 RGBA8888，格式可控
    img = img.convertToFormat(QImage::Format_RGBA8888);
    // 垂直翻转：QImage 像素原点在左上角，OpenGL 纹理坐标原点在左下角，
    // 不翻转贴图会上下颠倒。
    img = img.mirrored();

    m_texture = std::make_unique<QOpenGLTexture>(img, QOpenGLTexture::DontGenerateMipMaps);
    m_texture->setMinificationFilter(QOpenGLTexture::Linear);
    m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
    m_texture->setWrapMode(QOpenGLTexture::ClampToEdge);
    return true;
}
