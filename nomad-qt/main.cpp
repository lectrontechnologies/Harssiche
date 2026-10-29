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

// Marcadores de escolha do backend grafico (arquivos vazios na pasta de dados do app):
//   backend_vulkan  -> o usuario pediu para abrir em Vulkan
//   vk_trying       -> a ultima abertura em Vulkan ainda nao chegou ao primeiro quadro (se sobrou, travou)
//   vk_failed       -> o Vulkan falhou/travou e voltamos ao OpenGL (a interface avisa)
static QString markerPath(const char *name) {
    const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(d);
    return d + QLatin1Char('/') + QLatin1String(name);
}

int main(int argc, char *argv[]) {
    // O motor ainda usa OpenGL ES 3.x: este e o formato padrao (usado quando o backend e OpenGL).
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
    fmt.setVersion(3, 0);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);

    QGuiApplication app(argc, argv);

    bool useVk = QFile::exists(markerPath("backend_vulkan"));
    if (useVk && QFile::exists(markerPath("vk_trying"))) {
        // a abertura anterior em Vulkan nao terminou: volta ao OpenGL e avisa
        QFile::remove(markerPath("backend_vulkan"));
        QFile::remove(markerPath("vk_trying"));
        QFile f(markerPath("vk_failed"));
        f.open(QIODevice::WriteOnly);
        useVk = false;
    }

#if QT_CONFIG(vulkan)
    QVulkanInstance vkInst;
    if (useVk) {
        vkInst.setApiVersion(QVersionNumber(1, 1));
        if (!vkInst.create()) {
            QFile::remove(markerPath("backend_vulkan"));
            QFile f(markerPath("vk_failed"));
            f.open(QIODevice::WriteOnly);
            useVk = false;
        }
    }
#else
    useVk = false;
#endif

    if (useVk) {
        QFile t(markerPath("vk_trying"));
        t.open(QIODevice::WriteOnly);
        t.close();
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    } else {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    }

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("Nomad", "Main");
    if (engine.rootObjects().isEmpty()) return -1;

    auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!win) return -1;
#if QT_CONFIG(vulkan)
    if (useVk) win->setVulkanInstance(&vkInst);
#endif
    win->showFullScreen();
    return app.exec();
}
