#pragma once

#include <span>
#include <string_view>

#include "integer.hpp"

class ImageLoader {
 public:
  ImageLoader() = default;
  ImageLoader(const std::string_view& path);
  ImageLoader(ImageLoader&& another);
  ~ImageLoader();
  ImageLoader& operator=(ImageLoader&& another);

  void LoadFromFile(const std::string_view& path);
  void Destroy();
  void Reset();

  [[nodiscard]] ui32 GetWidth() const noexcept;
  [[nodiscard]] ui32 GetHeight() const noexcept;
  [[nodiscard]] ui32 GetChannels() const noexcept;
  [[nodiscard]] size_t GetSize() const noexcept;
  [[nodiscard]] std::span<const unsigned char> GetData() const noexcept;

 private:
  ImageLoader(const ImageLoader&) = default;
  ImageLoader& operator=(const ImageLoader&) = default;
  void MoveFrom(ImageLoader& another);

 private:
  unsigned char* pixel_data_ = nullptr;
  int width_ = -1;
  int height_ = -1;
  int channels_ = -1;
};