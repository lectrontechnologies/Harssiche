#include "viewport.h"

#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QQuickOpenGLUtils>
#include <QTouchEvent>
#include <algorithm>
#include <cmath>

#include "meshes.h"
#include "shaders.h"

// ---------------------------------------------------------------------------
// Renderer (thread de render do Qt Quick)
// ---------------------------------------------------------------------------
class ViewportRenderer : public QQuickFramebufferObject::Renderer, protected QOpenGLExtraFunctions {
public:
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override {
        QOpenGLFramebufferObjectFormat f;
        f.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        f.setSamples(4);
        return new QOpenGLFramebufferObject(size, f);
    }

    void synchronize(QQuickFramebufferObject *item) override {
        auto *v = static_cast<ViewportItem *>(item);
        m_scene = v->scene();
        m_cam = v->camera();
    }

    void render() override {
        if (!m_inited) init();
        const QSize sz = framebufferObject()->size();
        glViewport(0, 0, sz.width(), sz.height());
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        glClearColor(0.55f, 0.55f, 0.57f, 1.f);
        glClearDepthf(1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        const float aspect = sz.height() > 0 ? float(sz.width()) / float(sz.height()) : 1.f;
        const QMatrix4x4 V = m_cam.view();
        const QMatrix4x4 PV = m_cam.proj(aspect) * V;

        drawGrid(PV);
        drawMeshes(V, PV);

        QQuickOpenGLUtils::resetOpenGLState();
    }

private:
    struct Gpu {
        GLuint vao = 0, vbo = 0, ibo = 0;
        GLsizei count = 0;
    };

    GLuint compile(GLenum type, const char *src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(s, sizeof(log), nullptr, log);
            qWarning("Shader: %s", log);
        }
        return s;
    }

