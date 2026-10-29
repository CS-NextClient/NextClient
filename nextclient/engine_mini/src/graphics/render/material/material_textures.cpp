#include "graphics/render/material/material_textures.h"


#include <optional>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <ncl_material/MaterialPreset.h>

#include "engine.h"
#include "console/console.h"
#include "graphics/gl_local.h"
#include "graphics/render/StudioDraw.h"
#include "graphics/render/ScopedGlState.h"
#include "graphics/render/render_resources.h"
#include "graphics/texture_loader.h"
#include "graphics/texture_processing.h"
#include "resource/texture_manager/Texture.h"

namespace render
{
    static_assert(static_cast<int64_t>(tex::kTextureMaxSize) * tex::kTextureMaxSize == ncl_material::kMaxImagePixels);

    namespace
    {
        constexpr uint8_t kNeutralPixel[4] = {255, 255, 255, 255};

        class NativeMaterialImageLoader final : public MaterialImageLoader
        {
        public:
            unsigned int Load(const char* path, tex::TextureLoadGuard guard) override
            {
                std::optional<ScopedGlState> saved;
                if (guard.device != nullptr)
                {
                    saved.emplace(*guard.device);
                }
                tex::Texture texture{};
                tex::TexIdentifierStr identifier;
                identifier.assign(path);

                // A failure is stored as texture 0 so the miss is cached too; otherwise a material
                // naming a file that is not there would hit the filesystem on every frame.
                bool ok = tex::LoadTextureFromPath(texture, identifier, path, true, GL_LINEAR_MIPMAP_LINEAR, guard);

                if (!guard.is_current())
                {
                    return 0;
                }

                if (!ok)
                {
                    Con_Printf("render: material texture %s could not be loaded\n", path);
                }

                return ok ? static_cast<unsigned int>(texture.texnum) : 0u;
            }

            unsigned int CreateNeutral() override
            {
                GLuint texture = 0;
                GLint previous = 0;
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);

                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, kNeutralPixel);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));

                return texture;
            }

            void DeleteTexture(unsigned int texture) override
            {
                glDeleteTextures(1, &texture);
            }

            void ReportRejected(const char* path) override
            {
                Con_Printf("render: material texture \"%s\" is not an admissible path\n", path);
            }
        };
    } // namespace

    MaterialImageLoader& GetMaterialImageLoader()
    {
        static NativeMaterialImageLoader loader;
        return loader;
    }

    unsigned int MaterialTexture_Get(const char* path)
    {
        return GetResources().textures.Get(path);
    }

    unsigned int MaterialTexture_Neutral()
    {
        return GetResources().textures.Neutral();
    }

    void MaterialTextures_Forget()
    {
        GetResources().textures.Forget();
    }

    void MaterialTextures_Clear()
    {
        Studio_Flush(StudioFlushReason::Resource);
        GetResources().textures.Clear();
    }
} // namespace render
