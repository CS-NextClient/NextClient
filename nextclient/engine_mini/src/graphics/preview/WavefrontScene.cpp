#include "graphics/preview/WavefrontScene.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>

#include <goldsrc/format/wad.h>
#include <next_engine_mini/ScenePreviewInterface.h>

#include "common/wad.h"
#include "console/console.h"
#include "graphics/gl_local.h"
#include "graphics/preview/WavefrontParser.h"

namespace render::preview
{
    namespace
    {
        constexpr char kWadIdent[] = "WAD3";
        constexpr char kMiptexType = 0x43;

        // Read straight into the engine's own structures, which need no packing pragma.
        static_assert(sizeof(goldsrc::wadinfo_t) == 12);
        static_assert(sizeof(goldsrc::lumpinfo_t) == 32);
        static_assert(sizeof(goldsrc::miptex_t) == 40);

        // Names are stored padded to a fixed field, in whatever case the author used, while a
        // material refers to them in another; both sides go through the engine's own rule.
        bool SameName(const char* stored, std::string_view wanted)
        {
            char stored_clean[sizeof(goldsrc::lumpinfo_t::name)];
            W_CleanupName(stored, stored_clean);

            char wanted_padded[sizeof(stored_clean) + 1]{};
            std::memcpy(wanted_padded, wanted.data(), std::min(wanted.size(), sizeof(stored_clean)));

            char wanted_clean[sizeof(stored_clean)];
            W_CleanupName(wanted_padded, wanted_clean);

            return std::memcmp(stored_clean, wanted_clean, sizeof(stored_clean)) == 0;
        }

    } // namespace

    WavefrontScene::WavefrontScene(RenderDevice& device, PreviewAssetBackend& assets) :
        device_(device),
        assets_(assets)
    {
        device_.Register(this);
    }

    WavefrontScene::~WavefrontScene()
    {
        device_.Unregister(this);
        OnDeviceLost();
    }

    void WavefrontScene::OnDeviceLost()
    {
        for (Batch& batch : batches_)
        {
            batch.texnum = -1;
        }

        textures_invalid_ = true;
    }

    bool WavefrontScene::Load(const char* path, const char* const* wads, int wad_count, int tag)
    {
        path_ = path;
        wads_.assign(wads, wads + wad_count);
        tag_ = tag;

        return Read();
    }

    void WavefrontScene::Clear()
    {
        Free();
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
                if (device_.IsContextCurrent())
                {
                    assets_.UnloadTexture(batch.identifier.c_str());
                }
            }
        }

        batches_.clear();
    }

    bool WavefrontScene::Read()
    {
        std::string text = assets_.LoadText(path_.c_str());
        if (text.empty())
        {
            assets_.Report("WavefrontScene: %s not found\n", path_.c_str());
            return false;
        }

        WavefrontMesh mesh = Wavefront_Parse(text);

        int triangles = 0;

        for (WavefrontBatch& parsed : mesh.batches)
        {
            triangles += static_cast<int>(parsed.vertices.size() / 3);

            Batch batch;
            batch.texture = parsed.material;
            batch.identifier = "preview" + std::to_string(reinterpret_cast<uintptr_t>(this)) + "_" + std::to_string(tag_) + "_" +
                               std::to_string(batches_.size());
            batch.vertices = std::move(parsed.vertices);
            batches_.push_back(std::move(batch));
        }

        if (triangles == 0)
        {
            assets_.Report("WavefrontScene: %s holds no faces\n", path_.c_str());
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
                assets_.Report("WavefrontScene: %d faces without a material\n", static_cast<int>(batch.vertices.size() / 3));
            }
            else
            {
                assets_.Report("WavefrontScene: texture %s not found\n", batch.texture.c_str());
            }
        }

        assets_.Report(
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
        uint8_t* archive = assets_.LoadFile(wad_name, &length);

        if (archive == nullptr)
        {
            return;
        }

        if (length < static_cast<int>(sizeof(goldsrc::wadinfo_t)))
        {
            assets_.FreeFile(archive);
            return;
        }

        goldsrc::wadinfo_t header;
        std::memcpy(&header, archive, sizeof(header));

        // Bounded in 64 bits, like the lumps below: a corrupt count times the entry size, or a
        // corrupt position plus a size, wraps a 32 bit sum and would then admit the whole file.
        int64_t directory_end =
            static_cast<int64_t>(header.infotableofs) + static_cast<int64_t>(header.numlumps) * sizeof(goldsrc::lumpinfo_t);

        bool usable = std::memcmp(header.identification, kWadIdent, sizeof(header.identification)) == 0 && header.numlumps > 0 &&
                      header.infotableofs >= 0 && directory_end <= length;

        for (int i = 0; usable && i < header.numlumps; i++)
        {
            goldsrc::lumpinfo_t lump;
            std::memcpy(&lump, archive + header.infotableofs + i * sizeof(goldsrc::lumpinfo_t), sizeof(lump));

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
                    batch.texnum = assets_.UploadMiptex(batch.identifier.c_str(), archive + lump.filepos, lump.disksize);
                }
            }
        }

        assets_.FreeFile(archive);
    }

    void WavefrontScene::Draw(const PreviewLight& light)
    {
        if (textures_invalid_)
        {
            if (!device_.is_context_alive())
            {
                return;
            }

            LoadTextures();
            textures_invalid_ = false;
        }

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
                    -(vertex.normal[0] * light.direction[0] + vertex.normal[1] * light.direction[1] +
                      vertex.normal[2] * light.direction[2]);
                float shade = light.ambient + light.diffuse * std::max(0.0f, facing);

                qglColor4f(light.color[0] * shade, light.color[1] * shade, light.color[2] * shade, 1.0f);
                qglTexCoord2f(vertex.s, vertex.t);
                qglVertex3fv(vertex.position);
            }

            qglEnd();
        }

        qglColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

} // namespace render::preview
