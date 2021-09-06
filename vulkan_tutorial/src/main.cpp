#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include "spdlog/spdlog.h"

#include <string_view>
#include <filesystem>

#include "integer.h"
#include "application.h"
#include "unused_var.h"

int main(int argc, char** argv)
{
    unused_var(argc);

    try
    {
        Application app;
        app.set_executable_file(std::filesystem::path(argv[0]));
        app.run();
    }
    catch (const std::exception& e)
    {
        spdlog::critical("Unhandled exception: {}\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}