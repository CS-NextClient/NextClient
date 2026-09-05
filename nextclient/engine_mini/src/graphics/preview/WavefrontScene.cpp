#include "graphics/preview/WavefrontScene.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>

#include <next_engine_mini/ScenePreviewInterface.h>

#include "engine.h"
#include "common/wad.h"
#include "console/console.h"
#include "graphics/gl_local.h"
#include "graphics/preview/WavefrontParser.h"

namespace
{
    constexpr char kWadIdent[] = "WAD3";
    constexpr char kMiptexType = 0x43;
    constexpr int kMipLevels = 4;
    constexpr int kPaletteColors = 256;

    // Read straight into the engine's own structures, which need no packing pragma.
    static_assert(sizeof(wadinfo_t) == 12);
    static_assert(sizeof(lumpinfo_s) == 32);
    static_assert(sizeof(miptex_t) == 40);

    // Names are stored padded to a fixed field, in whatever case the author used, while a
    // material refers to them in another; both sides go through the engine's own rule.
    bool SameName(const char* stored, std::string_view wanted)
    {
        char stored_clean[sizeof(lumpinfo_s::name)];
        W_CleanupName(stored, stored_clean);

        char wanted_padded[sizeof(stored_clean) + 1]{};
        std::memcpy(wanted_padded, wanted.data(), std::min(wanted.size(), sizeof(stored_clean)));

        char wanted_clean[sizeof(stored_clean)];
        W_CleanupName(wanted_padded, wanted_clean);

        return std::memcmp(stored_clean, wanted_clean, sizeof(stored_clean)) == 0;
    }

    // A miptex holds eight bit indices for four mip levels and the palette behind the last of
    // them. System lifetime: a world texture is dropped with the map, and the number kept
    // here would then name whatever took its slot. Returns -1 for a lump that holds none.
    int LoadMiptex(const char* identifier, uint8_t* lump, int lump_size)
    {
        if (lump_size < static_cast<int>(sizeof(miptex_t)))
        {
            return -1;
        }

        miptex_t header;
        std::memcpy(&header, lump, sizeof(header));

        int wide = static_cast<int>(header.width);
        int tall = static_cast<int>(header.height);

        if (wide <= 0 || tall <= 0)
        {
            return -1;
        }

        uint32_t palette_offset = header.offsets[kMipLevels - 1] + (header.width / 8) * (header.height / 8);

        // Bounded in 64 bits: the offsets are unsigned 32 bit and a corrupt one wraps, so an
        // end past the lump would come out negative and pass as an int comparison.
        int64_t pixels_end = static_cast<int64_t>(header.offsets[0]) + static_cast<int64_t>(wide) * tall;
        int64_t palette_end = static_cast<int64_t>(header.offsets[kMipLevels - 1]) +
                              static_cast<int64_t>(header.width / 8) * (header.height / 8) + 2 + kPaletteColors * 3;

        if (pixels_end > lump_size || palette_end > lump_size)
        {
            return -1;
        }

        return GL_LoadTexture(
            identifier,
            GLT_SYSTEM,
            wide,
            tall,
            lump + header.offsets[0],
            1,
            static_cast<int>(TextureFormat::Opaque),
            lump + palette_offset + 2
        );
    }

    std::string LoadText(const char* path)
    {
        int length = 0;
        uint8_t* data = gEngfuncs.COM_LoadFile(path, 5, &length);

        if (data == nullptr)
        {
            return {};
        }

        std::string text(reinterpret_cast<const char*>(data), static_cast<size_t>(length));
        gEngfuncs.COM_FreeFile(data);

        return text;
    }
} // namespace

WavefrontScene::~WavefrontScene()
{
    Free();
}

bool WavefrontScene::Load(const char* path, const char* const* wads, int wad_count, int tag)
{
    path_ = path;
    wads_.assign(wads, wads + wad_count);
    tag_ = tag;

    return Read();
}

bool WavefrontScene::Reload()
{
    Free();

    return Read();
}

void WavefrontScene::Free()
{
    for (const Batch& batch : batches_)
    {
        if (batch.texnum >= 0)
        {
            GL_UnloadTexture(batch.identifier.c_str());
        }
    }

    batches_.clear();
}

