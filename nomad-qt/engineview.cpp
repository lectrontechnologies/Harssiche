#include "engineview.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QQuickOpenGLUtils>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTouchEvent>

#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#endif

static QPointer<EngineView> s_instance;

static bool sameTl(const EngTimeline &a, const EngTimeline &b) {
    return a.frame == b.frame && a.fs == b.fs && a.fe == b.fe && a.vs == b.vs && a.ve == b.ve &&
           a.playing == b.playing && a.dir == b.dir && a.visible == b.visible && a.hasSel == b.hasSel &&
           a.tool == b.tool && a.canUndo == b.canUndo && a.canRedo == b.canRedo && a.sculpt == b.sculpt && a.autoKey == b.autoKey &&
           a.anyKeys == b.anyKeys && a.keyFirst == b.keyFirst && a.keyLast == b.keyLast && a.rendering == b.rendering &&
           a.progress == b.progress && a.openRender == b.openRender && a.previewSeq == b.previewSeq &&
           a.keys == b.keys && a.rows == b.rows && a.selObj == b.selObj;
}

EngineView *EngineView::instance() { return s_instance.data(); }

EngineView::EngineView(QQuickItem *parent) : QQuickItem(parent) {
    s_instance = this;
    setAcceptTouchEvents(true);
    setAcceptedMouseButtons(Qt::NoButton);
    setFlag(QQuickItem::ItemAcceptsInputMethod, true);
    setActiveFocusOnTab(true);

#ifdef Q_OS_ANDROID
    // Mesma pasta de dados que o app antigo usava (getExternalFilesDir).
    static QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (activity.isValid()) {
        eng_set_android(QJniEnvironment::javaVM(), activity.object());
        QJniObject f = activity.callObjectMethod("getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;",
                                                 static_cast<jstring>(nullptr));
        if (f.isValid()) {
            QJniObject p = f.callObjectMethod("getAbsolutePath", "()Ljava/lang/String;");
            if (p.isValid()) m_dir = p.toString().toStdString();
        }
    }
#endif
    if (m_dir.empty()) m_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();

    connect(this, &QQuickItem::windowChanged, this, &EngineView::onWindowChanged);
}

void EngineView::onWindowChanged(QQuickWindow *w) {
    if (!w) return;
    connect(w, &QQuickWindow::beforeRenderPassRecording, this, &EngineView::renderEngine, Qt::DirectConnection);
    connect(w, &QQuickWindow::afterRenderPassRecording, this, &EngineView::renderEngineUi, Qt::DirectConnection);
    connect(w, &QQuickWindow::sceneGraphInvalidated, this, &EngineView::glLost, Qt::DirectConnection);
    // renderizacao continua: pede outro quadro a cada quadro apresentado
    connect(
        w, &QQuickWindow::frameSwapped, w, [w] { QMetaObject::invokeMethod(w, "update", Qt::QueuedConnection); },
        Qt::DirectConnection);
    w->setColor(Qt::black);
    updateSize();
    w->update();
}

void EngineView::geometryChange(const QRectF &n, const QRectF &o) {
    QQuickItem::geometryChange(n, o);
    updateSize();
}

void EngineView::updateSize() {
    const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
    m_pw = int(width() * dpr + 0.5);
    m_ph = int(height() * dpr + 0.5);
}

// ---------------------------------------------------------------------------
// Thread de render
// ---------------------------------------------------------------------------
void EngineView::renderEngine() {
    QQuickWindow *w = window();
    if (!w) return;
    const int pw = m_pw.load(), ph = m_ph.load();
    if (pw < 2 || ph < 2) return;

    w->beginExternalCommands();
    if (!eng_ready()) eng_init(m_dir.c_str(), pw, ph);
    else eng_resize(pw, ph);

    std::vector<EngEvent> evs;
    {
        QMutexLocker l(&m_mutex);
        evs.swap(m_queue);
    }
    for (const EngEvent &e : evs) eng_event(e);

    eng_frame();
    w->endExternalCommands();
    QQuickOpenGLUtils::resetOpenGLState();

    // estado da timeline -> QML (so quando muda)
    EngTimeline t;
    eng_timeline(t);
    if (!sameTl(t, m_lastTl)) {
        m_lastTl = t;
        QMetaObject::invokeMethod(this, [this, t] { applyTimeline(t); }, Qt::QueuedConnection);
    }
}

