#pragma once

#include <map>
#include <string>
#include <vector>

#include <tao/json.hpp>

#include "graphics/render/StudioMaterialRuntime.h"
#include "graphics/render/StudioQueue.h"
#include "graphics/render/material/MaterialCoverageRuntime.h"

class StudioProgramSourceFixture : public render::ShaderBackend
{
public:
    render::RenderDevice device;
    render::ShaderManager manager{device, *this};
    const ncl_gl::Program* program{};
    unsigned int resolutions{};
    unsigned int source_loads{};
    std::vector<std::string> names;
    std::vector<render::ProgramDesc> descriptors;
    std::map<std::string, std::string> sources;
    std::vector<unsigned int> deletions;
    int ready{};
    int compiled{};

    StudioProgramSourceFixture()
    {
        device.OnContextCreated();
    }

    bool LoadSource(const char* name, std::string& out) override
    {
        ++source_loads;
        auto found = sources.find(name);
        if (found == sources.end())
        {
            return false;
        }
        out = found->second;
        return true;
    }

    bool Build(const render::ProgramDesc& desc, ncl_gl::Program& result) override
    {
        ++resolutions;
        descriptors.push_back(desc);
        if (program == nullptr)
        {
            return false;
        }
        result = *program;
        return true;
    }

    std::vector<std::string> ListPrograms() override
    {
        return names;
    }
    void DeleteProgram(unsigned int name) override
    {
        deletions.push_back(name);
    }
    double Time() override
    {
        return 0;
    }
    void ReportWarmup(int ready_count, int, int compiled_count, double) override
    {
        ready = ready_count;
        compiled = compiled_count;
    }
};

class StudioMaterialSourceFixture : public render::ShaderBackend, public render::ManifestBackend, public render::MaterialImageLoader
{
public:
    render::RenderDevice device;
    render::ShaderManager shaders{device, *this};
    render::MaterialManifestCatalog catalog{shaders, *this};
    render::MaterialTextureCache cache{device, *this};
    std::map<std::string, ncl_material::ProgramManifest> manifests;
    std::map<std::string, GLuint> textures;
    std::vector<std::string> manifest_queries;
    std::vector<std::string> texture_queries;
    std::vector<std::string> localization;
    std::vector<std::string> errors;
    std::vector<GLuint> deletions;
    GLuint neutral{};
    unsigned int neutral_queries{};
    unsigned int rejected{};

    StudioMaterialSourceFixture()
    {
        device.OnContextCreated();
    }

    bool LoadSource(const char* name, std::string& out) override
    {
        std::string path(name);
        size_t suffix = path.rfind('.');
        std::string program = path.substr(0, suffix);
        auto found = manifests.find(program);
        if (found == manifests.end())
        {
            return false;
        }
        if (!path.ends_with(".json"))
        {
            out = "fixture source";
            return true;
        }
        manifest_queries.push_back(program);
        tao::json::value parameters = tao::json::empty_object;
        for (const ncl_material::ParamSchema& schema : found->second.parameters)
        {
            tao::json::value value = tao::json::empty_object;
            value["type"] = schema.type == ncl_material::ParamType::TextureRef ? "texture"
                            : schema.type == ncl_material::ParamType::Vec4     ? "vec4"
                                                                               : "scalar";
            if (schema.has_default)
            {
                if (schema.type == ncl_material::ParamType::TextureRef)
                {
                    value["default"] = schema.default_value.texture_ref;
                }
                else if (schema.type == ncl_material::ParamType::Vec4)
                {
                    value["default"] = tao::json::value::array(
                        {schema.default_value.vec4[0],
                         schema.default_value.vec4[1],
                         schema.default_value.vec4[2],
                         schema.default_value.vec4[3]}
                    );
                }
                else
                {
                    value["default"] = schema.default_value.scalar;
                }
            }
            value["instance"] = schema.instance;
            if (schema.has_range)
            {
                value["min"] = schema.min_value;
                value["max"] = schema.max_value;
            }
            if (!schema.label.empty())
            {
                value["label"] = schema.label;
            }
            if (!schema.hint.empty())
            {
                value["hint"] = schema.hint;
            }
            if (schema.widget == ncl_material::ParamWidget::Color)
            {
                value["widget"] = "color";
            }
            else if (schema.widget == ncl_material::ParamWidget::Choice)
            {
                value["widget"] = "choice";
                value["choices"] = tao::json::empty_array;
                for (const std::string& choice : schema.choices)
                {
                    value["choices"].push_back(choice);
                }
            }
            parameters[schema.name] = std::move(value);
        }
        tao::json::value document = {{"version", 1}, {"program", program}, {"parameters", std::move(parameters)}};
        if (!found->second.strings.empty())
        {
            document["strings"] = found->second.strings;
        }
        if (!found->second.render_contract.empty())
        {
            document["render_contract"] = found->second.render_contract;
        }
        if (found->second.max_overlays != 0)
        {
            document["overlays"] = {{"max", found->second.max_overlays}};
        }
        tao::json::value groups = tao::json::empty_array;
        for (const ncl_material::ParamGroup& group : found->second.groups)
        {
            if (group.name.empty())
            {
                continue;
            }
            tao::json::value members = tao::json::empty_array;
            for (size_t index : group.params)
            {
                members.push_back(found->second.parameters[index].name);
            }
            tao::json::value value = {{"name", group.name}, {"params", std::move(members)}};
            if (!group.label.empty())
            {
                value["label"] = group.label;
            }
            groups.push_back(std::move(value));
        }
        if (!groups.get_array().empty())
        {
            document["groups"] = std::move(groups);
        }
        out = tao::json::to_string(document);
        return true;
    }

