#pragma once

#include <string>
#include <vector>

class Image {
public:
    Image() = default;

    bool load(const std::string& filename);
    bool save(const std::string& filename) const;

    void grayscale();
    void invert();

    int getWidth() const noexcept {
        return width_;
    }

    int getHeight() const noexcept {
        return height_;
    }

private:
    int width_ = 0;
    int height_ = 0;
    int channels_ = 4;
    std::vector<unsigned char> pixels_;
};