// Interface do proprio motor (janelas, paineis, dialogos): desenhada depois do Qt Quick,
// por isso aparece por cima da timeline e das barras em QML.
void EngineView::renderEngineUi() {
    QQuickWindow *w = window();
    if (!w || !eng_ready()) return;
    w->beginExternalCommands();
    eng_frame_ui();
    w->endExternalCommands();
    QQuickOpenGLUtils::resetOpenGLState();

    std::vector<float> r;
    eng_ui_rects(r);
    QMutexLocker l(&m_rectMutex);
    m_rects.swap(r);
}

bool EngineView::uiHit(qreal x, qreal y) const {
    const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
    const float px = float(x * dpr), py = float(y * dpr);
    QMutexLocker l(&m_rectMutex);
    for (size_t i = 0; i + 3 < m_rects.size(); i += 4) {
        const float rx = m_rects[i], ry = m_rects[i + 1], rw = m_rects[i + 2], rh = m_rects[i + 3];
        if (px >= rx && px <= rx + rw && py >= ry && py <= ry + rh) return true;
    }
    return false;
}

void EngineView::applyTimeline(const EngTimeline &t) {
    const bool keysDiff = (t.keys != m_tl.keys);
    const bool rowsDiff = (t.rows != m_tl.rows);
    m_tl = t;
    emit timelineChanged();
    if (t.openRender) emit renderRequested();
    if (keysDiff) {
        m_keys.clear();
        for (float k : t.keys) m_keys.append(double(k));
        emit keysChanged();
    }
    if (rowsDiff) {
        m_rows.clear();
        for (const EngRow &r : t.rows) {
            QVariantMap m;
            m["name"] = QString::fromStdString(r.name);
            m["type"] = r.type;
            m["parent"] = r.parent;
            m["hide"] = r.hide;
            m["nor"] = r.nor;
            m_rows.append(m);
        }
        emit sceneChanged();
    }
}

void EngineView::command(int cmd, qreal a, qreal b) {
    EngEvent e;
    e.kind = 5;
    e.action = cmd;
    e.x[0] = float(a);
    e.x[1] = float(b);
    push(e);
    if (window()) window()->update();
}

void EngineView::glLost() { eng_gl_lost(); }

// ---------------------------------------------------------------------------
// Entrada (thread da interface) -> fila -> thread de render
// ---------------------------------------------------------------------------
void EngineView::push(const EngEvent &e) {
    QMutexLocker l(&m_mutex);
    if (m_queue.size() < 4096) m_queue.push_back(e);
}

EngEvent EngineView::makeTouch(int action) const {
    const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
    EngEvent e;
    e.kind = 0;
    e.action = action;
    e.n = int(m_ids.size());
    for (int i = 0; i < 2 && i < m_ids.size(); ++i) {
        const QPointF p = m_pos.value(m_ids[i]);
        e.x[i] = float(p.x() * dpr);
        e.y[i] = float(p.y() * dpr);
    }
    return e;
}

