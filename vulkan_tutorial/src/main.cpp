#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include "fmt/format.h"

#include <string_view>

#include "integer.h"
#include "application.h"

int main()
{
    try
    {
        Application app;
        app.run();
    }
    catch (const std::exception& e)
    {
        fmt::print("Exception: {}\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
