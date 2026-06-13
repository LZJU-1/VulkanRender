#pragma once

#include "platform/CommandLine.hpp"

namespace vr {

class RendererApp {
public:
    explicit RendererApp(AppConfig config);
    int run();

private:
    AppConfig config_;
};

} // namespace vr

