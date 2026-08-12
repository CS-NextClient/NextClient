#include "engine.h"

#include <string>
#include <vector>

#include <glad/glad.h>
#include "graphics/render/RenderDevice.h"
#include <ncl_material/MaterialPreset.h>

#include "console/console.h"
#include "graphics/gl_local.h"
#include "graphics/render/MaterialTextures.h"
#include "graphics/texture_loader.h"
#include "graphics/texture_processing.h"
#include "resource/texture_manager/Texture.h"

// The tool that predicts this path answers from the shared constant, so the two cannot be
// changed apart.
static_assert(
    static_cast<int64_t>(tex::kTextureMaxSize) * tex::kTextureMaxSize == ncl_material::kMaxImagePixels,
    "the shared image ceiling and the upload path's own no longer agree"
);

namespace
{
    struct Entry
    {
        std::string path;
        GLuint texture;
    };

    std::vector<Entry> g_Textures;

    // Below the capacity of tex::TexIdentifierStr, which is a fixed_string declared not to
    // allocate: assigning a longer path aborts the process rather than growing. A model
    // document is installed content and its paths are no more trusted than a server's.
    constexpr size_t kMaxTexturePathLength = 128;

    GLuint g_Neutral;

    class MaterialTextureResources : public render::IDeviceResource
    {
    public:
        void OnDeviceLost() override
        {
            // Names only, no glDelete: the context they belong to is gone.
            g_Textures.clear();
            g_Neutral = 0;
        }
    };

    MaterialTextureResources g_Resources;
    bool g_Registered;

    void EnsureRegistered()
    {
        if (!g_Registered)
        {
            g_Registered = true;
            render::Device_Register(&g_Resources);
        }
    }
}

unsigned int render::MaterialTexture_Get(const char* path)
{
    if (path == nullptr || *path == '\0' || !render::Device_Available())
    {
        return 0;
    }

    EnsureRegistered();

    for (const Entry& entry : g_Textures)
    {
        if (entry.path == path)
        {
            return entry.texture;
        }
    }

    // Remembered exactly like a failure to load, and for the same reason: this is asked
    // once per mesh per frame, so a refusal that is not cached is a console line per frame
    // for as long as the model is on screen.
    if (!ncl_material::IsAdmissibleTexturePath(path, kMaxTexturePathLength))
    {
        Con_Printf("render: material texture \"%s\" is not an admissible path\n", path);
        g_Textures.push_back(Entry {path, 0u});

        return 0;
    }

    tex::Texture texture {};
    tex::TexIdentifierStr identifier;
    identifier.assign(path);

    // A failure is stored as texture 0 so the miss is cached too; otherwise a material
    // naming a file that is not there would hit the filesystem on every frame.
    bool ok = tex::LoadTextureFromPath(texture, identifier, path, true, GL_LINEAR_MIPMAP_LINEAR);

    if (!ok)
    {
        Con_Printf("render: material texture %s could not be loaded\n", path);
    }

    g_Textures.push_back(Entry {path, ok ? static_cast<GLuint>(texture.texnum) : 0u});

    return g_Textures.back().texture;
}

unsigned int render::MaterialTexture_Neutral()
{
    if (g_Neutral != 0 || !render::Device_Available())
    {
        return g_Neutral;
    }

    EnsureRegistered();

    GLint previous = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);

    const uint8_t white[4] = {255, 255, 255, 255};

    glGenTextures(1, &g_Neutral);
    glBindTexture(GL_TEXTURE_2D, g_Neutral);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));

    return g_Neutral;
}

void render::MaterialTextures_Clear()
{
    // Deletes, unlike OnDeviceLost: this runs on a map change with the context alive, so
    // dropping the names alone would leak a texture per material image on every map.
    for (const Entry& entry : g_Textures)
    {
        if (entry.texture != 0)
        {
            glDeleteTextures(1, &entry.texture);
        }
    }

    if (g_Neutral != 0)
    {
        glDeleteTextures(1, &g_Neutral);
        g_Neutral = 0;
    }

    g_Textures.clear();
}
