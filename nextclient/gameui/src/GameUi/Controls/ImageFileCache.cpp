#include "GameUi/Controls/ImageFileCache.h"

#include <algorithm>
#include <cassert>
#include <utility>

ImageFileCache::ImageFileCache(
    size_t capacity,
    std::function<ImageFilePixels(const std::string&)> load,
    std::function<void(int)> release
) :
    capacity_(capacity),
    load_(std::move(load)),
    release_(std::move(release))
{
    assert(capacity_ > 0);
}

ImageFileCache::~ImageFileCache()
{
    Clear();
}

ImageFilePixels* ImageFileCache::Acquire(const std::string& path)
{
    if (path.empty())
    {
        return nullptr;
    }

    auto image = images_.find(path);

    if (image == images_.end())
    {
        if (images_.size() == capacity_)
        {
            auto oldest = std::min_element(images_.begin(), images_.end(), [](const auto& left, const auto& right) {
                return left.second.use < right.second.use;
            });

            if (oldest->second.texture_id != 0)
            {
                release_(oldest->second.texture_id);
            }

            images_.erase(oldest);
        }

        image = images_.emplace(path, load_(path)).first;
    }

    image->second.use = ++clock_;

    return image->second.texture_id != 0 ? &image->second : nullptr;
}

void ImageFileCache::Clear()
{
    for (const auto& image : images_)
    {
        if (image.second.texture_id != 0)
        {
            release_(image.second.texture_id);
        }
    }

    images_.clear();
    clock_ = 0;
}

const std::map<std::string, ImageFilePixels>& ImageFileCache::get_images() const
{
    return images_;
}
