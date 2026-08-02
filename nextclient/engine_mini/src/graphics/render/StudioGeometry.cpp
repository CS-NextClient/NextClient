#include "engine.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "graphics/render/GpuBuffer.h"
#include "graphics/render/RenderDevice.h"
#include <ncl_material/MaterialParse.h>
#include <ncl_studio/StudioMeshBuilder.h>
#include <studio.h>

#include "common/filesystem.h"
#include "console/console.h"
#include "graphics/render/MaterialManifests.h"
#include "graphics/render/ShaderManager.h"
#include "graphics/render/ServerMaterials.h"
#include "graphics/render/StudioGeometry.h"

namespace
{
    // Bones are byte indices in the file, so they travel as integers rather than being
    // converted to float and back in the shader. The shell normal travels as signed bytes
    // for the same kind of reason: it is a unit direction that only the glow shell reads,
    // and at full width it would have cost every studio model a third of its buffer for a
    // pass that almost no frame contains.
    struct GpuVertex
    {
        float position[3];
        float normal[3];
        float texel[2];
        uint8_t position_bone;
        uint8_t normal_bone;
        uint8_t shell_normal_bone;
        uint8_t bone_padding;
        int8_t shell_normal[3];
        int8_t padding;
    };

    // Drivers decode a signed normalised byte as value / 127 (the GL 4.2 rule; the 3.3
    // text says (2c+1)/255, which DX10-class hardware never implemented), so 127 is the
    // unit.
    int8_t QuantiseNormal(float value)
    {
        float scaled = value * 127.0f;

        return static_cast<int8_t>(std::clamp(scaled < 0.0f ? scaled - 0.5f : scaled + 0.5f, -127.0f, 127.0f));
    }

    struct Entry
    {
        std::string name;
        const studiohdr_t* header;
        bool failed;

        ncl_studio::StudioMesh mesh;
        render::GpuBuffer vertices;
        render::GpuBuffer indices;
        GLuint vao;
        render::StudioGpuModel gpu;

        // Materials are resolved once per model, against the texture header that was in
        // effect at the time; a model whose textures come from elsewhere gets its own
        // entry, so remembering which header these belong to is enough.
        ncl_material::MaterialDoc document;
        bool document_loaded;
        const studiohdr_t* material_header;
        unsigned int server_generation;
        uint32_t shader_generation;
        std::vector<ncl_material::Material> materials;

        // Kept beside the materials rather than folded into them: the material is what the
        // draw path reads, and where it came from is what an author has to be told before
        // saving a file that a higher source would beat.
        std::vector<ncl_material::MaterialOrigin> origins;
    };

    std::vector<std::unique_ptr<Entry>> g_Models;

    class GeometryResources : public render::IDeviceResource
    {
    public:
        void OnDeviceLost() override
        {
            // The buffers forget their own names; only the array objects are owned here.
            for (std::unique_ptr<Entry>& entry : g_Models)
            {
                entry->vao = 0;
            }

            g_Models.clear();
        }
    };

    GeometryResources g_Resources;
    bool g_Registered;

    void EnsureRegistered()
    {
        if (!g_Registered)
        {
            g_Registered = true;
            render::Device_Register(&g_Resources);
        }
    }

    void Release(Entry& entry)
    {
        if (entry.vao != 0 && render::Device_ContextAlive())
        {
            glDeleteVertexArrays(1, &entry.vao);
        }

        entry.vao = 0;
    }

    Entry* Find(const char* name, const studiohdr_t* header)
    {
        for (size_t i = g_Models.size(); i > 0; i--)
        {
            std::unique_ptr<Entry>& entry = g_Models[i - 1];

            // The engine reuses its model slots, so the header pointer alone is not a
            // safe identity; the name has to agree as well.
            if (entry->name != name)
            {
                continue;
            }

            if (entry->header == header)
            {
                return entry.get();
            }

            // Same model, different header: the engine reloaded it inside this map and the
            // old entry can never be found again. Dropped here rather than left to the map
            // change, which is the only other thing that would ever free it.
            Release(*entry);
            g_Models.erase(g_Models.begin() + (i - 1));
        }

        return nullptr;
    }

