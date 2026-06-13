#include "core/RendererApp.hpp"
#include "platform/CommandLine.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        vr::RendererApp app(vr::CommandLine::parse(argc, argv));
        return app.run();
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        vr::CommandLine::printHelp(std::cerr);
        return 1;
    }
}

