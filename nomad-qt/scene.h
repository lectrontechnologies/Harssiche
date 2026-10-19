#pragma once
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>
#include <cmath>
#include <vector>

struct Obj {
    int type = 0;
    QVector3D pos{0, 0, 0};
    QQuaternion rot;  // identidade
    QVector3D scl{1, 1, 1};
    QMatrix4x4 model() const {
        QMatrix4x4 m;
        m.translate(pos);
        m.rotate(rot);
        m.scale(scl);
        return m;
    }
};

struct Scene {
    std::vector<Obj> objs;
    int sel = -1;
};

struct Camera {
    float yaw = 35.f, pitch = 25.f, dist = 7.f, fov = 45.f;
    QVector3D target{0, 0.4f, 0};

    QVector3D eye() const {
        const float k = float(M_PI) / 180.f;
        float cp = std::cos(pitch * k), sp = std::sin(pitch * k);
        return target + dist * QVector3D(cp * std::sin(yaw * k), sp, cp * std::cos(yaw * k));
    }
    QVector3D forward() const { return (target - eye()).normalized(); }
    QVector3D right() const { return QVector3D::crossProduct(forward(), QVector3D(0, 1, 0)).normalized(); }
    QVector3D up() const { return QVector3D::crossProduct(right(), forward()); }
    QMatrix4x4 view() const {
        QMatrix4x4 m;
        m.lookAt(eye(), target, QVector3D(0, 1, 0));
        return m;
    }
    QMatrix4x4 proj(float aspect) const {
        QMatrix4x4 m;
        m.perspective(fov, aspect, 0.1f, 300.f);
        return m;
    }
};
