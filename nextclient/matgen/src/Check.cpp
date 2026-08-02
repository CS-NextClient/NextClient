#include "Check.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <ncl_material/MaterialParse.h>
#include <ncl_material/MaterialResolve.h>
#include <ncl_material/MaterialPreset.h>
#include <tao/json.hpp>
#include <stb/stb_image.h>

#include "Manifests.h"
#include "Model.h"

namespace matgen
{
    namespace
    {
        // The client's own ceiling for an image it will upload. A material that names a
        // larger one draws nothing, and finding that out in somebody else's game is the
        // whole reason this command exists.
        constexpr int kTextureMaxSize = 1024;

        // The renderer bounds a material image path at this; the server table is stricter
        // still, and the tool reports both because a document that only the client accepts
        // cannot be handed to a server.
        constexpr size_t kClientPathLength = 128;

        struct Report
        {
            int problems = 0;

            void Fail(const std::string& message)
            {
                std::printf("  ! %s\n", message.c_str());
                problems++;
            }
        };

        void CheckTexture(const std::string& game_dir, const std::string& path, Report& report)
        {
            if (!ncl_material::IsAdmissibleTexturePath(path, kClientPathLength))
            {
                report.Fail("\"" + path + "\" is not a path the client will open");
                return;
            }

            ncl_material::PresetBudget budget;
            if (!ncl_material::IsAdmissibleTexturePath(path, budget.max_path_length))
            {
                std::printf(
                    "  - \"%s\" is longer than a preset allows (%zu), so only a document beside the model can name it\n",
                    path.c_str(),
                    budget.max_path_length
                );
            }

            std::filesystem::path full = std::filesystem::path(game_dir) / path;

            std::vector<uint8_t> data;
            if (!ReadFile(full.string(), data))
            {
                report.Fail("\"" + path + "\" is not there (looked in " + full.string() + ")");
                return;
            }

            int width = 0;
            int height = 0;
            int components = 0;

            if (!stbi_info_from_memory(data.data(), static_cast<int>(data.size()), &width, &height, &components))
            {
                report.Fail("\"" + path + "\" is not an image the client can decode: " + stbi_failure_reason());
                return;
            }

            if (width > kTextureMaxSize || height > kTextureMaxSize)
            {
                report.Fail(
                    "\"" + path + "\" is " + std::to_string(width) + "x" + std::to_string(height) +
                    ", the client refuses anything over " + std::to_string(kTextureMaxSize)
                );
                return;
            }

            std::printf("  - %s  %dx%d\n", path.c_str(), width, height);
        }
    }

    // What a material is called and what it dresses, from either kind of file, so the
    // checking below is written once.
    struct Subject
    {
        std::string title;
        const ncl_material::Material* material;
    };

    // A document beside a model keys its materials by texture name; a preset gives each one
    // a name of its own and says which texture it dresses. Both are objects with a
    // "materials" member, and which one this is shows in whether that member is an array.
    bool LooksLikeAPreset(std::string_view json)
    {
        try
        {
            tao::json::value root = tao::json::from_string(std::string(json));
            const tao::json::value* materials = root.is_object() ? root.find("materials") : nullptr;

            return materials != nullptr && materials->is_array();
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    int RunCheck(const std::string& material_path, const CheckOptions& options)
    {
        std::vector<uint8_t> data;
        if (!ReadFile(material_path, data))
        {
            std::fprintf(stderr, "matgen check: cannot read %s\n", material_path.c_str());
            return 1;
        }

        std::string_view json(reinterpret_cast<const char*>(data.data()), data.size());

        std::vector<Subject> subjects;
        ncl_material::MaterialDoc doc;
        ncl_material::MaterialPreset preset;
        bool is_preset = LooksLikeAPreset(json);

        if (is_preset)
        {
            ncl_utils::ResultT<ncl_material::MaterialPreset> parsed =
                ncl_material::ParseMaterialPreset(json, ncl_material::PresetBudget {});

            if (parsed.has_error())
            {
                std::fprintf(stderr, "matgen check: %s: %s\n", material_path.c_str(), parsed.error_str().c_str());
                return 1;
            }

            preset = *parsed;

            for (const ncl_material::PresetMaterial& material : preset.materials)
            {
                subjects.push_back(Subject {material.name + " -> " + material.texture_name, &material.material});
            }
        }
        else
        {
            ncl_utils::ResultT<ncl_material::MaterialDoc> parsed = ncl_material::ParseMaterialDoc(json);

            if (parsed.has_error())
            {
                std::fprintf(stderr, "matgen check: %s: %s\n", material_path.c_str(), parsed.error_str().c_str());
                return 1;
            }

            doc = *parsed;

            for (const ncl_material::MaterialEntry& entry : doc.entries)
            {
                subjects.push_back(Subject {entry.texture_name, &entry.material});
            }
        }

        std::string game_dir = options.game_dir.empty() ? std::filesystem::current_path().string() : options.game_dir;

        std::printf("%s: %s, %zu materials, paths resolved against %s\n",
            material_path.c_str(), is_preset ? "preset" : "model document",
            subjects.size(), game_dir.c_str());

        Report report;

        for (const Subject& subject : subjects)
        {
            const ncl_material::Material& material = *subject.material;
            std::printf("%s -> %s\n", subject.title.c_str(), material.program.c_str());

            // The legacy program is what a model with no material of its own resolves to,
            // and the client checks nothing about it for the same reason.
            if (material.program == ncl_material::kLegacyProgram)
            {
                continue;
            }

            std::string manifest_error;
            const ncl_material::ProgramManifest* manifest = FindManifest(material.program, manifest_error);

            if (manifest == nullptr)
            {
                report.Fail(
                    manifest_error.empty()
                        ? "no program \"" + material.program + "\""
                        : "manifest of \"" + material.program + "\": " + manifest_error
                );
                continue;
            }

            ncl_utils::Result<> valid = ncl_material::Validate(material, *manifest);
            if (valid.has_error())
            {
                report.Fail(valid.error_str());
                continue;
            }

            for (const ncl_material::Param& param : material.params)
            {
                if (param.value.type == ncl_material::ParamType::TextureRef)
                {
                    CheckTexture(game_dir, param.value.texture_ref, report);
                }
            }
        }

        // Counted the way the format counts it: distinct files, because a file that reuses
        // one image is not charged twice for it.
        std::vector<std::string> distinct;

        for (const Subject& subject : subjects)
        {
            for (const ncl_material::Param& param : subject.material->params)
            {
                if (param.value.type != ncl_material::ParamType::TextureRef)
                {
                    continue;
                }

                if (std::find(distinct.begin(), distinct.end(), param.value.texture_ref) == distinct.end())
                {
                    distinct.push_back(param.value.texture_ref);
                }
            }
        }

        size_t texture_budget = is_preset ? ncl_material::PresetBudget {}.max_textures : ncl_material::kMaxDocTextures;

        std::printf("%zu distinct images, the file may name %zu\n", distinct.size(), texture_budget);

        if (report.problems == 0)
        {
            std::printf("ok\n");
            return 0;
        }

        std::printf("%d problems\n", report.problems);

        return 1;
    }
}
