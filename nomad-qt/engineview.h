#pragma once
#include <QHash>
#include <QList>
#include <QMutex>
#include <QPointF>
#include <QPointer>
#include <QQuickItem>
#include <QVariantList>
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
    Q_PROPERTY(qreal frame READ frame NOTIFY timelineChanged)
    Q_PROPERTY(int frameStart READ frameStart NOTIFY timelineChanged)
    Q_PROPERTY(int frameEnd READ frameEnd NOTIFY timelineChanged)
    Q_PROPERTY(int viewStart READ viewStart NOTIFY timelineChanged)
    Q_PROPERTY(int viewEnd READ viewEnd NOTIFY timelineChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY timelineChanged)
    Q_PROPERTY(int playDir READ playDir NOTIFY timelineChanged)
    Q_PROPERTY(bool timelineVisible READ timelineVisible NOTIFY timelineChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY timelineChanged)
    Q_PROPERTY(QVariantList keyFrames READ keyFrames NOTIFY keysChanged)
    Q_PROPERTY(int toolIndex READ toolIndex NOTIFY timelineChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY timelineChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY timelineChanged)
    Q_PROPERTY(bool sculptMode READ sculptMode NOTIFY timelineChanged)
    Q_PROPERTY(bool anyKeys READ anyKeys NOTIFY timelineChanged)
    Q_PROPERTY(int keyFirst READ keyFirst NOTIFY timelineChanged)
    Q_PROPERTY(int keyLast READ keyLast NOTIFY timelineChanged)
    Q_PROPERTY(bool rendering READ rendering NOTIFY timelineChanged)
    Q_PROPERTY(qreal renderProgress READ renderProgress NOTIFY timelineChanged)
    Q_PROPERTY(QString previewUrl READ previewUrl NOTIFY timelineChanged)

public:
    explicit EngineView(QQuickItem *parent = nullptr);
    static EngineView *instance();
    void showKeyboard();  // thread da interface
    void hideKeyboard();

    qreal frame() const { return m_tl.frame; }
    int frameStart() const { return m_tl.fs; }
    int frameEnd() const { return m_tl.fe; }
    int viewStart() const { return m_tl.vs; }
    int viewEnd() const { return m_tl.ve; }
    bool playing() const { return m_tl.playing; }
    int playDir() const { return m_tl.dir; }
    bool timelineVisible() const { return m_tl.visible; }
    bool hasSelection() const { return m_tl.hasSel; }
    QVariantList keyFrames() const { return m_keys; }
    int toolIndex() const { return m_tl.tool; }
    bool canUndo() const { return m_tl.canUndo; }
    bool canRedo() const { return m_tl.canRedo; }
    bool sculptMode() const { return m_tl.sculpt; }
    bool anyKeys() const { return m_tl.anyKeys; }
    int keyFirst() const { return m_tl.keyFirst; }
    int keyLast() const { return m_tl.keyLast; }
    bool rendering() const { return m_tl.rendering; }
    qreal renderProgress() const { return m_tl.progress; }
    QString previewUrl() const {
        if (m_tl.previewPath.empty()) return QString();
        return QStringLiteral("file://") + QString::fromStdString(m_tl.previewPath) + QStringLiteral("?") + QString::number(m_tl.previewSeq);
    }

    // Comandos da timeline (executados na thread de render):
    // 0 ir ao inicio, 1 reproduzir ao contrario, 2 parar, 3 reproduzir, 4 ir ao fim,
    // 5 key anterior, 6 proxima key, 7 inserir key, 8 apagar key, 9 ir ao frame a, 10 janela visivel a..b,
    // 11 aperta o botao do motor com id a (ferramentas 3..6, adicionar 0/1/2/10/18, desfazer 91, refazer 92, menu 60)
    Q_INVOKABLE void command(int cmd, qreal a = 0, qreal b = 0);
    // true se (x, y), em px logicos da janela, esta sobre uma janela/botao desenhado pelo motor
    Q_INVOKABLE bool uiHit(qreal x, qreal y) const;

signals:
    void timelineChanged();
    void keysChanged();
    void renderRequested();

protected:
    void touchEvent(QTouchEvent *ev) override;
    void touchUngrabEvent() override;
    void keyPressEvent(QKeyEvent *ev) override;
    void inputMethodEvent(QInputMethodEvent *ev) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery q) const override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void onWindowChanged(QQuickWindow *w);
    void renderEngine();    // thread de render, antes do Qt Quick desenhar (cena 3D)
    void renderEngineUi();  // thread de render, depois do Qt Quick (interface do motor)
    void glLost();
    void updateSize();
    void applyTimeline(const EngTimeline &t);
    void push(const EngEvent &e);
    EngEvent makeTouch(int action) const;

    std::string m_dir;
    EngTimeline m_tl;      // thread da interface
    EngTimeline m_lastTl;  // thread de render
    QVariantList m_keys;
    mutable QMutex m_rectMutex;
    std::vector<float> m_rects;
    QMutex m_mutex;
    std::vector<EngEvent> m_queue;
    std::atomic<int> m_pw{0}, m_ph{0};

    // estado dos dedos (ordem de chegada, como no Android)
    QList<int> m_ids;
    QHash<int, QPointF> m_pos;
};