    // Materials live beside the model: models/v_ak47.mdl -> models/v_ak47.mat. A file
    // rather than an appendix to the .mdl, because the model CRC is taken over the whole
    // file and a server enforcing consistency would kick anyone who edited one.
    void LoadDocument(Entry& entry)
    {
        entry.document_loaded = true;

        if (entry.name.size() < 4)
        {
            return;
        }

        std::string path = entry.name.substr(0, entry.name.size() - 4) + ".mat";

        FileHandle_t file = FS_Open(path.c_str(), "rb");
        if (file == nullptr)
        {
            return;
        }

        unsigned int size = FS_Size(file);
        std::string json;
        json.resize(size);

        int read = size != 0 ? FS_Read(json.data(), static_cast<int>(size), file) : 0;
        FS_Close(file);

        if (read != static_cast<int>(size))
        {
            return;
        }

        ncl_utils::ResultT<ncl_material::MaterialDoc> parsed = ncl_material::ParseMaterialDoc(json);

        if (parsed.has_error())
        {
            // Printed rather than swallowed: a material file that does not parse leaves
            // the model looking untouched, which is indistinguishable from not having one.
            Con_Printf("render: %s: %s\n", path.c_str(), parsed.error_str().c_str());
            return;
        }

        entry.document = *parsed;

        Con_DPrintf(ConLogType::Info, "render: %s: %u materials\n", path.c_str(),
            static_cast<unsigned>(entry.document.entries.size()));
    }

    size_t CachedModelSize(const studiohdr_t* header)
    {
        return header->textureindex != 0
            ? static_cast<size_t>(header->texturedataindex)
            : static_cast<size_t>(header->length);
    }

    bool Upload(Entry& entry)
    {
        std::vector<GpuVertex> gpu_vertices;
        gpu_vertices.reserve(entry.mesh.vertices.size());

        for (const ncl_studio::Vertex& source : entry.mesh.vertices)
        {
            GpuVertex vertex {};
            vertex.position[0] = source.position.x;
            vertex.position[1] = source.position.y;
            vertex.position[2] = source.position.z;
            vertex.normal[0] = source.normal.x;
            vertex.normal[1] = source.normal.y;
            vertex.normal[2] = source.normal.z;
            vertex.shell_normal[0] = QuantiseNormal(source.shell_normal.x);
            vertex.shell_normal[1] = QuantiseNormal(source.shell_normal.y);
            vertex.shell_normal[2] = QuantiseNormal(source.shell_normal.z);
            vertex.texel[0] = source.texel.x;
            vertex.texel[1] = source.texel.y;
            vertex.position_bone = source.position_bone;
            vertex.normal_bone = source.normal_bone;
            vertex.shell_normal_bone = source.shell_normal_bone;

            gpu_vertices.push_back(vertex);
        }

        bool buffers_ok =
            entry.vertices.Create(GL_ARRAY_BUFFER, gpu_vertices.data(), gpu_vertices.size() * sizeof(GpuVertex)) &&
            entry.indices.Create(
                GL_ELEMENT_ARRAY_BUFFER,
                entry.mesh.indices.data(),
                entry.mesh.indices.size() * sizeof(uint32_t)
            );

        // Last use of both arrays. Skinning happens in the vertex shader, so the buffers
        // are filled once and never rewritten; the draw path works from the ranges beside
        // them, and r_studio_info builds a copy of its own. Kept, they would hold 48 bytes
        // per vertex for as long as the model stays cached. Swapped rather than cleared,
        // because clear() keeps the capacity.
        std::vector<ncl_studio::Vertex>().swap(entry.mesh.vertices);
        std::vector<uint32_t>().swap(entry.mesh.indices);

        if (!buffers_ok)
        {
            return false;
        }

        GLint previous_vao = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_vao);

