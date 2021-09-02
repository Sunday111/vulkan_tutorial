#pragma once

#include <filesystem>

void read_file(const std::filesystem::path& path, std::vector<char>& buffer);
