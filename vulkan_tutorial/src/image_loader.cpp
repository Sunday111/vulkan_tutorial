#include "image_loader.h"

#include <stdexcept>

#include "fmt/format.h"
#include "stb/stb_image.h"

void ImageLoader::MoveFrom(ImageLoader& another) {
  *this = another;
  another.Reset();
}

ImageLoader::ImageLoader(const std::string_view& path) { LoadFromFile(path); }

ImageLoader::ImageLoader(ImageLoader&& another) { MoveFrom(another); }

ImageLoader& ImageLoader::operator=(ImageLoader&& another) {
  Destroy();
  MoveFrom(another);
  return *this;
}

ImageLoader::~ImageLoader() { Destroy(); }

void ImageLoader::LoadFromFile(const std::string_view& path) {
  Destroy();
  pixel_data_ =
      stbi_load(path.data(), &width_, &height_, &channels_, STBI_rgb_alpha);

  [[unlikely]] if (!pixel_data_) {
    throw std::runtime_error(
        fmt::format("Failed to load texture from file {}", path));
  }
}

void ImageLoader::Destroy() {
  if (pixel_data_) {
    stbi_image_free(pixel_data_);
    Reset();
  }
}

void ImageLoader::Reset() {
  pixel_data_ = nullptr;
  width_ = -1;
  height_ = -1;
  channels_ = -1;
}

ui32 ImageLoader::GetWidth() const noexcept {
  return static_cast<ui32>(width_);
}
ui32 ImageLoader::GetHeight() const noexcept {
  return static_cast<ui32>(height_);
}
ui32 ImageLoader::GetChannels() const noexcept { return 4; }
size_t ImageLoader::GetSize() const noexcept {
  return GetWidth() * GetHeight() * GetChannels();
}
std::span<const unsigned char> ImageLoader::GetData() const noexcept {
  return std::span(pixel_data_, pixel_data_ + GetSize());
}