    bool Build(const render::ProgramDesc&, ncl_gl::Program&) override
    {
        return false;
    }
    std::vector<std::string> ListPrograms() override
    {
        return {};
    }
    void DeleteProgram(unsigned int) override {}
    double Time() override
    {
        return 0;
    }
    void ReportWarmup(int, int, int, double) override {}
    void RegisterLocalization(const char* path) override
    {
        localization.emplace_back(path);
    }
    void ReportError(const std::string&, const std::string& error) override
    {
        errors.push_back(error);
    }

    GLuint Load(const char* path, tex::TextureLoadGuard) override
    {
        texture_queries.emplace_back(path);
        auto found = textures.find(path);
        return found != textures.end() ? found->second : 0;
    }

    GLuint CreateNeutral() override
    {
        ++neutral_queries;
        return neutral;
    }

    void DeleteTexture(unsigned int texture) override
    {
        deletions.push_back(texture);
    }
    void ReportRejected(const char*) override
    {
        ++rejected;
    }
};

class PreparedImageTextureState
{
private:
    GLint active_{};
    GLint texture_{};

public:
    PreparedImageTextureState()
    {
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active_);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_);
    }
    ~PreparedImageTextureState()
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_));
        glActiveTexture(static_cast<GLenum>(active_));
    }
};

inline int UploadPreparedMaterial(
    render::StudioMaterialRuntime& runtime,
    const render::StudioMaterialBindings& bindings,
    const ncl_material::Material& material,
    render::StudioMaterialState& state
)
{
    render::StudioMeshImages images;
    {
        PreparedImageTextureState saved;
        EXPECT_TRUE(runtime.PrepareImages(bindings, material, images));
    }
    return runtime.UploadMaterial(bindings, material, images, state);
}

inline void PreparePacketImages(render::StudioMaterialRuntime& runtime, render::StudioDrawPacket& packet)
{
    PreparedImageTextureState saved;
    for (render::StudioQueuedMesh& mesh : packet.meshes)
    {
        if (mesh.material)
        {
            EXPECT_TRUE(runtime.PrepareImages(*mesh.material_bindings, *mesh.material, mesh.images));
        }
    }
    packet.texture_revision = runtime.get_texture_revision();
}

inline void PrepareCoveragePacket(render::MaterialCoverageRuntime& coverage, render::StudioDrawPacket& packet)
{
    PreparedImageTextureState saved;
    for (render::StudioQueuedMesh& mesh : packet.meshes)
    {
        const ncl_material::Material* material = mesh.material ? &*mesh.material : nullptr;
        EXPECT_TRUE(coverage.PrepareImages(material, coverage.ManifestFor(material), mesh.images));
    }
    packet.texture_revision = coverage.get_texture_revision();
}

inline void UploadPreparedCoverage(
    render::MaterialCoverageRuntime& coverage,
    const render::MaterialCoverageBindings& bindings,
    const ncl_material::Material* material,
    const std::array<int, 4>& viewport,
    float time,
    render::StudioMaterialState& state
)
{
    render::StudioMeshImages images;
    const ncl_material::ProgramManifest* manifest = coverage.ManifestFor(material);
    {
        PreparedImageTextureState saved;
        EXPECT_TRUE(coverage.PrepareImages(material, manifest, images));
    }
    coverage.Upload(bindings, material, manifest, viewport, time, state, images.coverage_dissolve_map);
}
