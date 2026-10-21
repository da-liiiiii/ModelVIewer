#include "glwidget.h"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVector2D>
#include <QVector3D>

#include <cmath>
#include <limits>
#include <memory>

#include "objloader.h"

namespace {

const char *kVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uVP;
uniform mat4 uM;

out vec3 vWorldPos; // 世界空间位置，供片元着色器算光照
out vec3 vNormal;   // 世界空间法线
out vec2 vUV;       // 纹理坐标，供片元着色器采样贴图

void main()
{

    vWorldPos = vec3(uM * vec4(aPos, 1.0));
    vNormal = mat3(uM) * aNormal;
    vUV = aUV;
    gl_Position = uVP * uM * vec4(aPos, 1.0);
}
)";

const char *kFragmentShader = R"(
#version 330 core
in vec3 vWorldPos; // 世界空间位置，用于光照
in vec3 vNormal;   // 世界空间法线
in vec2 vUV;       // 纹理坐标，用于采样贴图

uniform vec3 uDiffuseColor; // 材质漫反射颜色 Kd（无贴图时用）
uniform vec3 uLightPos;     // 点光源位置（世界空间）
uniform vec3 uLightColor;   // 光源颜色
uniform vec3 uViewPos;      // 相机位置，用于高光
uniform sampler2D uTex;     // 漫反射贴图
uniform int uHasTexture;    // 1 = 有贴图，0 = 无贴图
uniform int uSelected;      // 1 = 该模型被选中（高亮）

out vec4 fragColor;

void main()
{
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPos - vWorldPos);

    // 环境光（无方向，均匀照亮暗部，避免背光面全黑）
    float ambientStrength = 0.5;
    vec3 ambient = ambientStrength * uLightColor;

    // 漫反射（N·L：法线越朝光源越亮）
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * uLightColor;

    // 高光（Blinn-Phong 半程向量 H = normalize(L + V)）
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 H = normalize(L + V);
    float specularStrength = 0.5;
    float shininess = 32.0;
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specular = specularStrength * spec * uLightColor;

    // 物体颜色：有贴图取贴图 texel，否则取材质色
    vec3 objectColor;
    if (uHasTexture == 1)
        objectColor = texture(uTex, vUV).rgb;
    else
        objectColor = uDiffuseColor;

    vec3 result = (ambient + diffuse + specular) * objectColor;

    // 选中高亮：把结果颜色向暖黄色混合，提示"这个模型被选中"
    if (uSelected == 1)
        result = mix(result, vec3(1.0, 0.8, 0.2), 0.5);

    fragColor = vec4(result, 1.0);

    // === 调试历史（已退役）===
    // 阶段 1：fragColor = vec4(vUV, 0.0, 1.0);     // 直接看 UV
    // 阶段 3：fragColor = vec4(uDiffuseColor, 1.0); // 平涂材质色
}
)";

