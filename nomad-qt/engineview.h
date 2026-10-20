#pragma once
#include <QHash>
#include <QList>
#include <QMutex>
#include <QPointF>
#include <QPointer>
#include <QQuickItem>
#include <QtQml/qqml.h>
#include <atomic>
#include <string>
#include <vector>

#include "engine_api.h"

class QQuickWindow;

// Item QML que hospeda o motor (OpenGL ES) por baixo do Qt Quick e traduz
// toques/teclado do Qt para o motor.
class EngineView : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(EngineView)

public:
    explicit EngineView(QQuickItem *parent = nullptr);
    static EngineView *instance();
    void showKeyboard();  // thread da interface
    void hideKeyboard();

protected:
    void touchEvent(QTouchEvent *ev) override;
    void touchUngrabEvent() override;
    void keyPressEvent(QKeyEvent *ev) override;
    void inputMethodEvent(QInputMethodEvent *ev) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery q) const override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void onWindowChanged(QQuickWindow *w);
    void renderEngine();  // thread de render, antes do Qt Quick desenhar
    void glLost();
    void updateSize();
    void push(const EngEvent &e);
    EngEvent makeTouch(int action) const;

    std::string m_dir;
    QMutex m_mutex;
    std::vector<EngEvent> m_queue;
    std::atomic<int> m_pw{0}, m_ph{0};

    // estado dos dedos (ordem de chegada, como no Android)
    QList<int> m_ids;
    QHash<int, QPointF> m_pos;
};