    GLuint link(const char *vs, const char *fs) {
        GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
        GLuint p = glCreateProgram();
        glAttachShader(p, v);
        glAttachShader(p, f);
        glLinkProgram(p);
        GLint ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetProgramInfoLog(p, sizeof(log), nullptr, log);
            qWarning("Link: %s", log);
        }
        glDeleteShader(v);
        glDeleteShader(f);
        return p;
    }

    void init() {
        initializeOpenGLFunctions();

        m_meshProg = link(kMeshVS, kMeshFS);
        uMVP = glGetUniformLocation(m_meshProg, "uMVP");
        uNrm = glGetUniformLocation(m_meshProg, "uNrmMat");
        uMatcap = glGetUniformLocation(m_meshProg, "uMatcap");
        uTint = glGetUniformLocation(m_meshProg, "uTint");
        uSel = glGetUniformLocation(m_meshProg, "uSel");
        uFlip = glGetUniformLocation(m_meshProg, "uFlip");

        m_lineProg = link(kLineVS, kLineFS);
        lMVP = glGetUniformLocation(m_lineProg, "uMVP");
        lColor = glGetUniformLocation(m_lineProg, "uColor");

        for (int t = 0; t < PrimCount; ++t) {
            MeshData md = makeMesh(t);
            Gpu &g = m_gpu[t];
            glGenVertexArrays(1, &g.vao);
            glBindVertexArray(g.vao);
            glGenBuffers(1, &g.vbo);
            glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
            glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(md.v.size() * sizeof(float)), md.v.data(), GL_STATIC_DRAW);
            glGenBuffers(1, &g.ibo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(md.i.size() * sizeof(uint16_t)), md.i.data(),
                         GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                                  reinterpret_cast<const void *>(3 * sizeof(float)));
            glBindVertexArray(0);
            g.count = GLsizei(md.i.size());
        }

        // grade 20x20 + eixos X e Z
        std::vector<float> gv;
        const int n = 10;
        for (int i = -n; i <= n; ++i) {
            gv.insert(gv.end(), {float(i), 0.f, float(-n), float(i), 0.f, float(n)});
            gv.insert(gv.end(), {float(-n), 0.f, float(i), float(n), 0.f, float(i)});
        }
        m_gridCount = GLsizei(gv.size() / 3);
        gv.insert(gv.end(), {-float(n), 0.f, 0.f, float(n), 0.f, 0.f});  // eixo X
        gv.insert(gv.end(), {0.f, 0.f, -float(n), 0.f, 0.f, float(n)});  // eixo Z
        glGenVertexArrays(1, &m_gridVao);
        glBindVertexArray(m_gridVao);
        glGenBuffers(1, &m_gridVbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_gridVbo);
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(gv.size() * sizeof(float)), gv.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glBindVertexArray(0);

        // matcap
        const int N = 256;
        std::vector<uint8_t> px = makeMatcap(N);
        glGenTextures(1, &m_tex);
        glBindTexture(GL_TEXTURE_2D, m_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        m_inited = true;
    }

    void drawGrid(const QMatrix4x4 &PV) {
        glUseProgram(m_lineProg);
        glUniformMatrix4fv(lMVP, 1, GL_FALSE, PV.constData());
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(m_gridVao);
        glUniform4f(lColor, 0.30f, 0.30f, 0.33f, 0.45f);
        glDrawArrays(GL_LINES, 0, m_gridCount);
        glUniform4f(lColor, 0.85f, 0.30f, 0.30f, 0.90f);
        glDrawArrays(GL_LINES, m_gridCount, 2);
        glUniform4f(lColor, 0.30f, 0.45f, 0.90f, 0.90f);
        glDrawArrays(GL_LINES, m_gridCount + 2, 2);
        glBindVertexArray(0);
        glDisable(GL_BLEND);
    }

    void drawMeshes(const QMatrix4x4 &V, const QMatrix4x4 &PV) {
        glUseProgram(m_meshProg);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_tex);
        glUniform1i(uMatcap, 0);
        glUniform3f(uTint, 1.f, 1.f, 1.f);
        for (size_t i = 0; i < m_scene.objs.size(); ++i) {
            const Obj &o = m_scene.objs[i];
            if (o.type < 0 || o.type >= PrimCount) continue;
            const QMatrix4x4 M = o.model();
            const QMatrix4x4 MVP = PV * M;
            const QMatrix3x3 NM = (V * M).normalMatrix();
            glUniformMatrix4fv(uMVP, 1, GL_FALSE, MVP.constData());
            glUniformMatrix3fv(uNrm, 1, GL_FALSE, NM.constData());
            glUniform1f(uSel, int(i) == m_scene.sel ? 1.f : 0.f);
            glUniform1f(uFlip, o.type == PrimPlane ? 1.f : 0.f);
            const Gpu &g = m_gpu[o.type];
            glBindVertexArray(g.vao);
            glDrawElements(GL_TRIANGLES, g.count, GL_UNSIGNED_SHORT, nullptr);
        }
        glBindVertexArray(0);
    }

    bool m_inited = false;
    Scene m_scene;
    Camera m_cam;
    GLuint m_meshProg = 0, m_lineProg = 0, m_tex = 0, m_gridVao = 0, m_gridVbo = 0;
    GLsizei m_gridCount = 0;
    GLint uMVP = -1, uNrm = -1, uMatcap = -1, uTint = -1, uSel = -1, uFlip = -1, lMVP = -1, lColor = -1;
    Gpu m_gpu[PrimCount];
};

// ---------------------------------------------------------------------------
// Item QML (thread da interface): cena, camera, toques, desfazer/refazer
// ---------------------------------------------------------------------------
ViewportItem::ViewportItem(QQuickItem *parent) : QQuickFramebufferObject(parent) {
    setAcceptTouchEvents(true);
    setAcceptedMouseButtons(Qt::NoButton);
    Obj cube;
    cube.type = PrimCube;
    cube.pos = QVector3D(0, 0.5f, 0);
    m_scene.objs.push_back(cube);
}

QQuickFramebufferObject::Renderer *ViewportItem::createRenderer() const {
    return new ViewportRenderer;
}

void ViewportItem::setTool(int t) {
    t = std::clamp(t, 0, 2);
    if (t == m_tool) return;
    m_tool = t;
    emit toolChanged();
}

void ViewportItem::commit(const Scene &before) {
    m_undo.push_back(before);
    if (m_undo.size() > 100) m_undo.erase(m_undo.begin());
    m_redo.clear();
    emit historyChanged();
    emit sceneChanged();
    update();
}

void ViewportItem::addPrimitive(int type) {
    if (type < 0 || type >= PrimCount) return;
    const Scene before = m_scene;
    float he[3];
    primHalfExtents(type, he);
    Obj o;
    o.type = type;
    o.pos = QVector3D(m_cam.target.x(), he[1], m_cam.target.z());
    m_scene.objs.push_back(o);
    m_scene.sel = int(m_scene.objs.size()) - 1;
    commit(before);
}