// 生成一个单位立方体的交错顶点数据（位置 + 面法线），用于启动时的默认显示。
void generateCube(QVector<float> &out)
{
    out.clear();
    out.reserve(36 * 8);

    auto addVertex = [&out](const QVector3D &p, const QVector3D &n, const QVector2D &uv)
    {
        out.append(p.x()); out.append(p.y()); out.append(p.z());
        out.append(n.x()); out.append(n.y()); out.append(n.z());
        out.append(uv.x()); out.append(uv.y());
    };

    // 四边形 → 两个三角形 (0,1,2) 和 (0,2,3)
    auto addFace = [&](const QVector3D &n,
                       const QVector3D &p0, const QVector3D &p1,
                       const QVector3D &p2, const QVector3D &p3)
    {
        // 每个面都铺满整张贴图：四个角固定映射到 UV 的 (0,0)(1,0)(1,1)(0,1)
        const QVector2D uv0(0.0f, 0.0f), uv1(1.0f, 0.0f);
        const QVector2D uv2(1.0f, 1.0f), uv3(0.0f, 1.0f);
        addVertex(p0, n, uv0); addVertex(p1, n, uv1); addVertex(p2, n, uv2);
        addVertex(p0, n, uv0); addVertex(p2, n, uv2); addVertex(p3, n, uv3);
    };

    const QVector3D a(-0.5f, -0.5f,  0.5f);
    const QVector3D b( 0.5f, -0.5f,  0.5f);
    const QVector3D c( 0.5f,  0.5f,  0.5f);
    const QVector3D d(-0.5f,  0.5f,  0.5f);
    const QVector3D e(-0.5f, -0.5f, -0.5f);
    const QVector3D f( 0.5f, -0.5f, -0.5f);
    const QVector3D g( 0.5f,  0.5f, -0.5f);
    const QVector3D h(-0.5f,  0.5f, -0.5f);

    addFace(QVector3D( 0,  0,  1), a, b, c, d); // 前 (+z)
    addFace(QVector3D( 0,  0, -1), f, e, h, g); // 后 (-z)
    addFace(QVector3D(-1,  0,  0), e, a, d, h); // 左 (-x)
    addFace(QVector3D( 1,  0,  0), b, f, g, c); // 右 (+x)
    addFace(QVector3D( 0,  1,  0), d, c, g, h); // 上 (+y)
    addFace(QVector3D( 0, -1,  0), e, f, b, a); // 下 (-y)
}

//生成模型包围盒
QVector<float> generateBox(QVector3D min, QVector3D max)
{
    QVector<float> out;
    out.reserve(24 * 8);

    auto addVertex = [&out](const QVector3D &p)
    {
        out.append(p.x()); out.append(p.y()); out.append(p.z());
        out.append(0); out.append(1); out.append(0);
        out.append(0); out.append(0);
    };

    auto addLine = [&](const QVector3D &p0, const QVector3D &p1)
    {
        addVertex(p0);
        addVertex(p1);
    };

    const QVector3D a(min.x(), min.y(), max.z());
    const QVector3D b( max.x(), min.y(), max.z());
    const QVector3D c( max.x(),  max.y(),  max.z());
    const QVector3D d(min.x(),  max.y(),  max.z());
    const QVector3D e(min.x(), min.y(), min.z());
    const QVector3D f( max.x(), min.y(), min.z());
    const QVector3D g( max.x(),  max.y(), min.z());
    const QVector3D h(min.x(),  max.y(), min.z());

    //12条线段
    addLine(a, b); addLine(b, c); addLine(c, d); addLine(d, a); 
    addLine(e, f); addLine(f, g); addLine(g, h); addLine(h, e); 
    addLine(a, e); addLine(b, f); addLine(c, g); addLine(d, h); 

    return out;
}

// 从交错顶点（每顶点 8 个 float：位置 xyz + 法线 xyz + UV xy）算出局部空间 AABB
void computeAabb(const QVector<SubMesh> &subMeshes, QVector3D &outMin, QVector3D &outMax)
{
    outMin = QVector3D(std::numeric_limits<float>::max(),
                       std::numeric_limits<float>::max(),
                       std::numeric_limits<float>::max());
    outMax = QVector3D(std::numeric_limits<float>::lowest(),
                       std::numeric_limits<float>::lowest(),
                       std::numeric_limits<float>::lowest());
    for (const SubMesh &s : subMeshes)
        for (int i = 0; i + 2 < (int)s.vertices.size(); i += 8)
        {
            const float x = s.vertices[i + 0];
            const float y = s.vertices[i + 1];
            const float z = s.vertices[i + 2];
            outMin.setX(qMin(outMin.x(), x));
            outMin.setY(qMin(outMin.y(), y));
            outMin.setZ(qMin(outMin.z(), z));
            outMax.setX(qMax(outMax.x(), x));
            outMax.setY(qMax(outMax.y(), y));
            outMax.setZ(qMax(outMax.z(), z));
        }
}

