#include "graphics/render/model/StudioModelCache.h"

#include <algorithm>
#include <cstring>
#include <string_view>

#include <goldsrc/engine/model.h>

#include "graphics/render/RenderDevice.h"

namespace render
{
    namespace
    {
        constexpr int kMaxBodyValues = 64;

        size_t CachedSize(const goldsrc::studiohdr_t* header)
        {
            if (header == nullptr)
            {
                return 0;
            }

            int extent = header->textureindex != 0 ? header->texturedataindex : header->length;
            return extent >= static_cast<int>(sizeof(goldsrc::studiohdr_t)) ? static_cast<size_t>(extent) : 0;
        }

    } // namespace

    StudioModelCache::StudioModelCache(
        RenderDevice& device,
        MaterialDocumentStore& documents,
        MaterialManifestCatalog& manifests,
        const ShaderManager& shaders,
        const ServerMaterials& server_materials,
        performance::PerformanceMonitor& monitor
    ) :
        monitor_(monitor),
        device_(device),
        documents_(documents),
        manifests_(manifests),
        shaders_(shaders),
        server_materials_(server_materials)
    {
        device_.Register(this);
    }

    StudioModelCache::~StudioModelCache()
    {
        device_.Unregister(this);
    }

    void StudioModelCache::OnDeviceLost()
    {
        ++mutation_epoch_;
        last_entry_ = nullptr;
        for (std::unique_ptr<Entry>& entry : models_)
        {
            entry->geometry.OnDeviceLost();
        }
    }

    StudioModelCache::Entry* StudioModelCache::Observe(
        goldsrc::model_t* model,
        const goldsrc::studiohdr_t* header,
        StudioModelBarrier barrier
    )
    {
        std::string_view name(model->name, strnlen(model->name, sizeof(model->name)));
        std::string owned_name;

        if (last_entry_ != nullptr && last_entry_->session == session_ && last_entry_->header == header && last_entry_->name == name)
        {
            return last_entry_;
        }

        for (size_t i = models_.size(); i > 0; i--)
        {
            Entry& entry = *models_[i - 1];

            if (entry.name != name)
            {
                continue;
            }

            if (entry.session == session_ && entry.header == header)
            {
                last_entry_ = &entry;
                return &entry;
            }

            if (barrier.before_replacement != nullptr)
            {
                owned_name.assign(name);
                name = owned_name;
                const uint64_t epoch = mutation_epoch_;
                barrier.before_replacement(barrier.user);
                if (epoch != mutation_epoch_)
                {
                    return nullptr;
                }
            }

            // A reused address in another model session cannot establish mesh identity.
            last_entry_ = nullptr;
            sorted_names_.clear();
            ++mutation_epoch_;
            entry.geometry.Release();
            models_.erase(models_.begin() + (i - 1));
        }

        std::unique_ptr<Entry> entry = std::make_unique<Entry>(device_, documents_, manifests_, shaders_, server_materials_, monitor_);
        entry->name = name;
        entry->header = header;
        entry->session = session_;
        last_entry_ = entry.get();
        models_.push_back(std::move(entry));
        ++mutation_epoch_;
        return last_entry_;
    }

    StudioModelCache::Entry* StudioModelCache::FindMetadata(const char* name)
    {
        if (name == nullptr)
        {
            return nullptr;
        }

        for (size_t i = models_.size(); i > 0; i--)
        {
            Entry& entry = *models_[i - 1];

            if (entry.session == session_ && entry.name == name && entry.materials.get_metadata() != nullptr)
            {
                return &entry;
            }
        }

        return nullptr;
    }

    StudioModelCache::Entry* StudioModelCache::FindByName(const char* name)
    {
        Entry* entry = FindMetadata(name);
        return entry != nullptr && entry->materials.is_ready() ? entry : nullptr;
    }
    bool StudioModelCache::PrepareMetadata(
        goldsrc::model_t* model,
        const goldsrc::studiohdr_t* header,
        const goldsrc::studiohdr_t* texture_header,
        const goldsrc::model_t* texture_model,
        StudioModelBarrier barrier
    )
    {
        if (model == nullptr || header == nullptr || texture_header == nullptr || texture_model == nullptr)
        {
            return false;
        }

        Entry* entry = Observe(model, header, barrier);
        if (entry == nullptr)
        {
            return false;
        }
        std::string_view texture_owner(texture_model->name, strnlen(texture_model->name, sizeof(texture_model->name)));
        bool prepared = entry->materials.Prepare(header, CachedSize(header), texture_header, CachedSize(texture_header), texture_owner);

        if (!prepared)
        {
            sorted_names_.clear();
        }

        return prepared;
    }

    const StudioGpuModel* StudioModelCache::PrepareGeometry(
        goldsrc::model_t* model,
        const goldsrc::studiohdr_t* header,
        StudioModelBarrier barrier
    )
    {
        if (model == nullptr || header == nullptr)
        {
            return nullptr;
        }

        Entry* entry = Observe(model, header, barrier);
        return entry != nullptr ? entry->geometry.Prepare(entry->name, header, CachedSize(header)) : nullptr;
    }

    const ncl_studio::StudioMetadata* StudioModelCache::Metadata(const char* model)
    {
        const Entry* entry = FindMetadata(model);
        return entry != nullptr ? entry->materials.get_metadata() : nullptr;
    }

