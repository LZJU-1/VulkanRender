#pragma once

#include "rhi/RenderDevice.hpp"

#include <memory>

namespace vr {

std::unique_ptr<RenderDevice> createVulkanDevice(bool enableValidation);

} // namespace vr