// 从交错顶点（每顶点 8 个 float = 位置 xyz + 法线 xyz + UV xy）提取三角面位置。
// 每 3 个连续顶点 = 1 个三角形（加载器已扇形三角化、非索引打包，存储顺序没变）。
void extractTriangles(const QVector<SubMesh> &subMeshes, std::vector<QVector3D> &outTriangles)
{
    for (const SubMesh &s : subMeshes)
        for (int i = 0; i + 2 < (int)s.vertices.size(); i += 8)
            outTriangles.push_back(QVector3D(s.vertices[i], s.vertices[i + 1], s.vertices[i + 2]));
}

// 射线与轴对齐盒（AABB）求交（slab 法）。
// O=射线原点，D=方向（已归一化 → 返回的 t 就是距离），min/max=盒子对角。
// 命中返回 true，近交点距离写进 outT。
bool intersectRayAABB(const QVector3D &O, const QVector3D &D,
                      const QVector3D &min, const QVector3D &max, float &outT)
{
    float tmin = -std::numeric_limits<float>::infinity();  // 进入时刻（越大越晚）
    float tmax =  std::numeric_limits<float>::infinity();  // 离开时刻（越小越早）

    for (int i = 0; i < 3; ++i)   // 依次处理 x / y / z 三组 slab
    {
        if (std::abs(D[i]) < 1e-8f)
        {
            // 该轴方向射线几乎平行：坐标恒定，若不在区间内则永远命中不了
            if (O[i] < min[i] || O[i] > max[i])
                return false;
        }
        else
        {
            float t1 = (min[i] - O[i]) / D[i];
            float t2 = (max[i] - O[i]) / D[i];
            if (t1 > t2)
                std::swap(t1, t2);      // 保证 t1=进入，t2=离开
            tmin = qMax(tmin, t1);      // 进入取"最晚"
            tmax = qMin(tmax, t2);      // 离开取"最早"
            if (tmin > tmax)
                return false;           // 区间空了 → 错过盒子
        }
    }

    if (tmax < 0.0f)
        return false;                        // 盒子整个在射线起点背后

    outT = (tmin >= 0.0f) ? tmin : 0.0f;     // 起点在盒内时，近交点就是起点
    return true;
}

// 射线与三角形求交（Möller–Trumbore）。O=射线原点，D=方向（已归一化 → t 即距离），
// v0/v1/v2=三角形三顶点（逆时针）。命中返回 true，距离写进 outT。
// 原理：解 [ -D, e1, e2 ]·[t,u,v]ᵀ = O-v0，用点/叉乘（=3×3 行列式）展开 Cramer 法则，
//       一次得到距离 t 与重心坐标 u,v；u≥0 且 v≥0 且 u+v≤1 即落在三角形内部。
bool intersectRayTriangle(const QVector3D &O, const QVector3D &D,
                          const QVector3D &v0, const QVector3D &v1, const QVector3D &v2,
                          float &outT)
{
    const QVector3D edge1 = v1 - v0;
    const QVector3D edge2 = v2 - v0;

    const QVector3D h = QVector3D::crossProduct(D, edge2);   // pvec = D × e2
    const float a = QVector3D::dotProduct(edge1, h);          // det = e1 · (D×e2)
    if (std::abs(a) < 1e-8f)
        return false;   // det≈0 → 射线与三角形所在平面平行（或三角形退化）

    const float f = 1.0f / a;
    const QVector3D s = O - v0;                               // tvec = O − v0

    const float u = f * QVector3D::dotProduct(s, h);
    if (u < 0.0f || u > 1.0f)
        return false;   // 交点落在边 e1 的错误一侧

    const QVector3D q = QVector3D::crossProduct(s, edge1);    // qvec = s × e1
    const float v = f * QVector3D::dotProduct(D, q);
    if (v < 0.0f || u + v > 1.0f)
        return false;   // 交点落在边 e2 的错误一侧，或越过 v0→v1 对角边

    const float t = f * QVector3D::dotProduct(edge2, q);
    if (t < 1e-6f)
        return false;   // 交点在射线起点背后（t≤0，几乎为 0）

    outT = t;
    return true;
}

} // namespace

GLWidget::GLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
}

GLWidget::~GLWidget() = default;

