#include "vulkan/VulkanCommandRecorder.hpp"

namespace vr {

void recordShadowPass(const RecordState& state) {
    VkClearValue shadowClear{};
    shadowClear.depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo shadowPass{};
    shadowPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    shadowPass.renderPass = state.shadowPass;
    shadowPass.framebuffer = state.shadowFramebuffer;
    shadowPass.renderArea.extent = {kShadowAtlasWidth, kShadowAtlasHeight};
    shadowPass.clearValueCount = 1;
    shadowPass.pClearValues = &shadowClear;
    vkCmdBeginRenderPass(state.cmd, &shadowPass, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.shadowPipeline);
    vkCmdBindDescriptorSets(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.forwardLayout, 0, 1, &state.fallbackDescriptorSet, 0, nullptr);
    VkDeviceSize shadowOffset = 0;
    vkCmdBindVertexBuffers(state.cmd, 0, 1, &state.vertexBuffer, &shadowOffset);
    for (std::uint32_t shadowIndex = 0; shadowIndex < kShadowMapCount; ++shadowIndex) {
        const std::uint32_t col = shadowIndex % kShadowAtlasColumns;
        const std::uint32_t row = shadowIndex / kShadowAtlasColumns;
        VkViewport shadowViewport{};
        shadowViewport.x = static_cast<float>(col * kShadowTileSize);
        shadowViewport.y = static_cast<float>(row * kShadowTileSize);
        shadowViewport.width = static_cast<float>(kShadowTileSize);
        shadowViewport.height = static_cast<float>(kShadowTileSize);
        shadowViewport.minDepth = 0.0f;
        shadowViewport.maxDepth = 1.0f;
        VkRect2D shadowScissor{};
        shadowScissor.offset = {static_cast<std::int32_t>(col * kShadowTileSize), static_cast<std::int32_t>(row * kShadowTileSize)};
        shadowScissor.extent = {kShadowTileSize, kShadowTileSize};
        vkCmdSetViewport(state.cmd, 0, 1, &shadowViewport);
        vkCmdSetScissor(state.cmd, 0, 1, &shadowScissor);
        vkCmdDraw(state.cmd, state.vertexCount, 1, 0, shadowIndex);
    }
    vkCmdEndRenderPass(state.cmd);
}

void recordGBufferFill(const RecordState& state) {
    VkViewport viewport{};
    viewport.width = static_cast<float>(state.extent.width);
    viewport.height = static_cast<float>(state.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{};
    scissor.extent = state.extent;

    std::array<VkClearValue, 4> gbufferClears{};
    gbufferClears[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    gbufferClears[1].color = {{0.5f, 0.5f, 1.0f, 1.0f}};
    gbufferClears[2].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    gbufferClears[3].depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo gbufferPass{};
    gbufferPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    gbufferPass.renderPass = state.gbufferPass;
    gbufferPass.framebuffer = state.gbufferFramebuffer;
    gbufferPass.renderArea.extent = state.extent;
    gbufferPass.clearValueCount = static_cast<std::uint32_t>(gbufferClears.size());
    gbufferPass.pClearValues = gbufferClears.data();
    vkCmdBeginRenderPass(state.cmd, &gbufferPass, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(state.cmd, 0, 1, &viewport);
    vkCmdSetScissor(state.cmd, 0, 1, &scissor);
    vkCmdBindPipeline(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.gbufferPipeline);
    VkDeviceSize gbufferOffset = 0;
    vkCmdBindVertexBuffers(state.cmd, 0, 1, &state.vertexBuffer, &gbufferOffset);

    if (!state.batches || state.batches->empty()) {
        vkCmdBindDescriptorSets(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.forwardLayout, 0, 1, &state.fallbackDescriptorSet, 0, nullptr);
        vkCmdDraw(state.cmd, state.vertexCount, 1, 0, 0);
    } else {
        for (const auto& batch : *state.batches) {
            if (batch.vertexCount == 0) {
                continue;
            }
            const std::uint32_t materialIndex = batch.materialIndex < state.materialDescriptorSets->size() ? batch.materialIndex : 0u;
            const VkDescriptorSet descriptorSet = (*state.materialDescriptorSets)[materialIndex];
            vkCmdBindDescriptorSets(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.forwardLayout, 0, 1, &descriptorSet, 0, nullptr);
            vkCmdDraw(state.cmd, batch.vertexCount, 1, batch.firstVertex, 0);
        }
    }

    if (state.sphereVertexCount > 0 && state.sphereInstanceCount > 0) {
        vkCmdBindPipeline(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.instancedGbufferPipeline);
        vkCmdBindDescriptorSets(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.forwardLayout, 0, 1, &state.fallbackDescriptorSet, 0, nullptr);
        std::array<VkBuffer, 2> sphereBuffers{state.sphereVertexBuffer, state.sphereInstanceBuffer};
        std::array<VkDeviceSize, 2> sphereOffsets{0, 0};
        vkCmdBindVertexBuffers(state.cmd, 0, static_cast<std::uint32_t>(sphereBuffers.size()), sphereBuffers.data(), sphereOffsets.data());
        vkCmdDraw(state.cmd, state.sphereVertexCount, state.sphereInstanceCount, 0, 0);
    }
    vkCmdEndRenderPass(state.cmd);
}

void recordV5RayTracing(const RecordState& state) {
    if (state.frameIndex < 4) previewLog("record v6: begin gbuffer pass");
    recordGBufferFill(state);

    if (state.frameIndex < 4) previewLog("record v6: gbuffer compute barrier");
    std::array<VkImageMemoryBarrier, 3> gbufferReadBarriers{};
    for (std::size_t i = 0; i < gbufferReadBarriers.size(); ++i) {
        VkImageMemoryBarrier& barrier = gbufferReadBarriers[i];
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = state.gbufferImages[i];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }
    vkCmdPipelineBarrier(
        state.cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        static_cast<std::uint32_t>(gbufferReadBarriers.size()),
        gbufferReadBarriers.data()
    );

    // History barrier before compute
    std::array<VkImageMemoryBarrier, 2> historyBeforeCompute{};
    historyBeforeCompute[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    historyBeforeCompute[0].oldLayout = state.v5HistoryReadInitialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    historyBeforeCompute[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    historyBeforeCompute[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    historyBeforeCompute[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    historyBeforeCompute[0].image = state.v5HistoryReadImage;
    historyBeforeCompute[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    historyBeforeCompute[0].subresourceRange.levelCount = 1;
    historyBeforeCompute[0].subresourceRange.layerCount = 1;
    historyBeforeCompute[0].srcAccessMask = state.v5HistoryReadInitialized ? VK_ACCESS_SHADER_READ_BIT : 0;
    historyBeforeCompute[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    historyBeforeCompute[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    historyBeforeCompute[1].oldLayout = state.v5HistoryWriteInitialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    historyBeforeCompute[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    historyBeforeCompute[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    historyBeforeCompute[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    historyBeforeCompute[1].image = state.v5HistoryWriteImage;
    historyBeforeCompute[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    historyBeforeCompute[1].subresourceRange.levelCount = 1;
    historyBeforeCompute[1].subresourceRange.layerCount = 1;
    historyBeforeCompute[1].srcAccessMask = state.v5HistoryWriteInitialized ? VK_ACCESS_SHADER_READ_BIT : 0;
    historyBeforeCompute[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(
        state.cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        static_cast<std::uint32_t>(historyBeforeCompute.size()),
        historyBeforeCompute.data()
    );

    if (state.frameIndex < 4) previewLog("record v6: swapchain to general");
    VkImageMemoryBarrier toGeneral{};
    toGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toGeneral.oldLayout = state.swapchainImageInitialized ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED;
    toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toGeneral.image = state.swapchainImage;
    toGeneral.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toGeneral.subresourceRange.levelCount = 1;
    toGeneral.subresourceRange.layerCount = 1;
    toGeneral.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(
        state.cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &toGeneral
    );

    if (state.frameIndex < 4) previewLog("record v6: dispatch raytrace signal compute");
    vkCmdBindPipeline(state.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, state.v5ComputePipeline);
    vkCmdBindDescriptorSets(state.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, state.v5Layout, 0, 1, &state.v5DescriptorSet, 0, nullptr);
    vkCmdDispatch(state.cmd, (state.extent.width + 7u) / 8u, (state.extent.height + 7u) / 8u, 1);

    VkImageMemoryBarrier historyAfterCompute{};
    historyAfterCompute.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    historyAfterCompute.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    historyAfterCompute.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    historyAfterCompute.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    historyAfterCompute.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    historyAfterCompute.image = state.v5HistoryWriteImage;
    historyAfterCompute.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    historyAfterCompute.subresourceRange.levelCount = 1;
    historyAfterCompute.subresourceRange.layerCount = 1;
    historyAfterCompute.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    historyAfterCompute.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(
        state.cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &historyAfterCompute
    );

    if (state.frameIndex < 4) previewLog("record v6: swapchain to present");
    VkImageMemoryBarrier toPresent = toGeneral;
    toPresent.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toPresent.dstAccessMask = 0;
    vkCmdPipelineBarrier(
        state.cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &toPresent
    );

    if (state.frameIndex < 4) previewLog("record v6: end command buffer");
}

void recordV4Deferred(const RecordState& state) {
    recordGBufferFill(state);

    VkViewport viewport{};
    viewport.width = static_cast<float>(state.extent.width);
    viewport.height = static_cast<float>(state.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{};
    scissor.extent = state.extent;

    VkClearValue ssaoClear{};
    ssaoClear.color = {{1.0f, 0.0f, 0.0f, 0.0f}};
    VkRenderPassBeginInfo ssaoPass{};
    ssaoPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    ssaoPass.renderPass = state.ssaoPass;
    ssaoPass.framebuffer = state.ssaoFramebuffer;
    ssaoPass.renderArea.extent = state.extent;
    ssaoPass.clearValueCount = 1;
    ssaoPass.pClearValues = &ssaoClear;
    vkCmdBeginRenderPass(state.cmd, &ssaoPass, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(state.cmd, 0, 1, &viewport);
    vkCmdSetScissor(state.cmd, 0, 1, &scissor);
    vkCmdBindPipeline(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.ssaoPipeline);
    vkCmdBindDescriptorSets(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.v4Layout, 0, 1, &state.v4DescriptorSet, 0, nullptr);
    vkCmdDraw(state.cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(state.cmd);

    VkClearValue blurClear{};
    blurClear.color = {{1.0f, 0.0f, 0.0f, 0.0f}};
    VkRenderPassBeginInfo blurPass{};
    blurPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    blurPass.renderPass = state.ssaoPass;
    blurPass.framebuffer = state.ssaoBlurFramebuffer;
    blurPass.renderArea.extent = state.extent;
    blurPass.clearValueCount = 1;
    blurPass.pClearValues = &blurClear;
    vkCmdBeginRenderPass(state.cmd, &blurPass, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(state.cmd, 0, 1, &viewport);
    vkCmdSetScissor(state.cmd, 0, 1, &scissor);
    vkCmdBindPipeline(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.ssaoBlurPipeline);
    vkCmdBindDescriptorSets(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.v4Layout, 0, 1, &state.v4DescriptorSet, 0, nullptr);
    vkCmdDraw(state.cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(state.cmd);

    std::array<VkClearValue, 2> composeClears{};
    composeClears[0].color = {{0.58f, 0.62f, 0.68f, 1.0f}};
    composeClears[1].depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo composePass{};
    composePass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    composePass.renderPass = state.forwardPass;
    composePass.framebuffer = state.forwardFramebuffer;
    composePass.renderArea.extent = state.extent;
    composePass.clearValueCount = static_cast<std::uint32_t>(composeClears.size());
    composePass.pClearValues = composeClears.data();
    vkCmdBeginRenderPass(state.cmd, &composePass, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(state.cmd, 0, 1, &viewport);
    vkCmdSetScissor(state.cmd, 0, 1, &scissor);
    vkCmdBindPipeline(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.v4ComposePipeline);
    vkCmdBindDescriptorSets(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.v4Layout, 0, 1, &state.v4DescriptorSet, 0, nullptr);
    vkCmdDraw(state.cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(state.cmd);
}

void recordV2V3Forward(const RecordState& state) {
    VkViewport viewport{};
    viewport.width = static_cast<float>(state.extent.width);
    viewport.height = static_cast<float>(state.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{};
    scissor.extent = state.extent;

    std::array<VkClearValue, 2> clears{};
    clears[0].color = {{0.58f, 0.62f, 0.68f, 1.0f}};
    clears[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPass{};
    renderPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPass.renderPass = state.forwardPass;
    renderPass.framebuffer = state.forwardFramebuffer;
    renderPass.renderArea.extent = state.extent;
    renderPass.clearValueCount = static_cast<std::uint32_t>(clears.size());
    renderPass.pClearValues = clears.data();
    vkCmdBeginRenderPass(state.cmd, &renderPass, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetViewport(state.cmd, 0, 1, &viewport);
    vkCmdSetScissor(state.cmd, 0, 1, &scissor);

    vkCmdBindPipeline(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.skyPipeline);
    vkCmdBindDescriptorSets(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.forwardLayout, 0, 1, &state.fallbackDescriptorSet, 0, nullptr);
    vkCmdDraw(state.cmd, 3, 1, 0, 0);

    vkCmdBindPipeline(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.forwardPipeline);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(state.cmd, 0, 1, &state.vertexBuffer, &offset);
    if (!state.batches || state.batches->empty()) {
        vkCmdBindDescriptorSets(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.forwardLayout, 0, 1, &state.fallbackDescriptorSet, 0, nullptr);
        vkCmdDraw(state.cmd, state.vertexCount, 1, 0, 0);
    } else {
        for (const auto& batch : *state.batches) {
            if (batch.vertexCount == 0) {
                continue;
            }
            const std::uint32_t materialIndex = batch.materialIndex < state.materialDescriptorSets->size() ? batch.materialIndex : 0u;
            const VkDescriptorSet descriptorSet = (*state.materialDescriptorSets)[materialIndex];
            vkCmdBindDescriptorSets(state.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state.forwardLayout, 0, 1, &descriptorSet, 0, nullptr);
            vkCmdDraw(state.cmd, batch.vertexCount, 1, batch.firstVertex, 0);
        }
    }
    vkCmdEndRenderPass(state.cmd);
}

} // namespace vr