    const ncl_material::Material* StudioModelCache::ResolveMaterial(
        goldsrc::model_t* model,
        const goldsrc::studiohdr_t* header,
        const goldsrc::studiohdr_t* texture_header,
        int texture_index,
        const goldsrc::model_t* texture_model,
        StudioModelBarrier barrier,
        ncl_material::MaterialOrigin* origin
    )
    {
        if (!PrepareMetadata(model, header, texture_header, texture_model, barrier))
        {
            return nullptr;
        }

        Entry* entry = Observe(model, header, barrier);
        return entry != nullptr ? entry->materials.Resolve(entry->name, texture_index, origin) : nullptr;
    }

    const ncl_material::Material* StudioModelCache::ResolveMaterialByName(
        const char* model,
        const char* texture_name,
        ncl_material::MaterialOrigin* origin
    )
    {
        Entry* entry = FindByName(model);

        if (entry == nullptr || texture_name == nullptr)
        {
            return nullptr;
        }

        const std::vector<ncl_studio::TextureInfo>& textures = entry->materials.get_textures()->textures;

        for (size_t index = 0; index < textures.size(); index++)
        {
            if (ncl_material::EqualsIgnoreCaseAscii(textures[index].name, texture_name))
            {
                return entry->materials.Resolve(entry->name, static_cast<int>(index), origin);
            }
        }

        return nullptr;
    }

    int StudioModelCache::ModelCount()
    {
        sorted_names_.clear();

        for (const std::unique_ptr<Entry>& entry : models_)
        {
            if (entry->session == session_ && entry->materials.is_ready())
            {
                sorted_names_.push_back(entry->name);
            }
        }

        std::sort(sorted_names_.begin(), sorted_names_.end());
        return static_cast<int>(sorted_names_.size());
    }

    const char* StudioModelCache::ModelName(int index)
    {
        return index >= 0 && index < static_cast<int>(sorted_names_.size()) ? sorted_names_[index].c_str() : nullptr;
    }

    int StudioModelCache::TextureCount(const char* model)
    {
        const Entry* entry = FindByName(model);
        return entry != nullptr ? static_cast<int>(entry->materials.get_textures()->textures.size()) : 0;
    }

    const char* StudioModelCache::TextureName(const char* model, int index)
    {
        const Entry* entry = FindByName(model);

        if (entry == nullptr || index < 0 || index >= static_cast<int>(entry->materials.get_textures()->textures.size()))
        {
            return nullptr;
        }

        return entry->materials.get_textures()->textures[index].name.c_str();
    }

    int StudioModelCache::SkinFamilyCount(const char* model)
    {
        const Entry* entry = FindByName(model);
        return entry != nullptr ? std::max(entry->materials.get_textures()->skins.families, 1) : 0;
    }

    int StudioModelCache::BodyCount(const char* model)
    {
        const Entry* entry = FindByName(model);
        const ncl_studio::StudioMetadata* metadata = entry != nullptr ? entry->materials.get_metadata() : nullptr;

        if (metadata == nullptr || metadata->bodyparts.empty())
        {
            return 0;
        }

        int count = 1;

        for (const ncl_studio::StudioBodyPart& part : metadata->bodyparts)
        {
            int models = std::max(static_cast<int>(part.submodels.size()), 1);

            if (count > kMaxBodyValues / models)
            {
                return kMaxBodyValues;
            }

            count *= models;
        }

        return count;
    }

    bool StudioModelCache::BodyName(const char* model, int body, char* out, int out_len)
    {
        const Entry* entry = FindByName(model);
        const ncl_studio::StudioMetadata* metadata = entry != nullptr ? entry->materials.get_metadata() : nullptr;

        if (metadata == nullptr || metadata->bodyparts.empty() || out == nullptr || out_len <= 0 || body < 0 || body >= BodyCount(model))
        {
            return false;
        }

        std::string joined;

        for (const ncl_studio::StudioBodyPart& part : metadata->bodyparts)
        {
            if (part.submodels.empty())
            {
                return false;
            }

            int base = std::max(part.base, 1);
            size_t chosen = (body / base) % part.submodels.size();

            if (!joined.empty())
            {
                joined += " / ";
            }

            joined += part.submodels[chosen];
        }

        size_t length = std::min(joined.size(), static_cast<size_t>(out_len - 1));
        std::memcpy(out, joined.data(), length);
        out[length] = '\0';
        return true;
    }

    void StudioModelCache::ReloadDocuments()
    {
        ++mutation_epoch_;
        for (std::unique_ptr<Entry>& entry : models_)
        {
            entry->materials.ReloadDocument();
        }
    }

    void StudioModelCache::ForgetSessionMetadata()
    {
        ++mutation_epoch_;
        session_++;
        last_entry_ = nullptr;
        sorted_names_.clear();

        for (std::unique_ptr<Entry>& entry : models_)
        {
            entry->header = nullptr;
            entry->materials.Forget();
        }
    }

    void StudioModelCache::ForgetModel(const char* model)
    {
        ++mutation_epoch_;
        for (std::unique_ptr<Entry>& entry : models_)
        {
            if (entry->name == model)
            {
                last_entry_ = nullptr;
                sorted_names_.clear();
                entry->session = 0;
                entry->header = nullptr;
                entry->materials.Forget();
            }
            else if (entry->materials.ForgetTextureOwner(model))
            {
                last_entry_ = nullptr;
                sorted_names_.clear();
            }
        }
    }

    void StudioModelCache::ReleaseGeometry()
    {
        ++mutation_epoch_;
        last_entry_ = nullptr;

        for (std::unique_ptr<Entry>& entry : models_)
        {
            entry->geometry.Release();
        }
    }

    void StudioModelCache::Clear()
    {
        ++mutation_epoch_;
        last_entry_ = nullptr;
        sorted_names_.clear();
        for (std::unique_ptr<Entry>& entry : models_)
        {
            entry->geometry.Release();
        }
        models_.clear();
    }
} // namespace render