bool GLWidget::loadModel(const QString &path)
{
    // 1. 解析 OBJ（纯 CPU，不需要 GL 上下文），顺便解析它引用的 MTL 材质
    QVector<SubMesh> subMeshes;
    QVector<Material> materials;
    if (!loadObj(path, subMeshes, &materials) || subMeshes.isEmpty())
        return false;

    // 2. 上传 GPU。按钮点击时上下文不一定“当前”，所以要先 makeCurrent
    makeCurrent();

    // 先释放旧网格的 GPU 资源，再换新的（重复加载模型时不泄漏）
    for (const auto &model : m_models)
        for(const auto &mesh : model->meshes())
            mesh->destroy();
    m_models.clear();
    m_selectedIndex = -1;   // 旧模型都没了，选中也失效

    m_materials = materials;

    m_models.push_back(std::make_unique<Model>());
    Model *model = m_models.back().get();
    for (const SubMesh &s : subMeshes)
    {
        //m_meshes.push_back(std::make_unique<Mesh>());
        model->meshes().push_back(std::make_unique<Mesh>());
        Mesh *mesh = model->meshes().back().get();

        // 找到该子网格的材质：设置漫反射颜色 + 加载贴图
        const Material *mat = findMaterial(s.materialName);
        if (mat)
        {
            mesh->setDiffuseColor(mat->kd);
            if (!mat->diffuseMap.isEmpty() && !mesh->loadTexture(mat->diffuseMap))
                qWarning() << "[Texture] failed to load:" << mat->diffuseMap;
        }
        

        mesh->upload(this, s.vertices);
    }

    // 步骤 A：算出本模型的局部空间 AABB（替换之前拾取写死的半径）
    QVector3D aabbMin, aabbMax;
    computeAabb(subMeshes, aabbMin, aabbMax);
    model->setAabb(aabbMin, aabbMax);
    qDebug() << "[AABB] min" << aabbMin << "max" << aabbMax;

    // 步骤 1（精确拾取）：把三角面位置提取到 CPU 侧，供后续逐三角形求交
    extractTriangles(subMeshes, model->triangles());
    qDebug() << "[Tris] count =" << model->triangles().size() / 3;

    model->boxMesh() = std::make_unique<Mesh>();
    model->boxMesh()->upload(this, generateBox(aabbMin, aabbMax));

    doneCurrent();

    //update();
    resetCameraStatus();
    return true;
}

void GLWidget::resetCameraStatus()
{
    // TODO(包围盒)：下面四个是写死的初始值。等做了包围盒自动取景后，
    // 应改成恢复到 loadModel 里算出的“本模型 home 值”（如 m_homeTarget/m_homeDistance），
    // 而不是固定回到原点 + 固定距离。
    m_yaw      = 0.0f;
    m_pitch    = 0.0f;
    m_distance = 5.0f;               // 和头文件里 m_distance 的默认值一致
    m_target   = QVector3D(0, 0, 0); // 回到原点
    update();                        // 请求重绘
}

