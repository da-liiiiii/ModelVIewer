#pragma once

#include <QString>
#include <QVector>
#include <QVector3D>

// 一个材质（对应 MTL 文件里的一个 newmtl 块）
struct Material
{
    QString name;        // newmtl 后面的名字
    QVector3D ka{0.0f, 0.0f, 0.0f};  // 环境光颜色 Ka
    QVector3D kd{0.8f, 0.8f, 0.8f};  // 漫反射颜色 Kd
    QVector3D ks{0.0f, 0.0f, 0.0f};  // 高光颜色 Ks
    float ns = 0.0f;                 // 高光锐利度 Ns（0~1000）
    QString diffuseMap;              // map_Kd 贴图路径（绝对路径，可能为空）
};

// 解析 Wavefront .mtl 材质库文件，把所有 newmtl 材质填入 outMaterials。
// 返回 false 表示文件无法打开（材质为空不算失败）。
bool loadMtl(const QString &path, QVector<Material> &outMaterials);
