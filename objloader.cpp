#include "objloader.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QTextStream>
#include <QVector2D>
#include <QVector3D>

namespace {

// 面里的一个"角"：引用位置、法线、纹理坐标三个索引（-1 表示缺失）
struct Corner
{
    int vi = -1;
    int ni = -1;
    int ti = -1;
};

// 把 OBJ 的 1-based 索引转成 0-based；负数表示从末尾倒数（-1 = 最后一个）
int resolveIndex(int idx, int count)
{
    if (idx > 0) return idx - 1;
    if (idx < 0) return count + idx;
    return -1;
}

// 去重键：位置、法线、纹理坐标三者索引都一样，才算同一个顶点。
// OBJ 允许同一个位置在不同面上配不同 UV（纹理接缝），所以 ti 必须参与去重。
struct VertexKey
{
    int vi = -1;
    int ni = -1;
    int ti = -1;

    bool operator==(const VertexKey &o) const
    {
        return vi == o.vi && ni == o.ni && ti == o.ti;
    }
};

// 让 QHash<VertexKey, int> 能工作：Qt 通过 ADL 找到这个 qHash 重载。
// 注意：本函数叫 qHash 后会“遮蔽”全局的 qHash(int)，所以函数体内不能再裸写
// qHash(k.vi, ...)，否则会又匹配回自己。这里改用手动混合三个 int。
size_t qHash(const VertexKey &k, size_t seed)
{
    size_t h = seed;
    h ^= size_t(k.vi) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= size_t(k.ni) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= size_t(k.ti) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

} // namespace

bool loadObj(const QString &path, QVector<SubMesh> &outSubMeshes,
             QVector<Material> *outMaterials)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QVector<QVector3D> positions;   // v 行
    QVector<QVector3D> normals;     // vn 行
    QVector<QVector2D> texcoords;   // vt 行
    QVector<QVector<Corner>> faces; // f 行
    QVector<QString> faceMaterials; // 每个面对应的材质名（与 faces 一一对应）
    QString currentMaterial;        // 当前 usemtl 指向的材质
    QString mtllibName;             // mtllib 行引用的材质库文件名

    QTextStream in(&file);
    while (!in.atEnd())
    {
        const QString line = in.readLine().simplified();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);