        GLint previous_buffer = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_buffer);

        glGenVertexArrays(1, &entry.vao);
        if (entry.vao == 0)
        {
            return false;
        }

        glBindVertexArray(entry.vao);
        glBindBuffer(GL_ARRAY_BUFFER, entry.vertices.name());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entry.indices.name());

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, position)));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, normal)));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, texel)));

        // The three bone bytes are adjacent, so they arrive as one integer vector rather
        // than as an attribute each.
        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(3, 3, GL_UNSIGNED_BYTE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, position_bone)));

        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_BYTE, GL_TRUE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, shell_normal)));

        glBindVertexArray(static_cast<GLuint>(previous_vao));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_buffer));

        return true;
    }
}

const render::StudioGpuModel* render::Geometry_Get(model_t* model, const studiohdr_t* header)
{
    if (!render::Device_Available() || model == nullptr || header == nullptr)
    {
        return nullptr;
    }

    EnsureRegistered();

    if (Entry* cached = Find(model->name, header))
    {
        return cached->failed ? nullptr : &cached->gpu;
    }

    std::unique_ptr<Entry> entry = std::make_unique<Entry>();
    entry->name = model->name;
    entry->header = header;
    entry->failed = true;
    entry->vao = 0;

    Entry& stored = *entry;
    g_Models.push_back(std::move(entry));

    ncl_utils::ResultT<ncl_studio::StudioMesh> mesh = ncl_studio::BuildStudioMesh(header, CachedModelSize(header));
    if (mesh.has_error())
    {
        Con_DPrintf(ConLogType::Error, "render: %s: %s\n", stored.name.c_str(), mesh.error_str().c_str());
        return nullptr;
    }

    stored.mesh = *mesh;

    if (!Upload(stored))
    {
        Con_DPrintf(ConLogType::Error, "render: %s: failed to upload studio geometry\n", stored.name.c_str());
        return nullptr;
    }

    stored.failed = false;
    stored.gpu.vao = stored.vao;
    stored.gpu.mesh = &stored.mesh;

    return &stored.gpu;
}

const ncl_material::Material* render::Geometry_Material(
    model_t* model,
    const studiohdr_t* header,
    const studiohdr_t* texture_header,
    int texture_index
)
{
    if (model == nullptr || texture_header == nullptr || texture_index < 0)
    {
        return nullptr;
    }

    Entry* entry = Find(model->name, header);
    if (entry == nullptr || entry->failed)
    {
        return nullptr;
    }

    // The manifests are an input to what is baked below, and they are dropped and re-read
    // by the same command that reloads shaders. Without this the authoring loop silently
    // keeps the old defaults, and a material demoted under the old manifest stays demoted.
    if (entry->shader_generation != render::Shaders_Generation())
    {
        entry->shader_generation = render::Shaders_Generation();
        entry->document_loaded = false;
        entry->materials.clear();
    }

    if (!entry->document_loaded)
    {
        entry->document = ncl_material::MaterialDoc {};
        LoadDocument(*entry);
    }

    if (entry->material_header != texture_header)
    {
        entry->material_header = texture_header;
        entry->materials.clear();
    }

    if (entry->server_generation != render::ServerMaterials_Generation())
    {
        entry->server_generation = render::ServerMaterials_Generation();
        entry->materials.clear();
    }

    if (entry->materials.empty())
    {
        entry->origins.clear();
        const mstudiotexture_t* textures = reinterpret_cast<const mstudiotexture_t*>(
            reinterpret_cast<const uint8_t*>(texture_header) + texture_header->textureindex
        );

        ncl_material::MaterialSources sources;
        sources.model = &entry->document;

        for (int i = 0; i < texture_header->numtextures; i++)
        {
            ncl_material::ResolvedMaterial resolved =
                ncl_material::Resolve(sources, textures[i].name, textures[i].flags);

            ncl_material::Material material = std::move(resolved.material);
            ncl_material::MaterialOrigin origin = resolved.origin;

            // Checked here, once per model, rather than at draw time: a material that does
            // not match its program would otherwise show up as a uniform that quietly did
            // nothing, which is the hardest kind of content error to chase.
            if (material.program != ncl_material::kLegacyProgram)
            {
                const ncl_material::ProgramManifest* manifest = render::Manifest_Get(material.program);

                // A program without a manifest can be neither checked nor have its samplers
                // enumerated, so a material naming one would reach the draw path unvalidated
                // and with its texture units handed out by position. The server table
                // already refuses such an entry; a document beside a model is installed
                // content just the same, and is refused on the same ground.
                std::string reason;

                if (manifest == nullptr)
                {
                    reason = "no program \"" + material.program + "\"";
                }
                else
                {
                    ncl_utils::Result<> valid = ncl_material::Validate(material, *manifest);
                    if (valid.has_error())
                    {
                        reason = valid.error_str();
                    }
                }

                if (reason.empty())
                {
                    ncl_material::ApplyDefaults(material, *manifest);
                }
                else
                {
                    // Back to the implicit material rather than to the engine path: the
                    // model still renders correctly, and the message says what to fix.
                    Con_Printf(
                        "render: %s, texture %s: %s\n",
                        entry->name.c_str(),
                        textures[i].name,
                        reason.c_str()
                    );

                    material = ncl_material::MakeImplicitMaterial(textures[i].flags);
                    origin = ncl_material::MaterialOrigin::Implicit;
                }
            }

            entry->materials.push_back(std::move(material));
            entry->origins.push_back(origin);
        }
    }

    if (texture_index >= static_cast<int>(entry->materials.size()))
    {
        return nullptr;
    }

    return &entry->materials[texture_index];
}

