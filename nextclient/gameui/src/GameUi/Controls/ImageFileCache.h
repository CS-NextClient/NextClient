#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

struct ImageFilePixels
{
    int texture_id{};
    int wide{};
    int tall{};
    std::vector<uint8_t> rgba;
    uint64_t use{};
    int paints{};
    int last_draw_wide{};
    int last_draw_tall{};
};

class ImageFileCache
{
private:
    size_t capacity_{};
    std::function<ImageFilePixels(const std::string&)> load_;
    std::function<void(int)> release_;
    std::map<std::string, ImageFilePixels> images_;
    uint64_t clock_{};

public:
    ImageFileCache(
        size_t capacity,
        std::function<ImageFilePixels(const std::string&)> load,
        std::function<void(int)> release
    );
    ~ImageFileCache();
    ImageFileCache(const ImageFileCache&) = delete;
    ImageFileCache& operator=(const ImageFileCache&) = delete;

    // The returned image remains valid until the next Acquire or Clear.
    ImageFilePixels* Acquire(const std::string& path);
    void Clear();
    const std::map<std::string, ImageFilePixels>& get_images() const;
};