void EngineView::touchEvent(QTouchEvent *ev) {
    bool moved = false;
    for (const QEventPoint &p : ev->points()) {
        const int id = p.id();
        switch (p.state()) {
        case QEventPoint::Pressed:
            m_ids.append(id);
            m_pos.insert(id, p.position());
            push(makeTouch(m_ids.size() == 1 ? 0 : 5));  // DOWN / POINTER_DOWN
            break;
        case QEventPoint::Updated:
            if (m_pos.contains(id)) {
                m_pos[id] = p.position();
                moved = true;
            }
            break;
        case QEventPoint::Released:
            if (m_pos.contains(id)) {
                m_pos[id] = p.position();
                push(makeTouch(m_ids.size() == 1 ? 1 : 6));  // UP / POINTER_UP
                m_ids.removeAll(id);
                m_pos.remove(id);
            }
            break;
        default:
            break;
        }
    }
    if (moved && !m_ids.isEmpty()) push(makeTouch(2));  // MOVE
    ev->accept();
}

void EngineView::touchUngrabEvent() {
    if (!m_ids.isEmpty()) push(makeTouch(3));  // CANCEL
    m_ids.clear();
    m_pos.clear();
}

void EngineView::keyPressEvent(QKeyEvent *ev) {
    EngEvent e;
    switch (ev->key()) {
    case Qt::Key_Backspace:
        e.kind = 2;
        push(e);
        ev->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        e.kind = 3;
        push(e);
        ev->accept();
        return;
    case Qt::Key_Back:
    case Qt::Key_Escape:
        if (eng_dialog_open()) {
            e.kind = 4;
            push(e);
            ev->accept();
        } else {
            ev->ignore();  // sem dialogo aberto: o Android fecha o app, como antes
        }
        return;
    default:
        break;
    }
    const QString text = ev->text();
    if (!text.isEmpty()) {
        for (uint cp : text.toUcs4()) {
            if (cp < 32 || cp == 127) continue;
            EngEvent t;
            t.kind = 1;
            t.cp = int(cp);
            push(t);
        }
        ev->accept();
        return;
    }
    QQuickItem::keyPressEvent(ev);
}

void EngineView::inputMethodEvent(QInputMethodEvent *ev) {
    for (uint cp : ev->commitString().toUcs4()) {
        EngEvent e;
        if (cp == 10 || cp == 13) {
            e.kind = 3;
        } else if (cp >= 32 && cp != 127) {
            e.kind = 1;
            e.cp = int(cp);
        } else {
            continue;
        }
        push(e);
    }
    ev->accept();
}

QVariant EngineView::inputMethodQuery(Qt::InputMethodQuery q) const {
    switch (q) {
    case Qt::ImEnabled:
        return true;
    case Qt::ImHints:
        return int(Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase | Qt::ImhSensitiveData);
    case Qt::ImSurroundingText:
        return QString();
    case Qt::ImCursorPosition:
    case Qt::ImAnchorPosition:
        return 0;
    default:
        return QVariant();
    }
}

void EngineView::showKeyboard() {
    forceActiveFocus();
    QGuiApplication::inputMethod()->show();
    QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
}

void EngineView::hideKeyboard() { QGuiApplication::inputMethod()->hide(); }

// ---------------------------------------------------------------------------
// Funcoes de plataforma chamadas pelo motor (thread de render)
// ---------------------------------------------------------------------------
void plat_show_keyboard() {
    if (EngineView *v = EngineView::instance())
        QMetaObject::invokeMethod(v, [v] { v->showKeyboard(); }, Qt::QueuedConnection);
}

void plat_hide_keyboard() {
    if (EngineView *v = EngineView::instance())
        QMetaObject::invokeMethod(v, [v] { v->hideKeyboard(); }, Qt::QueuedConnection);
}

void plat_keep_screen_on(bool on) {
#ifdef Q_OS_ANDROID
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([on]() -> QVariant {
        QJniObject act = QNativeInterface::QAndroidApplication::context();
        if (act.isValid()) {
            QJniObject win = act.callObjectMethod("getWindow", "()Landroid/view/Window;");
            if (win.isValid()) win.callMethod<void>(on ? "addFlags" : "clearFlags", "(I)V", jint(128));
        }
        return QVariant();
    });
#else
    Q_UNUSED(on)
#endif
}
