#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class IFileSystem;

namespace vgui2
{
    class ISurface;
}

namespace material_editor
{
class MaterialModelThumbnailCache
{
private:
    struct Entry
    {
        int texture{};
        uint64_t use{};
        bool ready{};
    };

    IFileSystem& filesystem_;
    vgui2::ISurface& surface_;
    std::map<std::string, Entry> entries_;
    std::vector<std::string> visible_;
    uint64_t clock_{};

public:
    MaterialModelThumbnailCache(IFileSystem& filesystem, vgui2::ISurface& surface);
    void SetVisibleModels(const std::vector<std::string>& paths);
    void ProcessOne();
    void Draw(const std::string& path, int x, int y, int side) const;
    void Clear();

private:
    void Evict();
};

} // namespace material_editor