        if (parts[0] == "v" && parts.size() >= 4)
            positions.append(QVector3D(parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat()));
        else if (parts[0] == "vn" && parts.size() >= 4)
            normals.append(QVector3D(parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat()));
        else if (parts[0] == "vt" && parts.size() >= 3)
            texcoords.append(QVector2D(parts[1].toFloat(), parts[2].toFloat())); // 忽略可选的第 3 个分量 w
        else if (parts[0] == "f" && parts.size() >= 4)
        {
            QVector<Corner> face;
            for (int i = 1; i < parts.size(); ++i)
            {
                // 形如 "v/vt/vn"、"v//vn" 或 "v"
                const QStringList c = parts[i].split('/');
                Corner corner;
                corner.vi = c.value(0).toInt();
                if (c.size() >= 2 && !c.value(1).isEmpty())
                    corner.ti = c.value(1).toInt();   // 中间的 vt
                if (c.size() >= 3 && !c.value(2).isEmpty())
                    corner.ni = c.value(2).toInt();   // 第三个 vn
                face.append(corner);
            }
            faces.append(face);
            faceMaterials.append(currentMaterial); // 这个面属于当前 usemtl 的材质
        }
        else if (parts[0] == "usemtl" && parts.size() >= 2)
            currentMaterial = parts[1]; // 之后的 f 行都归这个材质
        else if (parts[0] == "mtllib" && parts.size() >= 2)
            mtllibName = parts[1]; // 记下材质库文件名，循环结束后解析
        // 其它行（o、g...）本阶段忽略
    }

    // 解析 mtllib 引用的材质库（如果有）
    if (outMaterials && !mtllibName.isEmpty())
        loadMtl(QFileInfo(path).absolutePath() + '/' + mtllibName, *outMaterials);

    if (positions.isEmpty())
        return false;

    // 去重、三角化、打包都按材质分组进行：每个 usemtl 段有独立的去重表，
    // 产出一个独立的 SubMesh（不同材质要分别 draw，各自的顶点数组必须自包含）。
    struct Group
    {
        SubMesh mesh;                 // materialName + 最终交错顶点
        QHash<VertexKey, int> indexMap;
        QVector<QVector3D> outPos;
        QVector<QVector3D> outNrm;
        QVector<QVector2D> outUv;
    };
    QVector<Group> groups;
    QHash<QString, int> nameToGroup; // 材质名 → groups 下标

    // 取得（或新建）某材质对应的组
    auto groupFor = [&](const QString &mat) -> Group &
    {
        auto it = nameToGroup.constFind(mat);
        if (it != nameToGroup.constEnd())
            return groups[it.value()];
        const int idx = groups.size();
        nameToGroup.insert(mat, idx);
        groups.append(Group());
        groups[idx].mesh.materialName = mat;
        return groups[idx];
    };

    for (int fi = 0; fi < faces.size(); ++fi)
    {
        Group &g = groupFor(faceMaterials.at(fi));
        const QVector<Corner> &face = faces.at(fi);

        // 解析角索引，非法的面跳过
        QVector<Corner> resolved;
        bool ok = true;
        for (const Corner &c : face)
        {
            const int vi = resolveIndex(c.vi, positions.size());
            if (vi < 0 || vi >= positions.size())
            {
                ok = false;
                break;
            }
            const int ni = resolveIndex(c.ni, normals.size());
            const int ti = resolveIndex(c.ti, texcoords.size());
            resolved.append(Corner{vi, ni, ti});
        }
        if (!ok || resolved.size() < 3)
            continue;

        // 本组内的去重 / 追加（注意都写到 g 里，而不是全局数组）
        auto vertexFor = [&](int vi, int ni, int ti) -> int
        {
            const VertexKey key{vi, ni, ti};
            auto it = g.indexMap.constFind(key);
            if (it != g.indexMap.constEnd())
                return it.value();

            const int idx = g.outPos.size();
            g.outPos.append(positions.at(vi));
            g.outNrm.append((ni >= 0 && ni < normals.size()) ? normals.at(ni) : QVector3D(0, 0, 0));
            g.outUv.append((ti >= 0 && ti < texcoords.size()) ? texcoords.at(ti) : QVector2D(0, 0));
            g.indexMap.insert(key, idx);
            return idx;
        };
        auto appendFlat = [&](int vi, int ti, const QVector3D &faceNormal)
        {
            g.outPos.append(positions.at(vi));
            g.outNrm.append(faceNormal);
            g.outUv.append((ti >= 0 && ti < texcoords.size()) ? texcoords.at(ti) : QVector2D(0, 0));
        };

        // 扇形三角化：多边形 (0,1,2,...,n-1) → 三角形 (0, i, i+1)
        for (int i = 1; i < resolved.size() - 1; ++i)
        {
            const Corner &a = resolved[0];
            const Corner &b = resolved[i];
            const Corner &c = resolved[i + 1];

            // 面法线（用原始位置算，供缺失法线的角做平面着色）
            const QVector3D faceNormal =
                QVector3D::crossProduct(positions.at(b.vi) - positions.at(a.vi),
                                        positions.at(c.vi) - positions.at(a.vi)).normalized();

            if (a.ni >= 0) vertexFor(a.vi, a.ni, a.ti); else appendFlat(a.vi, a.ti, faceNormal);
            if (b.ni >= 0) vertexFor(b.vi, b.ni, b.ti); else appendFlat(b.vi, b.ti, faceNormal);
            if (c.ni >= 0) vertexFor(c.vi, c.ni, c.ti); else appendFlat(c.vi, c.ti, faceNormal);
        }
    }

    // 把每组收集到的顶点打包成交错的 float 数组，装进 outSubMeshes
    outSubMeshes.clear();
    outSubMeshes.reserve(groups.size());
    for (Group &g : groups)
    {
        if (g.outPos.isEmpty())
            continue; // 空组（例如全是不合法面）不产出

        g.mesh.vertices.reserve(g.outPos.size() * 8);
        for (int i = 0; i < g.outPos.size(); ++i)
        {
            g.mesh.vertices.append(g.outPos.at(i).x());
            g.mesh.vertices.append(g.outPos.at(i).y());
            g.mesh.vertices.append(g.outPos.at(i).z());
            g.mesh.vertices.append(g.outNrm.at(i).x());
            g.mesh.vertices.append(g.outNrm.at(i).y());
            g.mesh.vertices.append(g.outNrm.at(i).z());
            g.mesh.vertices.append(g.outUv.at(i).x());
            g.mesh.vertices.append(g.outUv.at(i).y());
        }
        outSubMeshes.append(g.mesh);
    }

    return !outSubMeshes.isEmpty();
}
