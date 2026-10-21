#include "model.h"

QMatrix4x4 Model::getMatrix() 
{
    QMatrix4x4 m;                // 从单位矩阵开始，杜绝累积
    m.translate(m_position);                     // m = T
    m.rotate(m_rotation.x(), 1.0f, 0.0f, 0.0f);  // m = T·Rx（绕 X，俯仰）
    m.rotate(m_rotation.y(), 0.0f, 1.0f, 0.0f);  // m = T·Rx·Ry（绕 Y，偏航）
    m.rotate(m_rotation.z(), 0.0f, 0.0f, 1.0f);  // m = T·Rx·Ry·Rz（绕 Z，滚转）
    m.scale(m_scale);                            // m = T·R·S
    return m;
}