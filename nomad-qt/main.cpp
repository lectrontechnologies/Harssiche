#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QtGui/qtguiglobal.h>
#if QT_CONFIG(vulkan)
#include <QVulkanInstance>
#endif

// O app usa so Vulkan. Se o aparelho nao criar a instancia Vulkan 1.1, grava o marcador "vk_failed"
// (arquivo vazio na pasta de dados) e a interface avisa; nao ha outro backend grafico.
static QString markerPath(const char *name) {
    const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(d);
    return d + QLatin1Char('/') + QLatin1String(name);
}

int main(int argc, char *argv[]) {
    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);

    QGuiApplication app(argc, argv);
    QFile::remove(markerPath("vk_failed"));

    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
#if QT_CONFIG(vulkan)
    QVulkanInstance vkInst;
    vkInst.setApiVersion(QVersionNumber(1, 1));
    if (!vkInst.create()) {
        QFile f(markerPath("vk_failed"));
        f.open(QIODevice::WriteOnly);
    }
#else
    {
        QFile f(markerPath("vk_failed"));
        f.open(QIODevice::WriteOnly);
    }
#endif

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("Nomad", "Main");
    if (engine.rootObjects().isEmpty()) return -1;

    auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!win) return -1;
#if QT_CONFIG(vulkan)
    if (vkInst.isValid()) win->setVulkanInstance(&vkInst);
#endif
    win->showFullScreen();
    return app.exec();
}