namespace
{
    // The most recently prepared entry for a name. Entries are keyed by name and header
    // because the engine reuses model slots, and an editor has only the name.
    Entry* FindByName(const char* model)
    {
        if (model == nullptr)
        {
            return nullptr;
        }

        for (size_t i = g_Models.size(); i > 0; i--)
        {
            Entry& entry = *g_Models[i - 1];

            if (!entry.failed && entry.name == model)
            {
                return &entry;
            }
        }

        return nullptr;
    }

    const mstudiotexture_t* TexturesOf(const Entry& entry)
    {
        if (entry.material_header == nullptr)
        {
            return nullptr;
        }

        return reinterpret_cast<const mstudiotexture_t*>(
            reinterpret_cast<const uint8_t*>(entry.material_header) + entry.material_header->textureindex
        );
    }

    // Names rather than entry pointers, and rebuilt on demand rather than kept sorted:
    // models are prepared as they are first drawn, a list an author is reading must not
    // reorder itself under them, and a map change between the count and the names would
    // leave pointers into freed entries.
    std::vector<std::string> g_SortedNames;
}

int render::Geometry_ModelCount()
{
    g_SortedNames.clear();

    for (const std::unique_ptr<Entry>& entry : g_Models)
    {
        if (!entry->failed)
        {
            g_SortedNames.push_back(entry->name);
        }
    }

    std::sort(g_SortedNames.begin(), g_SortedNames.end());

    return static_cast<int>(g_SortedNames.size());
}

const char* render::Geometry_ModelName(int index)
{
    if (index < 0 || index >= static_cast<int>(g_SortedNames.size()))
    {
        return nullptr;
    }

    return g_SortedNames[index].c_str();
}

int render::Geometry_SkinFamilyCount(const char* model)
{
    const Entry* entry = FindByName(model);

    return entry != nullptr && entry->material_header != nullptr
        ? std::max(entry->material_header->numskinfamilies, 1)
        : 0;
}

namespace
{
    // A body value packs one submodel choice per body part, each on its own digit base, so
    // the number of distinct looks is the product. Bounded because the value is a product
    // and a model with several many-submodel parts would otherwise offer thousands of
    // combinations nobody is going to page through.
    constexpr int kMaxBodyValues = 64;

