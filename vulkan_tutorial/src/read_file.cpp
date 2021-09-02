#include "read_file.h"

#include <fstream>

#include "fmt/format.h"

void read_file(const std::filesystem::path& path, std::vector<char>& buffer)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    [[unlikely]]
    if (!file.is_open())
    {
        auto message = fmt::format("failed to open file {}", path.string());
        throw std::runtime_error(std::move(message));
    }
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    buffer.resize(size);

    [[unlikely]]
    if (!file.read(buffer.data(), size))
    {
        auto message = fmt::format("failed to read {} bytes from file {}", size, path.string());
        throw std::runtime_error(std::move(message));
    }
}
