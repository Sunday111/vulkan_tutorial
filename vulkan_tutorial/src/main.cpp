#include <filesystem>
#include <string_view>

#include "application.hpp"
#include "spdlog/spdlog.h"
#include "unused_var.hpp"

int main(int argc, char** argv) {
  UnusedVar(argc);

  try {
    Application app;
    app.SetExecutableFile(std::filesystem::path(argv[0]));
    app.Run();
  } catch (const std::exception& e) {
    spdlog::critical("Unhandled exception: {}\n", e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}