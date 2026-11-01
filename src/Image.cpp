#include "Image.h"

#include <cstddef>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

bool Image::load(const std::string& filename) {
    int sourceChannels = 0;
    unsigned char* loaded = stbi_load(filename.c_str(), &width_, &height_, &sourceChannels, 4);

    if (loaded == nullptr) {
        width_ = 0;
        height_ = 0;
        pixels_.clear();
        return false;
    }

    channels_ = 4;
    const std::size_t pixelCount = static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_)
            * static_cast<std::size_t>(channels_);

    pixels_.assign(loaded, loaded + pixelCount);
    stbi_image_free(loaded);
    return true;
}

bool Image::save(const std::string& filename) const {
    if (pixels_.empty() || width_ <= 0 || height_ <= 0) {
        return false;
    }

    const int stride = width_ * channels_;
    return stbi_write_png(filename.c_str(), width_, height_, channels_, pixels_.data(), stride) != 0;
}

void Image::grayscale() {
    if (pixels_.empty()) {
        return;
    }

    for (std::size_t i = 0; i < pixels_.size(); i += 4) {
        const auto gray
                = static_cast<unsigned char>(0.299 * pixels_[i] + 0.587 * pixels_[i + 1] + 0.114 * pixels_[i + 2]);

        pixels_[i] = gray;
        pixels_[i + 1] = gray;
        pixels_[i + 2] = gray;
    }
}

void Image::invert() {
    if (pixels_.empty()) {
        return;
    }

    for (std::size_t i = 0; i < pixels_.size(); i += 4) {
        pixels_[i] = static_cast<unsigned char>(255 - pixels_[i]);
        pixels_[i + 1] = static_cast<unsigned char>(255 - pixels_[i + 1]);
        pixels_[i + 2] = static_cast<unsigned char>(255 - pixels_[i + 2]);
    }
}
