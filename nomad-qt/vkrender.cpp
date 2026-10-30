#include "vkrender.h"

#if QT_CONFIG(vulkan)
#include <QQuickWindow>
#include <algorithm>
#include <cstdint>
#include <cstring>

#include "vkshaders.h"  // gerado no build (glslc): k_mesh_vert/k_mesh_frag/k_text_vert/k_text_frag

namespace {
// dados por ordem de desenho, no mesmo layout (std430) do buffer de armazenamento dos shaders
struct GpuOp {
    float mvp[16];
    float mod[16];
    float col[4];
    float a[4];
    float b[4];
    float tl[4];
};
struct MeshPC {
    float cam[4];
    float mcR[4];
    float mcU[4];
    float mcB[4];
};
struct TextPC {
    float scr[2];
    float pad[2];
    float col[4];
};
}  // namespace

bool VkRend::fail(const char *why) {
    m_failed = true;
    m_why = why;
    return false;
}

bool VkRend::findMem(uint32_t bits, VkMemoryPropertyFlags want, uint32_t &out) {
    VkPhysicalDeviceMemoryProperties mp;
    m_if->vkGetPhysicalDeviceMemoryProperties(m_phys, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) {
            out = i;
            return true;
        }
    }
    return false;
}

// buffer visivel pela CPU, coerente e mapeado de forma permanente
bool VkRend::mkBuf(Buf &b, VkDeviceSize size, VkBufferUsageFlags usage) {
    VkBufferCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size = size;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (m_df->vkCreateBuffer(m_dev, &ci, nullptr, &b.b) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    m_df->vkGetBufferMemoryRequirements(m_dev, b.b, &mr);
    uint32_t mt = 0;
    if (!findMem(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mt)) return false;
    VkMemoryAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = mt;
    if (m_df->vkAllocateMemory(m_dev, &ai, nullptr, &b.m) != VK_SUCCESS) return false;
    if (m_df->vkBindBufferMemory(m_dev, b.b, b.m, 0) != VK_SUCCESS) return false;
    if (m_df->vkMapMemory(m_dev, b.m, 0, VK_WHOLE_SIZE, 0, &b.p) != VK_SUCCESS) return false;
    b.size = size;
    return true;
}

void VkRend::freeBuf(Buf &b) {
    if (b.p) m_df->vkUnmapMemory(m_dev, b.m);
    if (b.b) m_df->vkDestroyBuffer(m_dev, b.b, nullptr);
    if (b.m) m_df->vkFreeMemory(m_dev, b.m, nullptr);
    b = Buf();
}

VkPipeline VkRend::mkPipe(VkShaderModule vs, VkShaderModule fs, VkPipelineLayout lay, VkRenderPass rp, bool text, bool lines, bool depth) {
    VkPipelineShaderStageCreateInfo st[2] = {};
    st[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    st[0].module = vs;
    st[0].pName = "main";
    st[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    st[1].module = fs;
    st[1].pName = "main";

    VkVertexInputBindingDescription bd = {};
    bd.binding = 0;
    bd.stride = text ? 16 : 24;
    bd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription ad[2] = {};
    ad[0].location = 0;
    ad[0].binding = 0;
    ad[0].format = text ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R32G32B32_SFLOAT;
    ad[0].offset = 0;
    ad[1].location = 1;
    ad[1].binding = 0;
    ad[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    ad[1].offset = 12;
    VkPipelineVertexInputStateCreateInfo vi = {};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bd;
    vi.vertexAttributeDescriptionCount = text ? 1 : 2;
    vi.pVertexAttributeDescriptions = ad;

    VkPipelineInputAssemblyStateCreateInfo ia = {};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = lines ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = depth ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = depth ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;
    VkPipelineColorBlendAttachmentState cba = {};
    cba.blendEnable = VK_TRUE;
    cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.colorBlendOp = VK_BLEND_OP_ADD;
    cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.alphaBlendOp = VK_BLEND_OP_ADD;
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
    gp.layout = lay;
    gp.renderPass = rp;
    gp.subpass = 0;
    VkPipeline p = VK_NULL_HANDLE;
    if (m_df->vkCreateGraphicsPipelines(m_dev, VK_NULL_HANDLE, 1, &gp, nullptr, &p) != VK_SUCCESS) return VK_NULL_HANDLE;
    return p;
}

bool VkRend::prepare(QQuickWindow *w) {
    if (m_ready) return true;
    if (m_failed) return false;

    QSGRendererInterface *rif = w->rendererInterface();
    QVulkanInstance *inst = w->vulkanInstance();
    if (!rif || !inst) return false;  // ainda nao disponivel: tenta no proximo quadro
    void *pp = rif->getResource(w, QSGRendererInterface::PhysicalDeviceResource);
    void *pd = rif->getResource(w, QSGRendererInterface::DeviceResource);
    void *pr = rif->getResource(w, QSGRendererInterface::RenderPassResource);
    if (!pp || !pd || !pr) return false;
    m_phys = *reinterpret_cast<VkPhysicalDevice *>(pp);
    m_dev = *reinterpret_cast<VkDevice *>(pd);
    m_rp = *reinterpret_cast<VkRenderPass *>(pr);
    m_if = inst->functions();
    m_df = inst->deviceFunctions(m_dev);
    if (!m_if || !m_df) return fail("funcoes");
    m_slots = std::clamp(w->graphicsStateInfo().framesInFlight, 1, kMaxSlots);

    // layouts de descritores: [ops] buffer de armazenamento; [tex] atlas da fonte
    VkDescriptorSetLayoutBinding b0 = {};
    b0.binding = 0;
    b0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b0.descriptorCount = 1;
    b0.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dl = {};
    dl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dl.bindingCount = 1;
    dl.pBindings = &b0;
    if (m_df->vkCreateDescriptorSetLayout(m_dev, &dl, nullptr, &m_lOps) != VK_SUCCESS) return fail("layout ops");
    VkDescriptorSetLayoutBinding b1 = {};
    b1.binding = 0;
    b1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b1.descriptorCount = 1;
    b1.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dl.pBindings = &b1;
    if (m_df->vkCreateDescriptorSetLayout(m_dev, &dl, nullptr, &m_lTex) != VK_SUCCESS) return fail("layout tex");

    VkPushConstantRange pcr = {};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    VkPipelineLayoutCreateInfo pl = {};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &pcr;
    pl.pSetLayouts = &m_lOps;
    pcr.size = sizeof(MeshPC);
    if (m_df->vkCreatePipelineLayout(m_dev, &pl, nullptr, &m_plMesh) != VK_SUCCESS) return fail("layout malha");
    pl.pSetLayouts = &m_lTex;
    pcr.size = sizeof(TextPC);
    if (m_df->vkCreatePipelineLayout(m_dev, &pl, nullptr, &m_plText) != VK_SUCCESS) return fail("layout texto");

    auto mod = [&](const uint32_t *code, size_t bytes) {
        VkShaderModuleCreateInfo ci = {};
        ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = bytes;
        ci.pCode = code;
        VkShaderModule m = VK_NULL_HANDLE;
        if (m_df->vkCreateShaderModule(m_dev, &ci, nullptr, &m) != VK_SUCCESS) return VkShaderModule(VK_NULL_HANDLE);
        return m;
    };
    VkShaderModule mv = mod(k_mesh_vert, k_mesh_vert_size), mf = mod(k_mesh_frag, k_mesh_frag_size);
    VkShaderModule tv = mod(k_text_vert, k_text_vert_size), tf = mod(k_text_frag, k_text_frag_size);
    bool ok = mv && mf && tv && tf;
    if (ok) {
        for (int l = 0; l < 2 && ok; ++l)
            for (int d = 0; d < 2 && ok; ++d) {
                m_pMesh[l][d] = mkPipe(mv, mf, m_plMesh, m_rp, false, l == 1, d == 1);
                ok = m_pMesh[l][d] != VK_NULL_HANDLE;
            }
        if (ok) {
            m_pText = mkPipe(tv, tf, m_plText, m_rp, true, false, false);
            ok = m_pText != VK_NULL_HANDLE;
        }
    }
    for (VkShaderModule m : {mv, mf, tv, tf})
        if (m) m_df->vkDestroyShaderModule(m_dev, m, nullptr);
    if (!ok) return fail("pipelines");

    // descritores, buffers por slot e amostrador
    VkDescriptorPoolSize ps[2] = {};
    ps[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ps[0].descriptorCount = kMaxSlots;
    ps[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ps[1].descriptorCount = 1;
    VkDescriptorPoolCreateInfo dpi = {};
    dpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpi.maxSets = kMaxSlots + 1;
    dpi.poolSizeCount = 2;
    dpi.pPoolSizes = ps;
    if (m_df->vkCreateDescriptorPool(m_dev, &dpi, nullptr, &m_pool) != VK_SUCCESS) return fail("pool");

    for (int i = 0; i < m_slots; ++i) {
        Slot &s = m_slot[i];
        if (!mkBuf(s.ops, VkDeviceSize(kMaxOps) * sizeof(GpuOp), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) return fail("buffer ops");
        if (!mkBuf(s.text, VkDeviceSize(kMaxTextFloats) * 4, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)) return fail("buffer texto");
        VkDescriptorSetAllocateInfo ai = {};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = m_pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &m_lOps;
        if (m_df->vkAllocateDescriptorSets(m_dev, &ai, &s.set) != VK_SUCCESS) return fail("conjunto ops");
        VkDescriptorBufferInfo bi = {s.ops.b, 0, VK_WHOLE_SIZE};
        VkWriteDescriptorSet wr = {};
        wr.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wr.dstSet = s.set;
        wr.dstBinding = 0;
        wr.descriptorCount = 1;
        wr.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        wr.pBufferInfo = &bi;
        m_df->vkUpdateDescriptorSets(m_dev, 1, &wr, 0, nullptr);
    }

    VkSamplerCreateInfo sci = {};
    sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxLod = 0.f;
    if (m_df->vkCreateSampler(m_dev, &sci, nullptr, &m_samp) != VK_SUCCESS) return fail("sampler");
    VkDescriptorSetAllocateInfo ati = {};
    ati.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ati.descriptorPool = m_pool;
    ati.descriptorSetCount = 1;
    ati.pSetLayouts = &m_lTex;
    if (m_df->vkAllocateDescriptorSets(m_dev, &ati, &m_setTex) != VK_SUCCESS) return fail("conjunto tex");

    m_ready = true;
    uploadFont(w);  // se falhar, o texto fica desligado (m_font = false) e as demais ordens continuam
    return true;
}

// atlas da fonte (R8) para uma imagem do dispositivo, copiado com comandos gravados antes do passe
bool VkRend::uploadFont(QQuickWindow *w) {
    int fw = 0, fh = 0;
    const unsigned char *px = eng_vk_font(fw, fh);
    if (!px) return false;

    VkImageCreateInfo ic = {};
    ic.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ic.imageType = VK_IMAGE_TYPE_2D;
    ic.format = VK_FORMAT_R8_UNORM;
    ic.extent = {uint32_t(fw), uint32_t(fh), 1};
    ic.mipLevels = 1;
    ic.arrayLayers = 1;
    ic.samples = VK_SAMPLE_COUNT_1_BIT;
    ic.tiling = VK_IMAGE_TILING_OPTIMAL;
    ic.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ic.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ic.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (m_df->vkCreateImage(m_dev, &ic, nullptr, &m_img) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    m_df->vkGetImageMemoryRequirements(m_dev, m_img, &mr);
    uint32_t mt = 0;
    if (!findMem(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mt)) return false;
    VkMemoryAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = mt;
    if (m_df->vkAllocateMemory(m_dev, &ai, nullptr, &m_imgMem) != VK_SUCCESS) return false;
    if (m_df->vkBindImageMemory(m_dev, m_img, m_imgMem, 0) != VK_SUCCESS) return false;
    VkImageViewCreateInfo vc = {};
    vc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vc.image = m_img;
    vc.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vc.format = VK_FORMAT_R8_UNORM;
    vc.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (m_df->vkCreateImageView(m_dev, &vc, nullptr, &m_imgView) != VK_SUCCESS) return false;

    const VkDeviceSize bytes = VkDeviceSize(fw) * fh;
    if (!mkBuf(m_stage, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT)) return false;
    std::memcpy(m_stage.p, px, size_t(bytes));

    QSGRendererInterface *rif = w->rendererInterface();
    w->beginExternalCommands();
    void *pcb = rif->getResource(w, QSGRendererInterface::CommandListResource);
    if (!pcb) {
        w->endExternalCommands();
        return false;
    }
    VkCommandBuffer cb = *reinterpret_cast<VkCommandBuffer *>(pcb);
    VkImageMemoryBarrier br = {};
    br.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    br.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    br.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    br.image = m_img;
    br.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    br.srcAccessMask = 0;
    br.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    br.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    br.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    m_df->vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &br);
    VkBufferImageCopy cp = {};
    cp.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    cp.imageExtent = {uint32_t(fw), uint32_t(fh), 1};
    m_df->vkCmdCopyBufferToImage(cb, m_stage.b, m_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cp);
    br.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    br.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    br.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    br.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    m_df->vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &br);
    w->endExternalCommands();

    VkDescriptorImageInfo ii = {m_samp, m_imgView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet wr = {};
    wr.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr.dstSet = m_setTex;
    wr.dstBinding = 0;
    wr.descriptorCount = 1;
    wr.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wr.pImageInfo = &ii;
    m_df->vkUpdateDescriptorSets(m_dev, 1, &wr, 0, nullptr);
    m_font = true;
    return true;
}

// buffer de vertices da malha (criado na primeira vez que ela aparece)
VkRend::MeshBuf *VkRend::meshFor(const EngVkOp &op) {
    auto it = m_meshes.find(op.vid);
    if (it != m_meshes.end() && it->second.n == op.n) {
        it->second.last = m_frame;
        return &it->second;
    }
    if (it != m_meshes.end()) {
        freeBuf(it->second.b);
        m_meshes.erase(it);
    }
    MeshBuf mb;
    mb.n = op.n;
    mb.last = m_frame;
    if (!mkBuf(mb.b, VkDeviceSize(op.n) * 24, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)) {
        freeBuf(mb.b);
        return nullptr;
    }
    std::memcpy(mb.b.p, op.cpu, size_t(op.n) * 24);
    return &m_meshes.emplace(op.vid, mb).first->second;
}

void VkRend::upload(QQuickWindow *w, const EngVkFrame &f) {
    if (!m_ready) return;
    m_cur = w->graphicsStateInfo().currentFrameSlot % m_slots;
    ++m_frame;
    Slot &s = m_slot[m_cur];
    GpuOp *dst = static_cast<GpuOp *>(s.ops.p);
    uint32_t k = 0;
    m_sceneMeshes = 0;
    for (int seg = 0; seg < 2; ++seg) {
        const std::vector<EngVkOp> &ops = seg == 0 ? f.scene : f.ui;
        for (const EngVkOp &op : ops) {
            if (op.kind != 0 || !op.cpu || op.n <= 0) continue;
            if (k >= kMaxOps) break;
            meshFor(op);
            GpuOp &g = dst[k++];
            std::memcpy(g.mvp, op.mvp, sizeof g.mvp);
            std::memcpy(g.mod, op.mod, sizeof g.mod);
            std::memcpy(g.col, op.col, sizeof g.col);
            std::memcpy(g.a, op.a, sizeof g.a);
            std::memcpy(g.b, op.b, sizeof g.b);
            std::memcpy(g.tl, op.tl, sizeof g.tl);
        }
        if (seg == 0) m_sceneMeshes = k;
    }
    // malhas recriadas ganham outro id: libera as antigas (so depois de varios quadros, ja fora da GPU)
    if ((m_frame & 63) == 0) {
        for (auto it = m_meshes.begin(); it != m_meshes.end();) {
            if (it->second.last + 16 < m_frame) {
                freeBuf(it->second.b);
                it = m_meshes.erase(it);
            } else {
                ++it;
            }
        }
    }
    const size_t nt = std::min<size_t>(f.text.size(), kMaxTextFloats);
    if (nt) std::memcpy(s.text.p, f.text.data(), nt * 4);
}

void VkRend::replay(QQuickWindow *w, int pw, int ph, const std::vector<EngVkOp> &ops, uint32_t baseIdx, const EngVkFrame &f, bool ui) {
    QSGRendererInterface *rif = w->rendererInterface();
    w->beginExternalCommands();
    void *pcb = rif->getResource(w, QSGRendererInterface::CommandListResource);
    if (!pcb) {
        w->endExternalCommands();
        return;
    }
    VkCommandBuffer cb = *reinterpret_cast<VkCommandBuffer *>(pcb);
    Slot &s = m_slot[m_cur];

    VkViewport vp = {0.f, 0.f, float(pw), float(ph), 0.f, 1.f};
    const VkRect2D full = {{0, 0}, {uint32_t(pw), uint32_t(ph)}};
    m_df->vkCmdSetViewport(cb, 0, 1, &vp);
    m_df->vkCmdSetScissor(cb, 0, 1, &full);

    MeshPC mpc = {};
    std::memcpy(mpc.cam, f.cam, sizeof mpc.cam);
    std::memcpy(mpc.mcR, f.mcR, sizeof mpc.mcR);
    std::memcpy(mpc.mcU, f.mcU, sizeof mpc.mcU);
    std::memcpy(mpc.mcB, f.mcB, sizeof mpc.mcB);
    mpc.mcR[3] = float(ph);

    int mode = 0;  // 0 nada ligado, 1 malha, 2 texto
    VkPipeline cur = VK_NULL_HANDLE;
    uint32_t idx = baseIdx;
    bool anyDepth = false;
    for (const EngVkOp &op : ops) {
        if (op.kind == 2) {  // recorte
            VkRect2D sc = full;
            if (op.sc[2] > 0.f && op.sc[3] > 0.f) {
                const int x0 = std::clamp(int(op.sc[0]), 0, pw), y0 = std::clamp(int(op.sc[1]), 0, ph);
                const int x1 = std::clamp(int(op.sc[0] + op.sc[2] + 0.999f), 0, pw), y1 = std::clamp(int(op.sc[1] + op.sc[3] + 0.999f), 0, ph);
                sc = {{x0, y0}, {uint32_t(std::max(0, x1 - x0)), uint32_t(std::max(0, y1 - y0))}};
            }
            m_df->vkCmdSetScissor(cb, 0, 1, &sc);
            continue;
        }
        if (op.kind == 0) {
            if (!op.cpu || op.n <= 0) continue;
            const uint32_t myIdx = idx++;
            if (myIdx >= kMaxOps) continue;
            MeshBuf *mb = meshFor(op);
            if (!mb) continue;
            const bool depth = !ui && op.depth;
            anyDepth = anyDepth || depth;
            VkPipeline want = m_pMesh[op.lines ? 1 : 0][depth ? 1 : 0];
            if (mode != 1) {
                m_df->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_plMesh, 0, 1, &s.set, 0, nullptr);
                m_df->vkCmdPushConstants(cb, m_plMesh, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof mpc, &mpc);
                mode = 1;
                cur = VK_NULL_HANDLE;
            }
            if (want != cur) {
                m_df->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, want);
                cur = want;
            }
            const VkDeviceSize off = 0;
            m_df->vkCmdBindVertexBuffers(cb, 0, 1, &mb->b.b, &off);
            m_df->vkCmdDraw(cb, uint32_t(op.n), 1, 0, myIdx);
        } else if (op.kind == 1) {
            if (!m_font || op.tcount <= 0 || size_t(op.tfirst + op.tcount) * 4 > kMaxTextFloats) continue;
            if (mode != 2) {
                m_df->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pText);
                m_df->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_plText, 0, 1, &m_setTex, 0, nullptr);
                mode = 2;
                cur = VK_NULL_HANDLE;
            }
            TextPC tpc = {};
            tpc.scr[0] = float(pw);
            tpc.scr[1] = float(ph);
            std::memcpy(tpc.col, op.col, sizeof tpc.col);
            m_df->vkCmdPushConstants(cb, m_plText, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof tpc, &tpc);
            const VkDeviceSize off = VkDeviceSize(op.tfirst) * 16;
            m_df->vkCmdBindVertexBuffers(cb, 0, 1, &s.text.b, &off);
            m_df->vkCmdDraw(cb, uint32_t(op.tcount), 1, 0, 0);
        }
    }

    // o Qt Quick desenha o QML em seguida: devolve a profundidade a 1.0 para o 3D nao "vencer" o teste
    if (anyDepth) {
        VkClearAttachment ca = {};
        ca.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        ca.clearValue.depthStencil = {1.0f, 0};
        VkClearRect cr = {full, 0, 1};
        m_df->vkCmdClearAttachments(cb, 1, &ca, 1, &cr);
    }
    w->endExternalCommands();
}

void VkRend::drawScene(QQuickWindow *w, int pw, int ph, const EngVkFrame &f) {
    if (!m_ready || pw < 2 || ph < 2) return;
    replay(w, pw, ph, f.scene, 0, f, false);
}

void VkRend::drawUi(QQuickWindow *w, int pw, int ph, const EngVkFrame &f) {
    if (!m_ready || pw < 2 || ph < 2) return;
    replay(w, pw, ph, f.ui, m_sceneMeshes, f, true);
}

void VkRend::release() {
    if (m_df && m_dev) {
        for (auto &kv : m_meshes) freeBuf(kv.second.b);
        for (int i = 0; i < kMaxSlots; ++i) {
            freeBuf(m_slot[i].ops);
            freeBuf(m_slot[i].text);
        }
        freeBuf(m_stage);
        for (int l = 0; l < 2; ++l)
            for (int d = 0; d < 2; ++d)
                if (m_pMesh[l][d]) m_df->vkDestroyPipeline(m_dev, m_pMesh[l][d], nullptr);
        if (m_pText) m_df->vkDestroyPipeline(m_dev, m_pText, nullptr);
        if (m_plMesh) m_df->vkDestroyPipelineLayout(m_dev, m_plMesh, nullptr);
        if (m_plText) m_df->vkDestroyPipelineLayout(m_dev, m_plText, nullptr);
        if (m_pool) m_df->vkDestroyDescriptorPool(m_dev, m_pool, nullptr);
        if (m_lOps) m_df->vkDestroyDescriptorSetLayout(m_dev, m_lOps, nullptr);
        if (m_lTex) m_df->vkDestroyDescriptorSetLayout(m_dev, m_lTex, nullptr);
        if (m_samp) m_df->vkDestroySampler(m_dev, m_samp, nullptr);
        if (m_imgView) m_df->vkDestroyImageView(m_dev, m_imgView, nullptr);
        if (m_img) m_df->vkDestroyImage(m_dev, m_img, nullptr);
        if (m_imgMem) m_df->vkFreeMemory(m_dev, m_imgMem, nullptr);
    }
    m_meshes.clear();
    for (int l = 0; l < 2; ++l)
        for (int d = 0; d < 2; ++d) m_pMesh[l][d] = VK_NULL_HANDLE;
    m_pText = VK_NULL_HANDLE;
    m_plMesh = m_plText = VK_NULL_HANDLE;
    m_pool = VK_NULL_HANDLE;
    m_lOps = m_lTex = VK_NULL_HANDLE;
    m_samp = VK_NULL_HANDLE;
    m_imgView = VK_NULL_HANDLE;
    m_img = VK_NULL_HANDLE;
    m_imgMem = VK_NULL_HANDLE;
    m_font = false;
    m_ready = false;
}
#endif
