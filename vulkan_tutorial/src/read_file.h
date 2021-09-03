#pragma once

#include <filesystem>
#include <vector>

void read_file(const std::filesystem::path& path, std::vector<char>& buffer);
