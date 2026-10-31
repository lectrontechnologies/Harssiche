#pragma once
#include <QtGui/qtguiglobal.h>

#include "engine_api.h"

class QQuickWindow;

#if QT_CONFIG(vulkan)
#include <QVulkanFunctions>
#include <QVulkanInstance>
#include <unordered_map>
#include <vector>

// Renderizador Vulkan do motor: executa as ordens de desenho gravadas pelo motor (malhas, texto, recortes).
// Desenha dentro do passe de renderizacao do Qt Quick: o 3D por baixo do QML e a interface do motor por cima
// (mesma tecnica do exemplo "Vulkan Under QML"). Tudo roda na thread de render.
class VkRend {
public:
    ~VkRend() { release(); }
    bool prepare(QQuickWindow *w);                      // antes do passe: cria os recursos (uma vez)
    void upload(QQuickWindow *w, const EngVkFrame &f);  // antes do passe: copia as ordens para o slot atual
    void drawScene(QQuickWindow *w, int pw, int ph, const EngVkFrame &f);  // dentro do passe, antes do QML
    void drawUi(QQuickWindow *w, int pw, int ph, const EngVkFrame &f);     // dentro do passe, depois do QML
    void release();
    bool ready() const { return m_ready; }
    bool failed() const { return m_failed; }
    const char *why() const { return m_why; }  // motivo da falha (diagnostico)

private:
    struct Buf {
        VkBuffer b = VK_NULL_HANDLE;
        VkDeviceMemory m = VK_NULL_HANDLE;
        void *p = nullptr;
        VkDeviceSize size = 0;
    };
    struct Slot {
        Buf ops, text, pal;  // ordens, vertices de texto, paletas de ossos
        Buf gzv, gzi;        // vertices e indices do ImGui (gizmos)
        VkDescriptorSet set = VK_NULL_HANDLE;
        VkDescriptorSet gzSet = VK_NULL_HANDLE;  // atlas do ImGui
        unsigned gzVer = 0;                      // versao do atlas ligada a este conjunto
    };
    struct Img {  // imagem + memoria + visao + buffer de copia
        VkImage img = VK_NULL_HANDLE;
        VkDeviceMemory mem = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        Buf stage;
        uint64_t retired = 0;
    };
    struct MeshBuf {
        Buf b;
        Buf skin;  // indices e pesos de osso (so malhas com esqueleto)
        int n = 0;
        uint64_t last = 0;  // ultimo quadro em que foi usada (para liberar malhas que sumiram)
    };

    bool fail(const char *why);
    bool findMem(uint32_t bits, VkMemoryPropertyFlags want, uint32_t &out);
    bool mkBuf(Buf &b, VkDeviceSize size, VkBufferUsageFlags usage);
    void freeBuf(Buf &b);
    VkPipeline mkPipe(VkShaderModule vs, VkShaderModule fs, VkPipelineLayout lay, VkRenderPass rp, bool text, bool lines, bool depth, bool skin, bool gz);
    bool uploadFont(QQuickWindow *w);
    bool uploadImg(QQuickWindow *w, Img &im, const unsigned char *px, int fw, int fh, VkFormat fmt, int bpp, bool alphaSwz);
    void freeImg(Img &im);
    void syncGz(QQuickWindow *w);
    void drawGz(QQuickWindow *w, int pw, int ph, const EngVkGz &g);
    MeshBuf *meshFor(const EngVkOp &op);
    void replay(QQuickWindow *w, int pw, int ph, const std::vector<EngVkOp> &ops, uint32_t baseIdx, const EngVkFrame &f, bool ui);

    QVulkanFunctions *m_if = nullptr;
    QVulkanDeviceFunctions *m_df = nullptr;
    VkPhysicalDevice m_phys = VK_NULL_HANDLE;
    VkDevice m_dev = VK_NULL_HANDLE;
    VkRenderPass m_rp = VK_NULL_HANDLE;

    static constexpr int kMaxSlots = 3;
    static constexpr uint32_t kMaxOps = 8192;
    static constexpr uint32_t kMaxTextFloats = 160000;
    static constexpr uint32_t kMaxGzVerts = 20000;
    static constexpr uint32_t kMaxGzIdx = 60000;
    int m_slots = 2;
    int m_cur = 0;
    uint64_t m_frame = 0;
    uint32_t m_sceneMeshes = 0;
    Slot m_slot[kMaxSlots];

    VkDescriptorSetLayout m_lOps = VK_NULL_HANDLE, m_lTex = VK_NULL_HANDLE;
    VkPipelineLayout m_plMesh = VK_NULL_HANDLE, m_plText = VK_NULL_HANDLE;
    VkPipeline m_pMesh[2][2][2] = {};  // [linhas][profundidade][esqueleto]
    VkPipeline m_pText = VK_NULL_HANDLE;
    VkPipeline m_pGz = VK_NULL_HANDLE;
    VkPipelineLayout m_plGz = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    VkSampler m_samp = VK_NULL_HANDLE;
    VkDescriptorSet m_setTex = VK_NULL_HANDLE;
    Img m_fontImg;
    Img m_gzImg;
    std::vector<Img> m_retire;  // imagens substituidas, liberadas depois de alguns quadros
    unsigned m_gzVer = 0, m_gzTried = 0;
    bool m_gzDraw = false;
    bool m_font = false;
    bool m_ready = false;
    bool m_failed = false;
    const char *m_why = "";
    std::unordered_map<int, MeshBuf> m_meshes;
};
#endif
