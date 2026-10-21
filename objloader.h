#pragma once

#include <QString>
#include <QVector>

#include "mtlloader.h"

// 一个"子网格"：同一材质的一组顶点（OBJ 里一个 usemtl 段）
struct SubMesh
{
    QString materialName;      // 对应的 Material.name（空 = 没有 usemtl 的裸几何）
    QVector<float> vertices;   // 交错顶点：位置 xyz + 法线 xyz + UV xy
};

// 解析 Wavefront .obj 文件，按 usemtl 把面分组成多个子网格。
// outSubMeshes 每个元素对应一段相同材质的几何；outMaterials 非空时还会解析 mtllib。
// 返回 false 表示加载失败（文件不存在 / 无法打开 / 没有顶点）。
bool loadObj(const QString &path, QVector<SubMesh> &outSubMeshes,
             QVector<Material> *outMaterials = nullptr);