void ViewportItem::deleteSelected() {
    if (m_scene.sel < 0 || m_scene.sel >= int(m_scene.objs.size())) return;
    const Scene before = m_scene;
    m_scene.objs.erase(m_scene.objs.begin() + m_scene.sel);
    m_scene.sel = -1;
    commit(before);
}

void ViewportItem::newScene() {
    if (m_scene.objs.empty()) return;
    const Scene before = m_scene;
    m_scene.objs.clear();
    m_scene.sel = -1;
    commit(before);
}

void ViewportItem::undo() {
    if (m_undo.empty()) return;
    m_redo.push_back(m_scene);
    m_scene = m_undo.back();
    m_undo.pop_back();
    emit historyChanged();
    emit sceneChanged();
    update();
}

void ViewportItem::redo() {
    if (m_redo.empty()) return;
    m_undo.push_back(m_scene);
    m_scene = m_redo.back();
    m_redo.pop_back();
    emit historyChanged();
    emit sceneChanged();
    update();
}

float ViewportItem::worldPerPixel(float distance) const {
    const float k = float(M_PI) / 180.f;
    return 2.f * distance * std::tan(m_cam.fov * 0.5f * k) / std::max(1.f, float(height()));
}

// Raio de toque contra a caixa de cada objeto (no espaco local do objeto).
int ViewportItem::pickAt(const QPointF &p) const {
    const float w = float(width()), h = float(height());
    if (w < 1.f || h < 1.f) return -1;
    bool ok = false;
    const QMatrix4x4 inv = (m_cam.proj(w / h) * m_cam.view()).inverted(&ok);
    if (!ok) return -1;
    const float nx = 2.f * float(p.x()) / w - 1.f, ny = 1.f - 2.f * float(p.y()) / h;
    const QVector4D n4 = inv.map(QVector4D(nx, ny, -1.f, 1.f));
    const QVector4D f4 = inv.map(QVector4D(nx, ny, 1.f, 1.f));
    const QVector3D a = n4.toVector3D() / n4.w();
    const QVector3D b = f4.toVector3D() / f4.w();
    const QVector3D dir = b - a;

    int best = -1;
    float bestT = 1e30f;
    for (size_t i = 0; i < m_scene.objs.size(); ++i) {
        const Obj &o = m_scene.objs[i];
        bool ok2 = false;
        const QMatrix4x4 mi = o.model().inverted(&ok2);
        if (!ok2) continue;
        const QVector3D lo = mi.map(a);
        const QVector3D ld = mi.mapVector(dir);
        float he[3];
        primHalfExtents(o.type, he);
        float tmin = 0.f, tmax = 1e30f;
        bool hit = true;
        for (int k = 0; k < 3 && hit; ++k) {
            const float oc = lo[k], dc = ld[k];
            if (std::fabs(dc) < 1e-8f) {
                if (oc < -he[k] || oc > he[k]) hit = false;
            } else {
                float t1 = (-he[k] - oc) / dc, t2 = (he[k] - oc) / dc;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) hit = false;
            }
        }
        if (hit && tmin < bestT) {
            bestT = tmin;
            best = int(i);
        }
    }
    return best;
}

void ViewportItem::orbit(const QPointF &d) {
    m_cam.yaw -= float(d.x()) * 0.4f;
    m_cam.pitch = std::clamp(m_cam.pitch + float(d.y()) * 0.4f, -89.f, 89.f);
}

void ViewportItem::panZoom(const QPointF &d, float zoomRatio) {
    const float wpp = worldPerPixel(m_cam.dist);
    const QVector3D r = m_cam.right(), u = m_cam.up();
    m_cam.target += -r * (float(d.x()) * wpp) + u * (float(d.y()) * wpp);
    if (zoomRatio > 0.01f) m_cam.dist = std::clamp(m_cam.dist / zoomRatio, 0.5f, 120.f);
}

