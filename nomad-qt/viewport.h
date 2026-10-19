#pragma once
#include <QElapsedTimer>
#include <QHash>
#include <QPointF>
#include <QQuickFramebufferObject>
#include <QtQml/qqml.h>
#include <vector>

#include "scene.h"

class ViewportItem : public QQuickFramebufferObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Viewport)
    Q_PROPERTY(int tool READ tool WRITE setTool NOTIFY toolChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(int objectCount READ objectCount NOTIFY sceneChanged)

public:
    explicit ViewportItem(QQuickItem *parent = nullptr);
    Renderer *createRenderer() const override;

    int tool() const { return m_tool; }
    void setTool(int t);
    bool canUndo() const { return !m_undo.empty(); }
    bool canRedo() const { return !m_redo.empty(); }
    int objectCount() const { return int(m_scene.objs.size()); }

    const Scene &scene() const { return m_scene; }
    const Camera &camera() const { return m_cam; }

    Q_INVOKABLE void addPrimitive(int type);
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE void newScene();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

signals:
    void toolChanged();
    void historyChanged();
    void sceneChanged();

protected:
    void touchEvent(QTouchEvent *ev) override;
    void touchUngrabEvent() override;

private:
    enum Mode { Idle, Undecided, Orbit, Transform, Gesture };

    void commit(const Scene &before);
    int pickAt(const QPointF &p) const;
    void applyTransform(const QPointF &d);
    void orbit(const QPointF &d);
    void panZoom(const QPointF &midDelta, float zoomRatio);
    float worldPerPixel(float distance) const;

    Scene m_scene;
    Camera m_cam;
    int m_tool = 0;  // 0 mover, 1 rotacionar, 2 escalar
    std::vector<Scene> m_undo, m_redo;

    // estado do toque
    Mode m_mode = Idle;
    QHash<int, QPointF> m_pts;
    QPointF m_press;
    QElapsedTimer m_pressTimer;
    float m_moved = 0.f;
    int m_hit = -1;
    Scene m_before;
    bool m_changed = false;
    float m_lastDist = 0.f;
    QPointF m_lastMid;
};
