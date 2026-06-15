#include "platform/VulkanPipelineBuilder.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>

namespace vr {
namespace {

VkShaderModule createShader(VkDevice device, const std::filesystem::path& path) {
    const std::vector<std::uint32_t> code = readSpirv(resolveProjectPath(path));
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(std::uint32_t);
    createInfo.pCode = code.data();
    VkShaderModule shader = VK_NULL_HANDLE;
    require(vkCreateShaderModule(device, &createInfo, nullptr, &shader), "vkCreateShaderModule");
    return shader;
}

} // anonymous namespace

Pipelines createPipelines(
    VkDevice device,
    VkSampleCountFlagBits msaaSamples,
    const RenderPasses& passes,
    VkDescriptorSetLayout forwardDescriptorLayout,
    VkDescriptorSetLayout v4DescriptorLayout,
    VkDescriptorSetLayout v5DescriptorLayout,
    bool enableV4,
    bool enableV5
) {
    Pipelines p;

    // --- Shared state structs reused across pipelines ---
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(GpuPreviewVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 9> attributes{};
    attributes[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GpuPreviewVertex, px)};
    attributes[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GpuPreviewVertex, nx)};
    attributes[2] = {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GpuPreviewVertex, r)};
    attributes[3] = {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(GpuPreviewVertex, u)};
    attributes[4] = {4, 0, VK_FORMAT_R32_SFLOAT, offsetof(GpuPreviewVertex, textured)};
    attributes[5] = {5, 0, VK_FORMAT_R32_SFLOAT, offsetof(GpuPreviewVertex, roughness)};
    attributes[6] = {6, 0, VK_FORMAT_R32_SFLOAT, offsetof(GpuPreviewVertex, metalness)};
    attributes[7] = {7, 0, VK_FORMAT_R32_SFLOAT, offsetof(GpuPreviewVertex, kind)};
    attributes[8] = {8, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GpuPreviewVertex, tx)};

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = msaaSamples;

    VkPipelineDepthStencilStateCreateInfo depth{};
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlend{};
    colorBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &colorBlend;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = static_cast<std::uint32_t>(std::size(dynamicStates));
    dynamic.pDynamicStates = dynamicStates;

    // --- Forward pipeline layout ---
    {
        VkPipelineLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout.setLayoutCount = 1;
        layout.pSetLayouts = &forwardDescriptorLayout;
        require(vkCreatePipelineLayout(device, &layout, nullptr, &p.forwardLayout), "vkCreatePipelineLayout");
    }

    // --- Main forward pipeline ---
    {
        const VkShaderModule vertexShader = createShader(device, "shaders/vulkan_gpu/simple_color.vert.spv");
        const VkShaderModule fragmentShader = createShader(device, "shaders/vulkan_gpu/simple_color.frag.spv");

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertexShader;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragmentShader;
        stages[1].pName = "main";

        VkGraphicsPipelineCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        createInfo.stageCount = 2;
        createInfo.pStages = stages;
        createInfo.pVertexInputState = &vertexInput;
        createInfo.pInputAssemblyState = &inputAssembly;
        createInfo.pViewportState = &viewportState;
        createInfo.pRasterizationState = &raster;
        createInfo.pMultisampleState = &multisample;
        createInfo.pDepthStencilState = &depth;
        createInfo.pColorBlendState = &blend;
        createInfo.pDynamicState = &dynamic;
        createInfo.layout = p.forwardLayout;
        createInfo.renderPass = passes.forward;
        createInfo.subpass = 0;
        require(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &p.forward), "vkCreateGraphicsPipelines");

        vkDestroyShaderModule(device, fragmentShader, nullptr);
        vkDestroyShaderModule(device, vertexShader, nullptr);
    }

    // --- Shadow depth pipeline ---
    {
        const VkShaderModule shadowVertexShader = createShader(device, "shaders/vulkan_gpu/shadow_depth.vert.spv");
        VkPipelineShaderStageCreateInfo shadowStage{};
        shadowStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shadowStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        shadowStage.module = shadowVertexShader;
        shadowStage.pName = "main";

        VkPipelineRasterizationStateCreateInfo shadowRaster{};
        shadowRaster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        shadowRaster.polygonMode = VK_POLYGON_MODE_FILL;
        shadowRaster.cullMode = VK_CULL_MODE_BACK_BIT;
        shadowRaster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        shadowRaster.depthBiasEnable = VK_TRUE;
        shadowRaster.depthBiasConstantFactor = 1.4f;
        shadowRaster.depthBiasSlopeFactor = 1.8f;
        shadowRaster.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo shadowMultisample{};
        shadowMultisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        shadowMultisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendStateCreateInfo shadowBlend{};
        shadowBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

        VkGraphicsPipelineCreateInfo shadowCreateInfo{};
        shadowCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        shadowCreateInfo.stageCount = 1;
        shadowCreateInfo.pStages = &shadowStage;
        shadowCreateInfo.pVertexInputState = &vertexInput;
        shadowCreateInfo.pInputAssemblyState = &inputAssembly;
        shadowCreateInfo.pViewportState = &viewportState;
        shadowCreateInfo.pRasterizationState = &shadowRaster;
        shadowCreateInfo.pMultisampleState = &shadowMultisample;
        shadowCreateInfo.pDepthStencilState = &depth;
        shadowCreateInfo.pColorBlendState = &shadowBlend;
        shadowCreateInfo.pDynamicState = &dynamic;
        shadowCreateInfo.layout = p.forwardLayout;
        shadowCreateInfo.renderPass = passes.shadow;
        shadowCreateInfo.subpass = 0;
        require(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &shadowCreateInfo, nullptr, &p.shadow), "vkCreateGraphicsPipelines(shadow)");
        vkDestroyShaderModule(device, shadowVertexShader, nullptr);
    }

    // --- Skybox pipeline ---
    {
        const VkShaderModule skyVertexShader = createShader(device, "shaders/vulkan_gpu/skybox.vert.spv");
        const VkShaderModule skyFragmentShader = createShader(device, "shaders/vulkan_gpu/skybox.frag.spv");
        VkPipelineShaderStageCreateInfo skyStages[2]{};
        skyStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        skyStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        skyStages[0].module = skyVertexShader;
        skyStages[0].pName = "main";
        skyStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        skyStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        skyStages[1].module = skyFragmentShader;
        skyStages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo skyVertexInput{};
        skyVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineRasterizationStateCreateInfo skyRaster{};
        skyRaster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        skyRaster.polygonMode = VK_POLYGON_MODE_FILL;
        skyRaster.cullMode = VK_CULL_MODE_NONE;
        skyRaster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        skyRaster.lineWidth = 1.0f;
        VkPipelineDepthStencilStateCreateInfo skyDepth{};
        skyDepth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        skyDepth.depthTestEnable = VK_FALSE;
        skyDepth.depthWriteEnable = VK_FALSE;

        VkGraphicsPipelineCreateInfo skyCreateInfo{};
        skyCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        skyCreateInfo.stageCount = 2;
        skyCreateInfo.pStages = skyStages;
        skyCreateInfo.pVertexInputState = &skyVertexInput;
        skyCreateInfo.pInputAssemblyState = &inputAssembly;
        skyCreateInfo.pViewportState = &viewportState;
        skyCreateInfo.pRasterizationState = &skyRaster;
        skyCreateInfo.pMultisampleState = &multisample;
        skyCreateInfo.pDepthStencilState = &skyDepth;
        skyCreateInfo.pColorBlendState = &blend;
        skyCreateInfo.pDynamicState = &dynamic;
        skyCreateInfo.layout = p.forwardLayout;
        skyCreateInfo.renderPass = passes.forward;
        skyCreateInfo.subpass = 0;
        require(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &skyCreateInfo, nullptr, &p.sky), "vkCreateGraphicsPipelines(sky)");

        vkDestroyShaderModule(device, skyFragmentShader, nullptr);
        vkDestroyShaderModule(device, skyVertexShader, nullptr);
    }

    // --- GBuffer pipelines (V4 and V5) ---
    if (enableV4 || enableV5) {
        const VkShaderModule gbufferVertexShader = createShader(device, "shaders/vulkan_gpu/simple_color.vert.spv");
        const VkShaderModule gbufferFragmentShader = createShader(device, "shaders/vulkan_gpu/v4_gbuffer.frag.spv");
        VkPipelineShaderStageCreateInfo gbufferStages[2]{};
        gbufferStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        gbufferStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        gbufferStages[0].module = gbufferVertexShader;
        gbufferStages[0].pName = "main";
        gbufferStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        gbufferStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        gbufferStages[1].module = gbufferFragmentShader;
        gbufferStages[1].pName = "main";

        VkPipelineMultisampleStateCreateInfo gbufferMultisample{};
        gbufferMultisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        gbufferMultisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        std::array<VkPipelineColorBlendAttachmentState, 3> gbufferColorBlends{};
        for (auto& attachment : gbufferColorBlends) {
            attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }
        VkPipelineColorBlendStateCreateInfo gbufferBlend{};
        gbufferBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        gbufferBlend.attachmentCount = static_cast<std::uint32_t>(gbufferColorBlends.size());
        gbufferBlend.pAttachments = gbufferColorBlends.data();

        VkGraphicsPipelineCreateInfo gbufferCreateInfo{};
        gbufferCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gbufferCreateInfo.stageCount = 2;
        gbufferCreateInfo.pStages = gbufferStages;
        gbufferCreateInfo.pVertexInputState = &vertexInput;
        gbufferCreateInfo.pInputAssemblyState = &inputAssembly;
        gbufferCreateInfo.pViewportState = &viewportState;
        gbufferCreateInfo.pRasterizationState = &raster;
        gbufferCreateInfo.pMultisampleState = &gbufferMultisample;
        gbufferCreateInfo.pDepthStencilState = &depth;
        gbufferCreateInfo.pColorBlendState = &gbufferBlend;
        gbufferCreateInfo.pDynamicState = &dynamic;
        gbufferCreateInfo.layout = p.forwardLayout;
        gbufferCreateInfo.renderPass = passes.gbuffer;
        gbufferCreateInfo.subpass = 0;
        require(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gbufferCreateInfo, nullptr, &p.gbuffer), "vkCreateGraphicsPipelines(gbuffer)");

        // Instanced GBuffer pipeline
        {
            const VkShaderModule instancedVertexShader = createShader(device, "shaders/vulkan_gpu/v4_instanced_sphere.vert.spv");
            VkPipelineShaderStageCreateInfo instancedStages[2]{};
            instancedStages[0] = gbufferStages[0];
            instancedStages[0].module = instancedVertexShader;
            instancedStages[1] = gbufferStages[1];

            std::array<VkVertexInputBindingDescription, 2> instancedBindings{};
            instancedBindings[0] = binding;
            instancedBindings[1].binding = 1;
            instancedBindings[1].stride = sizeof(GpuPreviewSphereInstance);
            instancedBindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

            std::array<VkVertexInputAttributeDescription, 12> instancedAttributes{};
            std::copy(attributes.begin(), attributes.end(), instancedAttributes.begin());
            instancedAttributes[9] = {9, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GpuPreviewSphereInstance, px)};
            instancedAttributes[10] = {10, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GpuPreviewSphereInstance, r)};
            instancedAttributes[11] = {11, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(GpuPreviewSphereInstance, metalness)};

            VkPipelineVertexInputStateCreateInfo instancedVertexInput{};
            instancedVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            instancedVertexInput.vertexBindingDescriptionCount = static_cast<std::uint32_t>(instancedBindings.size());
            instancedVertexInput.pVertexBindingDescriptions = instancedBindings.data();
            instancedVertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(instancedAttributes.size());
            instancedVertexInput.pVertexAttributeDescriptions = instancedAttributes.data();

            VkGraphicsPipelineCreateInfo instancedCreateInfo = gbufferCreateInfo;
            instancedCreateInfo.pStages = instancedStages;
            instancedCreateInfo.pVertexInputState = &instancedVertexInput;
            require(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &instancedCreateInfo, nullptr, &p.gbufferInstanced), "vkCreateGraphicsPipelines(gbuffer instanced)");
            vkDestroyShaderModule(device, instancedVertexShader, nullptr);
        }

        vkDestroyShaderModule(device, gbufferFragmentShader, nullptr);
        vkDestroyShaderModule(device, gbufferVertexShader, nullptr);
    }

    // --- V4 fullscreen pipelines (SSAO, SSAO blur, compose) ---
    if (enableV4) {
        {
            VkPipelineLayoutCreateInfo composeLayout{};
            composeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            composeLayout.setLayoutCount = 1;
            composeLayout.pSetLayouts = &v4DescriptorLayout;
            require(vkCreatePipelineLayout(device, &composeLayout, nullptr, &p.v4ComposeLayout), "vkCreatePipelineLayout(v4 compose)");
        }

        const VkShaderModule fullscreenVertexShader = createShader(device, "shaders/vulkan_gpu/v4_fullscreen.vert.spv");
        const VkShaderModule ssaoFragmentShader = createShader(device, "shaders/vulkan_gpu/v4_ssao.frag.spv");
        const VkShaderModule ssaoBlurFragmentShader = createShader(device, "shaders/vulkan_gpu/v4_ssao_blur.frag.spv");
        const VkShaderModule composeFragmentShader = createShader(device, "shaders/vulkan_gpu/v4_ssao_compose.frag.spv");

        VkPipelineDepthStencilStateCreateInfo composeDepth{};
        composeDepth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        composeDepth.depthTestEnable = VK_FALSE;
        composeDepth.depthWriteEnable = VK_FALSE;

        VkPipelineRasterizationStateCreateInfo composeRaster{};
        composeRaster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        composeRaster.polygonMode = VK_POLYGON_MODE_FILL;
        composeRaster.cullMode = VK_CULL_MODE_NONE;
        composeRaster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        composeRaster.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo fullscreenMultisample{};
        fullscreenMultisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        fullscreenMultisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineVertexInputStateCreateInfo noVertexInput{};
        noVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        auto createFullscreenPipeline = [&](VkShaderModule fragShader, VkRenderPass targetPass, VkSampleCountFlagBits samples, const char* label) {
            VkPipelineShaderStageCreateInfo stages[2]{};
            stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            stages[0].module = fullscreenVertexShader;
            stages[0].pName = "main";
            stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[1].module = fragShader;
            stages[1].pName = "main";

            VkPipelineMultisampleStateCreateInfo passMultisample = fullscreenMultisample;
            passMultisample.rasterizationSamples = samples;

            VkGraphicsPipelineCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            createInfo.stageCount = 2;
            createInfo.pStages = stages;
            createInfo.pVertexInputState = &noVertexInput;
            createInfo.pInputAssemblyState = &inputAssembly;
            createInfo.pViewportState = &viewportState;
            createInfo.pRasterizationState = &composeRaster;
            createInfo.pMultisampleState = &passMultisample;
            createInfo.pDepthStencilState = &composeDepth;
            createInfo.pColorBlendState = &blend;
            createInfo.pDynamicState = &dynamic;
            createInfo.layout = p.v4ComposeLayout;
            createInfo.renderPass = targetPass;
            createInfo.subpass = 0;
            VkPipeline created = VK_NULL_HANDLE;
            require(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &created), label);
            return created;
        };

        p.ssao = createFullscreenPipeline(ssaoFragmentShader, passes.ssao, VK_SAMPLE_COUNT_1_BIT, "vkCreateGraphicsPipelines(v4 ssao)");
        p.ssaoBlur = createFullscreenPipeline(ssaoBlurFragmentShader, passes.ssao, VK_SAMPLE_COUNT_1_BIT, "vkCreateGraphicsPipelines(v4 ssao blur)");
        p.v4Compose = createFullscreenPipeline(composeFragmentShader, passes.forward, msaaSamples, "vkCreateGraphicsPipelines(v4 compose)");

        vkDestroyShaderModule(device, composeFragmentShader, nullptr);
        vkDestroyShaderModule(device, ssaoBlurFragmentShader, nullptr);
        vkDestroyShaderModule(device, ssaoFragmentShader, nullptr);
        vkDestroyShaderModule(device, fullscreenVertexShader, nullptr);
    }

    // --- V5 compute pipeline ---
    if (enableV5) {
        VkPipelineLayoutCreateInfo rtLayout{};
        rtLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        rtLayout.setLayoutCount = 1;
        rtLayout.pSetLayouts = &v5DescriptorLayout;
        require(vkCreatePipelineLayout(device, &rtLayout, nullptr, &p.v5RayTracingLayout), "vkCreatePipelineLayout(v5 rt)");

        const VkShaderModule rayTracingShader = createShader(device, "shaders/vulkan_gpu/v5_raytrace.comp.spv");
        VkPipelineShaderStageCreateInfo stage{};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = rayTracingShader;
        stage.pName = "main";

        VkComputePipelineCreateInfo computeCreateInfo{};
        computeCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computeCreateInfo.stage = stage;
        computeCreateInfo.layout = p.v5RayTracingLayout;
        require(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computeCreateInfo, nullptr, &p.v5Compute), "vkCreateComputePipelines(v5 rt)");
        vkDestroyShaderModule(device, rayTracingShader, nullptr);
    }

    return p;
}

} // namespace vr
