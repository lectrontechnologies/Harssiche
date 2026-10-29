#pragma once
#include <QtGui/qtguiglobal.h>

class QQuickWindow;

// Camera orbital do viewport Vulkan (alvo + yaw/pitch/distancia).
struct VkViewParams {
    float yaw = 0.9f, pitch = 0.5f, dist = 16.f;
    float tx = 0.f, ty = 0.5f, tz = 0.f;
};

#if QT_CONFIG(vulkan)
#include <QVulkanFunctions>
#include <QVulkanInstance>

// Renderizador Vulkan do viewport (passo 1: fundo + grade). Desenha dentro do passe de
// renderizacao do Qt Quick, por baixo do QML (mesma tecnica do exemplo "Vulkan Under QML").
class VkRend {
public:
    ~VkRend() { release(); }
    bool prepare(QQuickWindow *w);  // cria o pipeline (uma vez); false se falhou
    void draw(QQuickWindow *w, int pw, int ph, const VkViewParams &v);
    void release();
    bool failed() const { return m_failed; }

private:
    QVulkanDeviceFunctions *m_df = nullptr;
    VkDevice m_dev = VK_NULL_HANDLE;
    VkPipelineLayout m_pl = VK_NULL_HANDLE;
    VkPipeline m_pipe = VK_NULL_HANDLE;
    bool m_init = false;
    bool m_failed = false;
};
#endif
