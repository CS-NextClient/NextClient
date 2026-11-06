#include "GameUi/Controls/ImageFilePanel.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <FileSystem.h>
#include <goldsrc/client_api.h>
#include <tier1/strtools.h>
#include <vgui/ISurfaceNext.h>

#include <ncl_image/Decode.h>
#include <ncl_material/MaterialPreset.h>

#include "GameUi/Controls/ImageFileLayout.h"

using namespace vgui2;

namespace
{
    constexpr size_t kCacheEntries = 64;
    constexpr int kCacheSize = 160;

    bool ReadFile(IFileSystem& filesystem, const char* path, std::vector<uint8_t>& out)
    {
        FileHandle_t file = filesystem.Open(path, "rb");
        if (file == nullptr)
        {
            return false;
        }

        unsigned int size = filesystem.Size(file);

        // Bound compressed input independently of decoded dimensions.
        if (!ncl_material::IsAdmissibleImageFileSize(size))
        {
            filesystem.Close(file);
            return false;
        }

        out.resize(size);
        int read = filesystem.Read(out.data(), static_cast<int>(size), file);
        filesystem.Close(file);

        return read == static_cast<int>(size);
    }

    // Box filter rather than nearest: these images are noise and scratches, and dropping
    // texels out of them turns a fine pattern into a different, coarser one.
    void Downscale(const uint8_t* source, int source_wide, int source_tall, ImageFilePixels& out)
    {
        float scale = static_cast<float>(kCacheSize) / std::max(source_wide, source_tall);

        out.wide = std::max(1, static_cast<int>(source_wide * scale));
        out.tall = std::max(1, static_cast<int>(source_tall * scale));
        out.rgba.assign(static_cast<size_t>(out.wide) * out.tall * 4, 0);

        for (int y = 0; y < out.tall; y++)
        {
            int y0 = y * source_tall / out.tall;
            int y1 = std::max(y0 + 1, (y + 1) * source_tall / out.tall);

            for (int x = 0; x < out.wide; x++)
            {
                int x0 = x * source_wide / out.wide;
                int x1 = std::max(x0 + 1, (x + 1) * source_wide / out.wide);

                int sum[4] = {0, 0, 0, 0};
                int count = 0;

                for (int sy = y0; sy < y1; sy++)
                {
                    for (int sx = x0; sx < x1; sx++)
                    {
                        const uint8_t* texel = source + (static_cast<size_t>(sy) * source_wide + sx) * 4;

                        for (int c = 0; c < 4; c++)
                        {
                            sum[c] += texel[c];
                        }

                        count++;
                    }
                }

                uint8_t* target = out.rgba.data() + (static_cast<size_t>(y) * out.wide + x) * 4;

                for (int c = 0; c < 4; c++)
                {
                    target[c] = static_cast<uint8_t>(sum[c] / count);
                }
            }
        }
    }

    ImageFilePixels LoadInternal(const std::string& path, IFileSystem& filesystem, ISurface& owner)
    {
        ImageFilePixels stored;

        std::vector<uint8_t> file;

        if (!ReadFile(filesystem, path.c_str(), file))
        {
            return stored;
        }

        auto image = ncl_image::DecodeRgba8(file, ncl_material::kMaxImagePixels);

        if (image.has_error())
        {
            return stored;
        }

        Downscale(image->pixels.get(), image->width, image->height, stored);

        stored.texture_id = owner.CreateNewTextureID(true);

        return stored;
    }
}

std::shared_ptr<ImageFileCache> ImageFile_CreateCache(IFileSystem& filesystem, ISurface& surface)
{
    return std::make_shared<ImageFileCache>(
        kCacheEntries,
        [&filesystem, &surface](const std::string& path) { return LoadInternal(path, filesystem, surface); },
        [&surface](int texture_id) { surface.DeleteTextureByID(texture_id); }
    );
}

void ImageFile_Draw(ImageFileCache& images, ISurface& surface, const std::string& path, int x, int y, int side)
{
    if (side <= 0)
    {
        return;
    }

    ImageFilePixels* image = images.Acquire(path);

    if (image == nullptr)
    {
        return;
    }

    ImageFileBounds bounds = ImageFile_Fit(image->wide, image->tall, x, y, side);
    image->paints++;
    image->last_draw_wide = bounds.wide;
    image->last_draw_tall = bounds.tall;

    // Re-uploading restores the surface state required by RGBA drawing.
    surface.DrawSetColor(255, 255, 255, 255);
    surface.DrawSetTextureRGBA(image->texture_id, image->rgba.data(), image->wide, image->tall, 1, false);
    surface.DrawTexturedRect(bounds.x, bounds.y, bounds.x + bounds.wide, bounds.y + bounds.tall);
}

CImageFilePanel::CImageFilePanel(Panel* parent, const char* name, std::shared_ptr<ImageFileCache> images, ISurface& surface) :
    BaseClass(parent, name),
    m_Images(std::move(images)),
    m_Surface(surface)
{
    SetPaintBackgroundEnabled(false);

    // Shows an image and nothing else, so it must not sit between the pointer and whatever
    // is behind it.
    SetMouseInputEnabled(false);
}

void CImageFilePanel::SetImageFile(const char* path)
{
    m_Path = path != nullptr ? path : "";

    Repaint();
}

void CImageFilePanel::OnThink()
{
    BaseClass::OnThink();

    if (!m_Path.empty())
    {
        Repaint();
    }
}

void CImageFilePanel::Paint()
{
    if (m_Path.empty())
    {
        return;
    }

    int wide{};
    int tall{};
    GetSize(wide, tall);

    // Fitted rather than stretched: a mask is judged on its pattern, and a pattern shown
    // at the wrong aspect is a different pattern.
    int side = wide < tall ? wide : tall;
    int x = (wide - side) / 2;
    int y = (tall - side) / 2;

    ImageFile_Draw(*m_Images, m_Surface, m_Path, x, y, side);
}

void ImageFilePanel_Report(const ImageFileCache* cache, goldsrc::cl_enginefunc_t& engine)
{
    if (cache == nullptr || cache->get_images().empty())
    {
        engine.pfnConsolePrint("r_mat_images: nothing has been asked for yet\n");
        return;
    }

    for (const std::pair<const std::string, ImageFilePixels>& entry : cache->get_images())
    {
        char line[512];

        if (entry.second.texture_id == 0)
        {
            Q_snprintf(line, sizeof(line), "  %-52s NOT LOADED\n", entry.first.c_str());
        }
        else
        {
            Q_snprintf(
                line,
                sizeof(line),
                "  %-52s %3dx%-3d id %-4d painted %d times, last at %dx%d\n",
                entry.first.c_str(),
                entry.second.wide,
                entry.second.tall,
                entry.second.texture_id,
                entry.second.paints,
                entry.second.last_draw_wide,
                entry.second.last_draw_tall
            );
        }

        engine.pfnConsolePrint(line);
    }

    engine.pfnConsolePrint("r_mat_images: paint counts are since loading each cached image\n");
}