bool WavefrontScene::Read()
{
    std::string text = LoadText(path_.c_str());
    if (text.empty())
    {
        Con_Printf("WavefrontScene: %s not found\n", path_.c_str());
        return false;
    }

    WavefrontMesh mesh = Wavefront_Parse(text);

    int triangles = 0;

    for (WavefrontBatch& parsed : mesh.batches)
    {
        triangles += static_cast<int>(parsed.vertices.size() / 3);

        Batch batch;
        batch.texture = parsed.material;
        batch.identifier = "preview" + std::to_string(tag_) + "_" + parsed.material;
        batch.vertices = std::move(parsed.vertices);
        batches_.push_back(std::move(batch));
    }

    if (triangles == 0)
    {
        Con_Printf("WavefrontScene: %s holds no faces\n", path_.c_str());
        batches_.clear();
        return false;
    }

    LoadTextures();

    int missing = 0;

    for (const Batch& batch : batches_)
    {
        if (batch.texnum >= 0)
        {
            continue;
        }

        missing++;

        if (batch.texture.empty())
        {
            Con_Printf("WavefrontScene: %d faces without a material\n", static_cast<int>(batch.vertices.size() / 3));
        }
        else
        {
            Con_Printf("WavefrontScene: texture %s not found\n", batch.texture.c_str());
        }
    }

    Con_Printf(
        "WavefrontScene: %s: %d triangles in %d batches, %d textures missing, %d faces skipped\n",
        path_.c_str(),
        triangles,
        static_cast<int>(batches_.size()),
        missing,
        mesh.skipped_faces
    );

    return true;
}

void WavefrontScene::LoadTextures()
{
    for (const std::string& wad : wads_)
    {
        bool complete = std::all_of(batches_.begin(), batches_.end(), [](const Batch& batch) { return batch.texnum >= 0; });
        if (complete)
        {
            break;
        }

        LoadTexturesFrom(wad.c_str());
    }
}

void WavefrontScene::LoadTexturesFrom(const char* wad_name)
{
    int length = 0;
    uint8_t* archive = gEngfuncs.COM_LoadFile(wad_name, 5, &length);

    if (archive == nullptr)
    {
        return;
    }

    if (length < static_cast<int>(sizeof(wadinfo_t)))
    {
        gEngfuncs.COM_FreeFile(archive);
        return;
    }

    wadinfo_t header;
    std::memcpy(&header, archive, sizeof(header));

    // Bounded in 64 bits, like the lumps below: a corrupt count times the entry size, or a
    // corrupt position plus a size, wraps a 32 bit sum and would then admit the whole file.
    int64_t directory_end = static_cast<int64_t>(header.infotableofs) + static_cast<int64_t>(header.numlumps) * sizeof(lumpinfo_s);

    bool usable = std::memcmp(header.identification, kWadIdent, sizeof(header.identification)) == 0 && header.numlumps > 0 &&
                  header.infotableofs >= 0 && directory_end <= length;

    for (int i = 0; usable && i < header.numlumps; i++)
    {
        lumpinfo_s lump;
        std::memcpy(&lump, archive + header.infotableofs + i * sizeof(lumpinfo_s), sizeof(lump));

        if (lump.type != kMiptexType || lump.compression != 0)
        {
            continue;
        }

        if (lump.filepos < 0 || lump.disksize < 0 || static_cast<int64_t>(lump.filepos) + lump.disksize > length)
        {
            continue;
        }

        for (Batch& batch : batches_)
        {
            if (batch.texnum < 0 && SameName(lump.name, batch.texture))
            {
                batch.texnum = LoadMiptex(batch.identifier.c_str(), archive + lump.filepos, lump.disksize);
            }
        }
    }

    gEngfuncs.COM_FreeFile(archive);
}

void WavefrontScene::Draw(const PreviewLight& light) const
{
    qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    for (const Batch& batch : batches_)
    {
        // Left out rather than drawn with whatever the last bind left behind.
        if (batch.texnum < 0)
        {
            continue;
        }

        GL_Bind(batch.texnum);

        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        qglBegin(GL_TRIANGLES);

        for (const WavefrontVertex& vertex : batch.vertices)
        {
            float facing =
                -(vertex.normal[0] * light.direction[0] + vertex.normal[1] * light.direction[1] + vertex.normal[2] * light.direction[2]);
            float shade = light.ambient + light.diffuse * std::max(0.0f, facing);

            qglColor4f(light.color[0] * shade, light.color[1] * shade, light.color[2] * shade, 1.0f);
            qglTexCoord2f(vertex.s, vertex.t);
            qglVertex3fv(vertex.position);
        }

        qglEnd();
    }

    qglColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}
