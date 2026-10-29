#include "vkrender.h"

#if QT_CONFIG(vulkan)
#include <QQuickWindow>
#include <cmath>
#include <cstdint>

#include "vkshaders.h"  // gerado no build: k_grid_vert / k_grid_frag (SPIR-V)

namespace {
struct PC {
    float eye[4];
    float right[4];
    float up[4];
    float fwd[4];
};
void nrm(float *v) {
    const float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 1e-8f) {
        v[0] /= l;
        v[1] /= l;
        v[2] /= l;
    }
}
void crs(const float *a, const float *b, float *o) {
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}
}  // namespace

bool VkRend::prepare(QQuickWindow *w) {
    if (m_init) return !m_failed;
    m_init = true;
    m_failed = true;

    QSGRendererInterface *rif = w->rendererInterface();
    QVulkanInstance *inst = w->vulkanInstance();
    if (!rif || !inst) {
        m_init = false;  // ainda nao disponivel: tenta de novo no proximo quadro
        m_failed = false;
        return false;
    }
    void *pd = rif->getResource(w, QSGRendererInterface::DeviceResource);
    void *pr = rif->getResource(w, QSGRendererInterface::RenderPassResource);
    if (!pd || !pr) {
        m_init = false;
        m_failed = false;
        return false;
    }
    m_dev = *reinterpret_cast<VkDevice *>(pd);
    const VkRenderPass rp = *reinterpret_cast<VkRenderPass *>(pr);
    m_df = inst->deviceFunctions(m_dev);
    if (!m_df) return false;

    auto mk = [&](const uint32_t *code, size_t bytes) {
        VkShaderModuleCreateInfo ci = {};
        ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = bytes;
        ci.pCode = code;
        VkShaderModule m = VK_NULL_HANDLE;
        if (m_df->vkCreateShaderModule(m_dev, &ci, nullptr, &m) != VK_SUCCESS) return VkShaderModule(VK_NULL_HANDLE);
        return m;
    };
    VkShaderModule vs = mk(k_grid_vert, k_grid_vert_size);
    VkShaderModule fs = mk(k_grid_frag, k_grid_frag_size);
    if (!vs || !fs) {
        if (vs) m_df->vkDestroyShaderModule(m_dev, vs, nullptr);
        if (fs) m_df->vkDestroyShaderModule(m_dev, fs, nullptr);
        return false;
    }

    VkPushConstantRange pcr = {};
    pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(PC);
    VkPipelineLayoutCreateInfo pli = {};
    pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges = &pcr;
    if (m_df->vkCreatePipelineLayout(m_dev, &pli, nullptr, &m_pl) != VK_SUCCESS) {
        m_df->vkDestroyShaderModule(m_dev, vs, nullptr);
        m_df->vkDestroyShaderModule(m_dev, fs, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo st[2] = {};
    st[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    st[0].module = vs;
    st[0].pName = "main";
    st[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    st[1].module = fs;
    st[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vi = {};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo ia = {};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp = {};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs = {};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms = {};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo ds = {};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;  // sem teste nem escrita de profundidade
    VkPipelineColorBlendAttachmentState cba = {};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb = {};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;
    const VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy = {};
    dy.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo gp = {};
    gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gp.stageCount = 2;
    gp.pStages = st;
    gp.pVertexInputState = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vp;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pDepthStencilState = &ds;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dy;
    gp.layout = m_pl;
    gp.renderPass = rp;
    gp.subpass = 0;
    const VkResult r = m_df->vkCreateGraphicsPipelines(m_dev, VK_NULL_HANDLE, 1, &gp, nullptr, &m_pipe);
    m_df->vkDestroyShaderModule(m_dev, vs, nullptr);
    m_df->vkDestroyShaderModule(m_dev, fs, nullptr);
    if (r != VK_SUCCESS) return false;

    m_failed = false;
    return true;
}

void VkRend::draw(QQuickWindow *w, int pw, int ph, const VkViewParams &v) {
    if (!m_init && !prepare(w)) return;
    if (m_failed || !m_pipe || pw < 2 || ph < 2) return;

    // camera orbital -> base ortonormal (direita/cima/frente) e posicao do olho
    const float cp = std::cos(v.pitch), sp = std::sin(v.pitch), cy = std::cos(v.yaw), sy = std::sin(v.yaw);
    float eye[3] = {v.tx + v.dist * cp * sy, v.ty + v.dist * sp, v.tz + v.dist * cp * cy};
    float fwd[3] = {v.tx - eye[0], v.ty - eye[1], v.tz - eye[2]};
    nrm(fwd);
    const float worldUp[3] = {0.f, 1.f, 0.f};
    float right[3], up[3];
    crs(fwd, worldUp, right);
    nrm(right);
    crs(right, fwd, up);

    PC pc;
    for (int i = 0; i < 3; ++i) {
        pc.eye[i] = eye[i];
        pc.right[i] = right[i];
        pc.up[i] = up[i];
        pc.fwd[i] = fwd[i];
    }
    pc.eye[3] = std::tan(0.5f * 0.8f);  // fov vertical ~46 graus
    pc.right[3] = float(pw) / float(ph);
    pc.up[3] = 0.f;
    pc.fwd[3] = 0.f;

    QSGRendererInterface *rif = w->rendererInterface();
    w->beginExternalCommands();
    void *pcb = rif->getResource(w, QSGRendererInterface::CommandListResource);
    if (pcb) {
        VkCommandBuffer cb = *reinterpret_cast<VkCommandBuffer *>(pcb);
        VkViewport vp = {0.f, 0.f, float(pw), float(ph), 0.f, 1.f};
        VkRect2D sc = {{0, 0}, {uint32_t(pw), uint32_t(ph)}};
        m_df->vkCmdSetViewport(cb, 0, 1, &vp);
        m_df->vkCmdSetScissor(cb, 0, 1, &sc);
        m_df->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipe);
        m_df->vkCmdPushConstants(cb, m_pl, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PC), &pc);
        m_df->vkCmdDraw(cb, 3, 1, 0, 0);
    }
    w->endExternalCommands();
}

void VkRend::release() {
    if (m_df && m_dev) {
        if (m_pipe) m_df->vkDestroyPipeline(m_dev, m_pipe, nullptr);
        if (m_pl) m_df->vkDestroyPipelineLayout(m_dev, m_pl, nullptr);
    }
    m_pipe = VK_NULL_HANDLE;
    m_pl = VK_NULL_HANDLE;
    m_init = false;
}
#endif