void ViewportItem::applyTransform(const QPointF &d) {
    if (m_scene.sel < 0 || m_scene.sel >= int(m_scene.objs.size())) return;
    Obj &o = m_scene.objs[size_t(m_scene.sel)];
    const QVector3D r = m_cam.right(), u = m_cam.up();
    switch (m_tool) {
    case 0: {  // mover no plano da tela
        const float wpp = worldPerPixel((o.pos - m_cam.eye()).length());
        o.pos += r * (float(d.x()) * wpp) - u * (float(d.y()) * wpp);
        break;
    }
    case 1: {  // rotacionar
        const QQuaternion qy = QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), float(d.x()) * 0.5f);
        const QQuaternion qx = QQuaternion::fromAxisAndAngle(r, float(d.y()) * 0.5f);
        o.rot = (qy * qx * o.rot).normalized();
        break;
    }
    default: {  // escalar (arrastar para cima aumenta)
        const float k = std::exp(-float(d.y()) * 0.005f);
        o.scl = QVector3D(std::clamp(o.scl.x() * k, 0.05f, 50.f), std::clamp(o.scl.y() * k, 0.05f, 50.f),
                          std::clamp(o.scl.z() * k, 0.05f, 50.f));
        break;
    }
    }
}

void ViewportItem::touchUngrabEvent() {
    if (m_mode == Transform && m_changed) commit(m_before);
    m_pts.clear();
    m_mode = Idle;
}

void ViewportItem::touchEvent(QTouchEvent *ev) {
    const auto &points = ev->points();

    // 1) novos toques
    for (const QEventPoint &p : points) {
        if (p.state() != QEventPoint::Pressed) continue;
        m_pts.insert(p.id(), p.position());
        if (m_pts.size() == 1) {
            m_mode = Undecided;
            m_press = p.position();
            m_pressTimer.start();
            m_moved = 0.f;
            m_hit = pickAt(p.position());
            m_changed = false;
        } else if (m_pts.size() == 2) {
            if (m_mode == Transform && m_changed) commit(m_before);
            m_mode = Gesture;
            const QList<int> keys = m_pts.keys();
            const QPointF a = m_pts.value(keys[0]), b = m_pts.value(keys[1]);
            m_lastDist = float(std::hypot(a.x() - b.x(), a.y() - b.y()));
            m_lastMid = (a + b) * 0.5;
        }
    }

    // 2) movimentos
    bool moved = false;
    QPointF delta;
    for (const QEventPoint &p : points) {
        if (p.state() != QEventPoint::Updated || !m_pts.contains(p.id())) continue;
        delta = p.position() - m_pts.value(p.id());
        m_pts[p.id()] = p.position();
        moved = true;
    }
    if (moved) {
        if (m_pts.size() == 1 && m_mode != Gesture) {
            if (m_mode == Undecided) {
                m_moved += float(std::hypot(delta.x(), delta.y()));
                if (m_moved > 8.f) {
                    if (m_hit >= 0 && m_hit == m_scene.sel) {
                        m_mode = Transform;
                        m_before = m_scene;
                        m_changed = false;
                    } else {
                        m_mode = Orbit;
                    }
                }
            }
            if (m_mode == Orbit) {
                orbit(delta);
                update();
            } else if (m_mode == Transform) {
                applyTransform(delta);
                m_changed = true;
                update();
            }
        } else if (m_pts.size() >= 2 && m_mode == Gesture) {
            QList<int> keys = m_pts.keys();
            std::sort(keys.begin(), keys.end());
            const QPointF a = m_pts.value(keys[0]), b = m_pts.value(keys[1]);
            const float dist = float(std::hypot(a.x() - b.x(), a.y() - b.y()));
            const QPointF mid = (a + b) * 0.5;
            const float ratio = m_lastDist > 1.f ? dist / m_lastDist : 1.f;
            panZoom(mid - m_lastMid, ratio);
            m_lastDist = dist;
            m_lastMid = mid;
            update();
        }
    }

    // 3) toques soltos
    for (const QEventPoint &p : points) {
        if (p.state() == QEventPoint::Released) m_pts.remove(p.id());
    }
    if (m_pts.isEmpty() && m_mode != Idle) {
        if (m_mode == Undecided && m_pressTimer.elapsed() < 400) {
            if (m_scene.sel != m_hit) {
                m_scene.sel = m_hit;
                emit sceneChanged();
                update();
            }
        } else if (m_mode == Transform && m_changed) {
            commit(m_before);
        }
        m_mode = Idle;
    }
    ev->accept();
}