bool GLWidget::addModel(const QString &path)
{
    // 1. 解析 OBJ（纯 CPU，不需要 GL 上下文），顺便解析它引用的 MTL 材质
    QVector<SubMesh> subMeshes;
    QVector<Material> materials;
    if (!loadObj(path, subMeshes, &materials) || subMeshes.isEmpty())
        return false;

    // 2. 上传 GPU。按钮点击时上下文不一定“当前”，所以要先 makeCurrent
    makeCurrent();

    // 注意：这里【不能】像 loadModel 那样销毁/清空旧网格。
    // “添加”是在现有场景上追加；若销毁已有网格的 GPU 资源，
    // 之前的模型会消失（destroy 会把 m_vertexCount 清零，draw 直接跳过）。
    // for (const auto &mesh : m_meshes)
    //     mesh->destroy();   // loadModel 的清理逻辑，addModel 不能要
    // m_meshes.clear();      // 同理，不能清空
    m_materials = materials;

    m_models.push_back(std::make_unique<Model>());
    Model *model = m_models.back().get();
    model->setPosition(QVector3D(0.5, 0.5, 0));
    for (const SubMesh &s : subMeshes)
    {
        //m_meshes.push_back(std::make_unique<Mesh>());
        model->meshes().push_back(std::make_unique<Mesh>());
        Mesh *mesh = model->meshes().back().get();

        // 找到该子网格的材质：设置漫反射颜色 + 加载贴图
        const Material *mat = findMaterial(s.materialName);
        if (mat)
        {
            mesh->setDiffuseColor(mat->kd);
            if (!mat->diffuseMap.isEmpty() && !mesh->loadTexture(mat->diffuseMap))
                qWarning() << "[Texture] failed to load:" << mat->diffuseMap;
        }
        

        mesh->upload(this, s.vertices);
    }

    // 步骤 A：算出本模型的局部空间 AABB（替换之前拾取写死的半径）
    QVector3D aabbMin, aabbMax;
    computeAabb(subMeshes, aabbMin, aabbMax);
    model->setAabb(aabbMin, aabbMax);
    qDebug() << "[AABB] min" << aabbMin << "max" << aabbMax;

    // 步骤 1（精确拾取）：把三角面位置提取到 CPU 侧，供后续逐三角形求交
    extractTriangles(subMeshes, model->triangles());
    qDebug() << "[Tris] count =" << model->triangles().size() / 3;

    model->boxMesh() = std::make_unique<Mesh>();
    model->boxMesh()->upload(this, generateBox(aabbMin, aabbMax));

    doneCurrent();

    update();
    return true;
}

const Material *GLWidget::findMaterial(const QString &name) const
{
    for (const Material &m : m_materials)
        if (m.name == name)
            return &m;
    return nullptr;
}

QVector3D GLWidget::screenToWorldRay(const QPoint &screenPos, QVector3D &outDirection) const
{
    // 1. 屏幕坐标 → NDC。注意 y 翻转：屏幕 y 向下，NDC y 向上。
    const float ndcX = 2.0f * screenPos.x() / width() - 1.0f;
    const float ndcY = 1.0f - 2.0f * screenPos.y() / height();

    // 2. 用 (投影 × 视图) 的逆矩阵，把近/远平面上的点映射回世界空间。
    //    map() 会自动做透视除法（除以 w），等效于 unproject。
    const QMatrix4x4 inv = (m_projection * view).inverted();
    const QVector3D near = inv.map(QVector3D(ndcX, ndcY, -1.0f));
    const QVector3D far  = inv.map(QVector3D(ndcX, ndcY,  1.0f));

    // 3. 射线方向 = 远点 − 近点，归一化；原点取近平面点。
    outDirection = (far - near).normalized();
    return near;
}

void GLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    // 编译并链接着色器
    m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader);
    m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader);
    m_program.link();

    // 默认显示一个立方体，避免启动时黑屏
    QVector<float> cube;
    generateCube(cube);
    m_models.push_back(std::make_unique<Model>());
    m_models.back()->meshes().push_back(std::make_unique<Mesh>());
    m_models.back()->meshes().back()->upload(this, cube);
}

void GLWidget::resizeGL(int w, int h)
{
    m_projection.setToIdentity();
    m_projection.perspective(45.0f, float(w) / float(h), 0.1f, 100.0f);
}

void GLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // === 修改：用球坐标计算相机位置 eye，再用 lookAt 构建视图矩阵 ===
    const float yawRad   = qDegreesToRadians(m_yaw);
    const float pitchRad = qDegreesToRadians(m_pitch);

    QVector3D eye = m_target;
    eye.setX(eye.x() + m_distance * std::cos(pitchRad) * std::sin(yawRad));
    eye.setY(eye.y() + m_distance * std::sin(pitchRad));
    eye.setZ(eye.z() + m_distance * std::cos(pitchRad) * std::cos(yawRad));

    // 关键：lookAt 和 translate/rotate/perspective 一样是“乘法”，会乘到已有矩阵上，
    // 而不是覆盖。view 是成员变量、会跨帧保留，所以每帧必须先 setToIdentity 清零，
    // 否则视图矩阵一帧帧累积，画面会飞走/错乱。
    view.setToIdentity();
    view.lookAt(eye, m_target, QVector3D(0.0f, 1.0f, 0.0f));
    // === 修改结束 ===

    // === 旧代码（保留注释，供对照）===
    // 轨道相机：先后退到距离 m_distance，再按俯仰/左右旋转。
    // 矩阵作用到顶点时从右往左，所以等效于“先转、后移”。
    // view.translate(0.0f, 0.0f, -m_distance);
    // view.rotate(m_pitch, 1.0f, 0.0f, 0.0f); // 上下俯仰（绕 X 轴）
    // view.rotate(m_yaw, 0.0f, 1.0f, 0.0f);   // 左右环绕（绕 Y 轴）
    // view.lookAt(view.column(3).toVector3D(), m_target, QVector3D(0, 1, 0));
    // === 旧代码结束 ===

    const QMatrix4x4 vp = m_projection * view;

    m_program.bind();
    m_program.setUniformValue("uVP", vp);
    m_program.setUniformValue("uLightPos", m_lightPos);     // 点光源位置（世界空间）
    m_program.setUniformValue("uLightColor", m_lightColor); // 光源颜色
    m_program.setUniformValue("uViewPos", eye);             // 相机位置，用于高光
    m_program.setUniformValue("uTex", 0);                   // 贴图固定用纹理单元 0

    // 逐个模型绘制：外层每模型设 uM/uSelected，内层每材质设颜色/贴图
    for (int i = 0; i < int(m_models.size()); ++i)
    {
        const QMatrix4x4 m = m_models[i]->getMatrix();
        m_program.setUniformValue("uM", m);
        m_program.setUniformValue("uSelected", i == m_selectedIndex ? 1 : 0); // 每模型设一次
        if(i == m_selectedIndex)
        {
            //绘制模型包围盒
            m_program.setUniformValue("uDiffuseColor", QVector3D(0.0f, 1.0f, 0.0f)); // 绿色
            m_program.setUniformValue("uHasTexture", 0);

            if (m_models[i]->boxMesh())
                m_models[i]->boxMesh()->draw(this, GL_LINES);

        }

        for (const auto &mesh : m_models[i]->meshes())
        {
            m_program.setUniformValue("uDiffuseColor", mesh->diffuseColor());
            m_program.setUniformValue("uHasTexture", mesh->hasTexture() ? 1 : 0);
            mesh->draw(this, GL_TRIANGLES);
        }
    }
    
    m_program.release();
}

void GLWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_lastMousePos = event->pos();

        // === 步骤 3：点击拾取，遍历所有模型找最近命中 ===
        QVector3D dir;
        const QVector3D origin = screenToWorldRay(event->pos(), dir);

        int selectedIndex = -1;    // -1 = 还没命中任何模型
        float nearestT = 0.0f;     // 记录"最近命中"的距离
        for (int i = 0; i < int(m_models.size()); ++i)
        {
            // AABB 是局部空间的：把世界空间射线逆变换到模型局部空间再求交
            const QMatrix4x4 invModel = m_models[i]->getMatrix().inverted();
            const QVector3D localOrigin = invModel.map(origin);             // 点：含平移
            const QVector3D localDir = invModel.mapVector(dir).normalized(); // 方向：只旋转/缩放

            float t = 0.0f;
            if (intersectRayAABB(localOrigin, localDir,
                                 m_models[i]->aabbMin(), m_models[i]->aabbMax(), t))
            {
                // === 旧写法（保留对照）：AABB 命中就直接选中 ===
                // if (selectedIndex < 0 || t < nearestT) // 命中，且比之前更近
                // {
                //     selectedIndex = i;
                //     nearestT = t;
                // }

                // === 新写法：AABB 只当粗筛，命中后再逐三角形精测 ===
                // 注意：AABB 的 t 是"进入包围盒"的距离，真正的表面在盒内更远处，
                // 所以 bestT 要从 +∞ 起算，不能从 t 起算（t 一定 ≤ 表面距离）。
                float bestT = std::numeric_limits<float>::infinity();
                bool hit = false;
                const std::vector<QVector3D> &tris = m_models[i]->triangles();
                for (int j = 0; j + 2 < (int)tris.size(); j += 3)
                {
                    float tt = 0.0f;
                    if (intersectRayTriangle(localOrigin, localDir,
                                             tris[j], tris[j + 1], tris[j + 2], tt))
                    {
                        if (!hit || tt < bestT)   // 取本模型内最近的三角面
                        {
                            hit = true;
                            bestT = tt;
                        }
                    }
                }
                if (hit && (selectedIndex < 0 || bestT < nearestT))
                {
                    selectedIndex = i;
                    nearestT = bestT;
                }
            }
        }

        m_selectedIndex = selectedIndex;   // 存进成员：供 paintGL 高亮 + 以后移动用
        update();                          // 选中状态变了，立刻请求重绘（否则要等下次拖动才高亮）

        if (selectedIndex >= 0)
            qDebug() << "[Pick] selected model" << selectedIndex << "distance =" << nearestT;
        else
            qDebug() << "[Pick] no model hit";
    }
    if(event->button() == Qt::MiddleButton)
        m_lastMousePos = event->pos();
    if(event->button() == Qt::RightButton)
        m_lastMousePos = event->pos();


}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    // 只有按住左键拖动才旋转
    if ((event->buttons() & Qt::RightButton))
    {
        const QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();

        const float sensitivity = 0.5f; // 每移动 1 像素旋转 0.5 度
        m_yaw   -= delta.x() * sensitivity;
        m_pitch += delta.y() * sensitivity; // 正号：向上拖 = 俯仰角增大

        // 限制俯仰角，避免相机翻过“头顶”导致画面上下颠倒
        if (m_pitch > 89.0f)  m_pitch = 89.0f;
        if (m_pitch < -89.0f) m_pitch = -89.0f;
    }
    if((event->buttons() & Qt::MiddleButton))
    {
        const QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();

        QMatrix4x4 invView = view.inverted();
        QVector3D right = invView.column(0).toVector3D();
        QVector3D up    = invView.column(1).toVector3D();

        // 计算一个像素对应多少世界单位
        float worldPerPixel;
        if (true) {
            worldPerPixel = 2.0f * m_distance * tan(qDegreesToRadians(45 * 0.5f))
                            / height();
        }
        //  else {
        //     worldPerPixel = orthoHeight / viewportHeight;
        // }

        // 鼠标拖动场景：向右拖，场景向右，相机向左
        m_target -= right * delta.x() * worldPerPixel;
        m_target += up    * delta.y() * worldPerPixel;
    }

    //移动选中模型
    if((event->buttons() & Qt::LeftButton) && m_selectedIndex != -1)
    {
        const QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();

        QMatrix4x4 invView = view.inverted();
        QVector3D right = invView.column(0).toVector3D();
        QVector3D up    = invView.column(1).toVector3D();

        float worldPerPixel;
        if (true) {
            worldPerPixel = 2.0f * m_distance * tan(qDegreesToRadians(45 * 0.5f))
                            / height();
        }
        QVector3D newPos = m_models[m_selectedIndex]->position();
        newPos += right * delta.x() * worldPerPixel;   // 右移
        newPos -= up    * delta.y() * worldPerPixel;   // 下移
        m_models[m_selectedIndex]->setPosition(newPos);

    }


    update();
}

void GLWidget::wheelEvent(QWheelEvent *event)
{
    // 滚轮向上（angleDelta().y() > 0）→ 拉近（距离减小）
    const float steps = event->angleDelta().y() / 120.0f; // 120 是滚轮"一格"的标准值
    m_distance *= std::pow(0.9f, steps);                  // 每格缩放 10%，乘性更自然

    m_distance = qBound(0.2f, m_distance, 100.0f);        // 限制范围

    update();
}
