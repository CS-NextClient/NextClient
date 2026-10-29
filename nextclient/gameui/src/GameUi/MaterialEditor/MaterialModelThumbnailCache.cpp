#include "GameUi/MaterialEditor/MaterialModelThumbnailCache.h"

#include <algorithm>

#include <Color.h>
#include <FileSystem.h>
#include <vgui/ISurfaceNext.h>

#include "GameUi/MaterialEditor/MaterialModelThumbnail.h"

namespace material_editor
{

namespace
{
    constexpr size_t kCacheEntries = 64;
    constexpr unsigned int kMaxSourceBytes = 32 * 1024 * 1024;
    constexpr int kImageSize = 128;

    bool Read(IFileSystem& filesystem, const std::string& path, std::vector<uint8_t>& bytes)
    {
        FileHandle_t file = filesystem.Open(path.c_str(), "rb");

        if (file == nullptr)
        {
            return false;
        }

        unsigned int size = filesystem.Size(file);
        bool success = size > 0 && size <= kMaxSourceBytes;

        if (success)
        {
            bytes.resize(size);
            success = filesystem.Read(bytes.data(), static_cast<int>(size), file) == static_cast<int>(size);
        }

        filesystem.Close(file);
        return success;
    }
} // namespace

MaterialModelThumbnailCache::MaterialModelThumbnailCache(IFileSystem& filesystem, vgui2::ISurface& surface) :
    filesystem_(filesystem),
    surface_(surface)
{}

void MaterialModelThumbnailCache::SetVisibleModels(const std::vector<std::string>& paths)
{
    visible_ = paths;

    for (const std::string& path : visible_)
    {
        entries_[path].use = ++clock_;
    }

    Evict();
}

void MaterialModelThumbnailCache::Evict()
{
    while (entries_.size() > kCacheEntries)
    {
        auto oldest = entries_.end();

        for (auto entry = entries_.begin(); entry != entries_.end(); ++entry)
        {
            if (std::find(visible_.begin(), visible_.end(), entry->first) == visible_.end() &&
                (oldest == entries_.end() || entry->second.use < oldest->second.use))
            {
                oldest = entry;
            }
        }

        if (oldest == entries_.end())
        {
            return;
        }

        if (oldest->second.texture != 0)
        {
            surface_.DeleteTextureByID(oldest->second.texture);
        }

        entries_.erase(oldest);
    }
}

void MaterialModelThumbnailCache::ProcessOne()
{
    for (const std::string& path : visible_)
    {
        Entry& entry = entries_.at(path);

        if (entry.ready)
        {
            continue;
        }

        entry.ready = true;
        std::vector<uint8_t> model;
        std::vector<uint8_t> companion;

        if (Read(filesystem_, path, model))
        {
            // A companion is optional; the validated model header chooses whether to use it.
            std::string texture_path = path.substr(0, path.size() - 4) + "T.mdl";
            Read(filesystem_, texture_path, companion);
            auto image = RenderModelThumbnail(model, companion);

            if (!image.has_error())
            {
                entry.texture = surface_.CreateNewTextureID(true);
                surface_.DrawSetTextureRGBA(entry.texture, image->rgba.data(), kImageSize, kImageSize, true, true);
            }
        }

        return;
    }
}

void MaterialModelThumbnailCache::Draw(const std::string& path, int x, int y, int side) const
{
    auto entry = entries_.find(path);

    if (entry == entries_.end())
    {
        return;
    }

    if (entry->second.texture != 0)
    {
        surface_.DrawSetColor(255, 255, 255, 255);
        surface_.DrawSetTexture(entry->second.texture);
        surface_.DrawTexturedRect(x, y, x + side, y + side);
    }
    else
    {
        surface_.DrawSetColor(entry->second.ready ? Color(180, 90, 90, 255) : Color(110, 110, 110, 255));
        surface_.DrawOutlinedRect(x + 4, y + 4, x + side - 4, y + side - 4);
    }
}

void MaterialModelThumbnailCache::Clear()
{
    for (const auto& entry : entries_)
    {
        if (entry.second.texture != 0)
        {
            surface_.DeleteTextureByID(entry.second.texture);
        }
    }

    entries_.clear();
    visible_.clear();
}

} // namespace material_editor
