#include "ImageFilePanel.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <FileSystem.h>
#include <vgui/ISurfaceNext.h>

#include "GameUi.h"

#include <stb/stb_image.h>

using namespace vgui2;

struct ImageFilePixels
{
    int texture_id = 0;
    int wide = 0;
    int tall = 0;
    std::vector<unsigned char> rgba;

    // Diagnostic only, and mutable so a const image can still count its own paints: what
    // is asked of it is whether it is being drawn at all.
    mutable int paints = 0;
    mutable int last_draw_wide = 0;
    mutable int last_draw_tall = 0;
};

namespace
{
    std::map<std::string, ImageFilePixels> g_Images;

    // The renderer's own ceiling for a material image. This path does not go through the
    // engine's loader, so it does not inherit that check, and an image referenced by
    // somebody else's skin arrives at whatever size its author chose.
    constexpr int kMaxSize = 1024;

    // What is kept and handed to the surface. Nothing here is shown larger than a preview,
    // and the pixels are re-set on every paint, so carrying a full-sized skin around would
    // be paying a megabyte a frame to draw a thumbnail.
    constexpr int kCacheSize = 160;

    bool ReadFile(const char* path, std::vector<unsigned char>& out)
    {
        FileHandle_t file = g_pFullFileSystem->Open(path, "rb");
        if (file == nullptr)
        {
            return false;
        }

        unsigned int size = g_pFullFileSystem->Size(file);

        // Loosely bounded by what a decoded image of the largest accepted size would take.
        // The point is not the exact number but that a file claiming to be enormous is
        // refused before it is read into memory.
        if (size == 0 || size > kMaxSize * kMaxSize * 4)
        {
            g_pFullFileSystem->Close(file);
            return false;
        }

        out.resize(size);
        int read = g_pFullFileSystem->Read(out.data(), static_cast<int>(size), file);
        g_pFullFileSystem->Close(file);

        return read == static_cast<int>(size);
    }

    // Box filter rather than nearest: these images are noise and scratches, and dropping
    // texels out of them turns a fine pattern into a different, coarser one.
    void Downscale(const unsigned char* source, int source_wide, int source_tall, ImageFilePixels& out)
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
                        const unsigned char* texel = source + (static_cast<size_t>(sy) * source_wide + sx) * 4;

                        for (int c = 0; c < 4; c++)
                        {
                            sum[c] += texel[c];
                        }

                        count++;
                    }
                }

                unsigned char* target = out.rgba.data() + (static_cast<size_t>(y) * out.wide + x) * 4;

                for (int c = 0; c < 4; c++)
                {
                    target[c] = static_cast<unsigned char>(sum[c] / count);
                }
            }
        }
    }

    const ImageFilePixels* LoadInternal(const char* path)
    {
        if (path == nullptr || *path == '\0')
        {
            return nullptr;
        }

        std::map<std::string, ImageFilePixels>::iterator cached = g_Images.find(path);
        if (cached != g_Images.end())
        {
            return cached->second.texture_id != 0 ? &cached->second : nullptr;
        }

        // A failure is remembered as well, or a panel naming a file that is not there would
        // hit the filesystem on every frame it is visible.
        ImageFilePixels& stored = g_Images[path];

        std::vector<unsigned char> file;
        if (!ReadFile(path, file))
        {
            return nullptr;
        }

        int wide = 0;
        int tall = 0;
        int components = 0;
        stbi_uc* pixels = stbi_load_from_memory(
            file.data(),
            static_cast<int>(file.size()),
            &wide,
            &tall,
            &components,
            STBI_rgb_alpha
        );

        if (pixels == nullptr)
        {
            return nullptr;
        }

        if (wide <= 0 || tall <= 0 || wide > kMaxSize || tall > kMaxSize)
        {
            stbi_image_free(pixels);
            return nullptr;
        }

        Downscale(pixels, wide, tall, stored);
        stbi_image_free(pixels);

        stored.texture_id = surface()->CreateNewTextureID(true);

        return &stored;
    }
}

const ImageFilePixels* ImageFile_Get(const char* path)
{
    return LoadInternal(path);
}

void ImageFile_Draw(const ImageFilePixels* image, int x, int y, int side)
{
    if (image == nullptr || side <= 0)
    {
        return;
    }

    image->paints++;
    image->last_draw_wide = side;
    image->last_draw_tall = side;

    // The pixels go in on every paint rather than only when they change. Handing the
    // surface a texture id it already holds is what puts the draw state back where an RGBA
    // image needs it; binding by id alone leaves whatever was drawn before it.
    surface()->DrawSetColor(255, 255, 255, 255);
    surface()->DrawSetTextureRGBA(image->texture_id, image->rgba.data(), image->wide, image->tall, 1, false);
    surface()->DrawTexturedRect(x, y, x + side, y + side);
}

CImageFilePanel::CImageFilePanel(Panel* parent, const char* name) :
    BaseClass(parent, name)
{
    m_pImage = nullptr;

    SetPaintBackgroundEnabled(false);

    // Shows an image and nothing else, so it must not sit between the pointer and whatever
    // is behind it.
    SetMouseInputEnabled(false);
}

void CImageFilePanel::SetImageFile(const char* path)
{
    m_pImage = LoadInternal(path);

    Repaint();
}

void CImageFilePanel::OnThink()
{
    BaseClass::OnThink();

    if (m_pImage != nullptr)
    {
        Repaint();
    }
}

void CImageFilePanel::Paint()
{
    if (m_pImage == nullptr)
    {
        return;
    }

    int wide = 0;
    int tall = 0;
    GetSize(wide, tall);

    // Fitted rather than stretched: a mask is judged on its pattern, and a pattern shown
    // at the wrong aspect is a different pattern.
    int side = wide < tall ? wide : tall;
    int x = (wide - side) / 2;
    int y = (tall - side) / 2;

    ImageFile_Draw(m_pImage, x, y, side);
}

void ImageFilePanel_Report()
{
    if (g_Images.empty())
    {
        engine->pfnConsolePrint("r_mat_images: nothing has been asked for yet\n");
        return;
    }

    for (const std::pair<const std::string, ImageFilePixels>& entry : g_Images)
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

        engine->pfnConsolePrint(line);
        entry.second.paints = 0;
    }

    engine->pfnConsolePrint("r_mat_images: paint counts are since the previous report\n");
}
