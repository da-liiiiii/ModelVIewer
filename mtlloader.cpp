#include "mtlloader.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

bool loadMtl(const QString &path, QVector<Material> &outMaterials)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    outMaterials.clear();

    int current = -1; // 当前正在填充的材质下标（-1 = 还没遇到 newmtl）

    QTextStream in(&file);
    while (!in.atEnd())
    {
        const QString line = in.readLine().simplified();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        const QString &tag = parts[0];

        if (tag == "newmtl" && parts.size() >= 2)
        {
            // 开始一个新材质：追加一个，并记下它的下标
            outMaterials.append(Material());
            current = outMaterials.size() - 1;
            outMaterials[current].name = parts[1];
        }
        else if (current >= 0)
        {
            // 还没遇到任何 newmtl 之前，忽略其它行
            Material &m = outMaterials[current];

            if (tag == "Ka" && parts.size() >= 4)
                m.ka = QVector3D(parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat());
            else if (tag == "Kd" && parts.size() >= 4)
                m.kd = QVector3D(parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat());
            else if (tag == "Ks" && parts.size() >= 4)
                m.ks = QVector3D(parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat());
            else if (tag == "Ns" && parts.size() >= 2)
                m.ns = parts[1].toFloat();
            else if (tag == "map_Kd" && parts.size() >= 2)
                // 贴图路径相对 MTL 文件所在目录，这里拼成绝对路径，方便后面直接加载
                m.diffuseMap = QFileInfo(path).absolutePath() + '/' + parts[1];
            // 其它（d、illum、map_Ks、bump...）本阶段忽略
        }
    }

    return true;
}