    const mstudiobodyparts_t* BodyPartsOf(const Entry& entry)
    {
        if (entry.header == nullptr || entry.header->numbodyparts <= 0)
        {
            return nullptr;
        }

        return reinterpret_cast<const mstudiobodyparts_t*>(
            reinterpret_cast<const uint8_t*>(entry.header) + entry.header->bodypartindex
        );
    }
}

int render::Geometry_BodyCount(const char* model)
{
    const Entry* entry = FindByName(model);
    const mstudiobodyparts_t* parts = entry != nullptr ? BodyPartsOf(*entry) : nullptr;

    if (parts == nullptr)
    {
        return 0;
    }

    int count = 1;

    for (int i = 0; i < entry->header->numbodyparts; i++)
    {
        int models = parts[i].nummodels > 0 ? parts[i].nummodels : 1;

        if (count > kMaxBodyValues / models)
        {
            return kMaxBodyValues;
        }

        count *= models;
    }

    return count;
}

bool render::Geometry_BodyName(const char* model, int body, char* out, int out_len)
{
    const Entry* entry = FindByName(model);
    const mstudiobodyparts_t* parts = entry != nullptr ? BodyPartsOf(*entry) : nullptr;

    if (parts == nullptr || out == nullptr || out_len <= 0 || body < 0)
    {
        return false;
    }

    std::string joined;

    for (int i = 0; i < entry->header->numbodyparts; i++)
    {
        int models = parts[i].nummodels > 0 ? parts[i].nummodels : 1;
        int base = parts[i].base > 0 ? parts[i].base : 1;
        int chosen = (body / base) % models;

        const mstudiomodel_t* submodels = reinterpret_cast<const mstudiomodel_t*>(
            reinterpret_cast<const uint8_t*>(entry->header) + parts[i].modelindex
        );

        if (!joined.empty())
        {
            joined += " / ";
        }

        joined.append(submodels[chosen].name, strnlen(submodels[chosen].name, sizeof(submodels[chosen].name)));
    }

    V_strncpy(out, joined.c_str(), out_len);

    return true;
}

int render::Geometry_TextureCount(const char* model)
{
    const Entry* entry = FindByName(model);

    return entry != nullptr && entry->material_header != nullptr ? entry->material_header->numtextures : 0;
}

const char* render::Geometry_TextureName(const char* model, int index)
{
    const Entry* entry = FindByName(model);
    if (entry == nullptr || index < 0 || index >= render::Geometry_TextureCount(model))
    {
        return nullptr;
    }

    return TexturesOf(*entry)[index].name;
}

const ncl_material::Material* render::Geometry_MaterialByName(
    const char* model,
    const char* texture_name,
    ncl_material::MaterialOrigin* origin
)
{
    Entry* entry = FindByName(model);
    if (entry == nullptr || texture_name == nullptr || entry->material_header == nullptr)
    {
        return nullptr;
    }

    const mstudiotexture_t* textures = TexturesOf(*entry);

    for (int i = 0; i < entry->material_header->numtextures; i++)
    {
        if (!ncl_material::EqualsIgnoreCaseAscii(textures[i].name, texture_name))
        {
            continue;
        }

        // The materials are baked lazily by the draw path, so a model prepared but not yet
        // drawn has none. Asking through the same entry point fills them in.
        if (i >= static_cast<int>(entry->materials.size()))
        {
            return nullptr;
        }

        if (origin != nullptr)
        {
            *origin = entry->origins[i];
        }

        return &entry->materials[i];
    }

    return nullptr;
}

void render::Geometry_ReloadDocuments()
{
    for (std::unique_ptr<Entry>& entry : g_Models)
    {
        entry->document_loaded = false;
        entry->materials.clear();
        entry->origins.clear();
    }
}

void render::Geometry_Clear()
{
    if (!render::Device_ContextAlive())
    {
        g_Models.clear();
        return;
    }

    for (std::unique_ptr<Entry>& entry : g_Models)
    {
        Release(*entry);
    }

    g_Models.clear();
}
