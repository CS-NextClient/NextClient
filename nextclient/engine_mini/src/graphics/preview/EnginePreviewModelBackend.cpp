#include "graphics/preview/EnginePreviewModelBackend.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <limits>

#include <ncl_studio/studio_validate.h>
#include <tier1/strtools.h>

#include "common/model.h"
#include "graphics/render/material/MaterialDocumentStore.h"
#include "graphics/render/StudioDraw.h"
#include "graphics/render/model/StudioModelCache.h"

namespace render::preview
{
    namespace
    {
        bool IsModelPath(const char* path)
        {
            if (path == nullptr)
            {
                return false;
            }

            size_t length = strnlen(path, goldsrc::MAX_MODEL_NAME);
            return length > 11 && length < goldsrc::MAX_MODEL_NAME && V_strnicmp(path, "models/", 7) == 0 &&
                   V_stricmp(path + length - 4, ".mdl") == 0 && std::strstr(path, "..") == nullptr && std::strchr(path, '\\') == nullptr &&
                   std::strchr(path, ':') == nullptr;
        }

    } // namespace

    EnginePreviewModelBackend::EnginePreviewModelBackend(
        StudioModelCache& models,
        PreviewEngineBindings& engine,
        MaterialFileBackend& files
    ) :
        models_(models),
        engine_(engine),
        files_(files)
    {}

    void EnginePreviewModelBackend::RecordLoadError(const char* path, const std::string& reason)
    {
        std::string message = std::string(path != nullptr ? path : "<null>") + ": " + reason;
        if (last_load_error_ != message)
        {
            last_load_error_ = message;
            if (engine_.print != nullptr)
            {
                engine_.print("material preview: %s\n", message.c_str());
            }
        }
    }

    const char* EnginePreviewModelBackend::get_last_load_error() const
    {
        return last_load_error_.c_str();
    }

    goldsrc::model_t* EnginePreviewModelBackend::FindKnown(const char* path) const
    {
        for (int index = 0; index < *engine_.known_count; index++)
        {
            if (V_stricmp(engine_.known_models[index].name, path) == 0)
            {
                return &engine_.known_models[index];
            }
        }

        return nullptr;
    }

    PreviewModelStatus EnginePreviewModelBackend::ReadHeader(const char* path, goldsrc::studiohdr_t* out)
    {
        std::string bytes;
        MaterialReadStatus read = files_.ReadFile(path, bytes, static_cast<size_t>(std::numeric_limits<int>::max()));
        if (read != MaterialReadStatus::Ready)
        {
            RecordLoadError(path, read == MaterialReadStatus::NotFound ? "model file not found" : "model file read failed");
            return read == MaterialReadStatus::NotFound ? PreviewModelStatus::NotFound : PreviewModelStatus::InvalidModel;
        }

        ncl_utils::Result<> valid = ncl_studio::ValidateStudioFile(bytes.data(), bytes.size());
        if (valid.has_error())
        {
            RecordLoadError(path, valid.error_str());
            return PreviewModelStatus::InvalidModel;
        }

        std::memcpy(out, bytes.data(), sizeof(*out));
        return PreviewModelStatus::Ready;
    }

    bool EnginePreviewModelBackend::is_renderer_available() const
    {
        return engine_.studio != nullptr && engine_.studio->StudioDrawModel != nullptr && engine_.current_entity != nullptr &&
               engine_.blend != nullptr && engine_.known_models != nullptr && engine_.known_count != nullptr &&
               engine_.load_model != nullptr && engine_.model_data != nullptr;
    }

    int EnginePreviewModelBackend::get_available_slots() const
    {
        int available = goldsrc::MAX_KNOWN_MODELS - *engine_.known_count;

        for (int index = 0; index < *engine_.known_count; index++)
        {
            if (engine_.known_models[index].needload == NL_UNREFERENCED &&
                V_stricmp(engine_.known_models[index].name, model_path_.c_str()) != 0 &&
                V_stricmp(engine_.known_models[index].name, texture_path_.c_str()) != 0)
            {
                available++;
            }
        }

        return available;
    }

    PreviewModelStatus EnginePreviewModelBackend::InspectModel(const char* path, int* new_slots)
    {
        *new_slots = 0;

        if (!IsModelPath(path))
        {
            RecordLoadError(path, "invalid model path");
            return PreviewModelStatus::InvalidModel;
        }

        model_path_ = path;
        texture_path_.clear();

        goldsrc::studiohdr_t header{};
        goldsrc::model_t* known = FindKnown(path);

        if (known != nullptr && known->type == goldsrc::mod_studio && known->cache.data != nullptr)
        {
            std::memcpy(&header, known->cache.data, sizeof(header));
        }
        else
        {
            PreviewModelStatus status = ReadHeader(path, &header);

            if (status != PreviewModelStatus::Ready)
            {
                return status;
            }
        }

        *new_slots = known == nullptr ? 1 : 0;

        if (header.numtextures == 0)
        {
            texture_path_ = path;
            texture_path_.replace(texture_path_.size() - 4, 4, "T.mdl");

            if (texture_path_.size() >= goldsrc::MAX_MODEL_NAME)
            {
                RecordLoadError(path, "texture companion path exceeds the engine model name limit");
                return PreviewModelStatus::InvalidModel;
            }

            goldsrc::model_t* textures = FindKnown(texture_path_.c_str());

            if (textures == nullptr || textures->type != goldsrc::mod_studio || textures->cache.data == nullptr)
            {
                PreviewModelStatus status = ReadHeader(texture_path_.c_str(), &header);

                if (status != PreviewModelStatus::Ready || header.numtextures <= 0)
                {
                    return status == PreviewModelStatus::Ready ? PreviewModelStatus::InvalidModel : status;
                }
            }

            *new_slots += textures == nullptr ? 1 : 0;
        }

        return PreviewModelStatus::Ready;
    }

    PreviewModelStatus EnginePreviewModelBackend::LoadModel(
        const char* path,
        PreviewModelInfo* out,
        PreparedPreviewModel* prepared,
        StudioModelBarrier barrier
    )
    {
        goldsrc::model_t* model = engine_.load_model(path, false, true);

        if (model == nullptr || model->type != goldsrc::mod_studio || model->cache.data == nullptr)
        {
            return PreviewModelStatus::InvalidModel;
        }

        const goldsrc::studiohdr_t* header = static_cast<const goldsrc::studiohdr_t*>(engine_.model_data(model));
        const goldsrc::studiohdr_t* textures = header;
        goldsrc::model_t* texture_model = model;

        if (header->numtextures == 0)
        {
            char companion[goldsrc::MAX_MODEL_NAME];
            size_t length = strnlen(path, sizeof(companion));
            if (length >= sizeof(companion) - 1)
            {
                RecordLoadError(path, "texture companion path exceeds the engine model name limit");
                return PreviewModelStatus::InvalidModel;
            }

            std::memcpy(companion, path, length - 4);
            std::memcpy(companion + length - 4, "T.mdl", 6);
            texture_model = engine_.load_model(companion, false, true);

            if (texture_model == nullptr || texture_model->type != goldsrc::mod_studio || texture_model->cache.data == nullptr)
            {
                return PreviewModelStatus::InvalidModel;
            }

            textures = static_cast<const goldsrc::studiohdr_t*>(engine_.model_data(texture_model));
        }

        if (model->type != goldsrc::mod_studio || model->cache.data != header || V_stricmp(model->name, path) != 0 ||
            texture_model->type != goldsrc::mod_studio || texture_model->cache.data != textures)
        {
            return PreviewModelStatus::InvalidModel;
        }

        if (textures->numtextures <= 0 || !models_.PrepareMetadata(model, header, textures, texture_model, barrier))
        {
            RecordLoadError(path, "studio metadata preparation failed");
            return PreviewModelStatus::InvalidModel;
        }

        if (model->type != goldsrc::mod_studio || model->cache.data != header || V_stricmp(model->name, path) != 0 ||
            texture_model->type != goldsrc::mod_studio || texture_model->cache.data != textures)
        {
            return PreviewModelStatus::InvalidModel;
        }

        const ncl_studio::StudioMetadata* metadata = models_.Metadata(path);

        if (metadata == nullptr)
        {
            return PreviewModelStatus::InvalidModel;
        }

        for (int axis = 0; axis < 3; axis++)
        {
            out->bounds_min[axis] = metadata->minimum[axis];
            out->bounds_max[axis] = metadata->maximum[axis];
        }

        if (prepared != nullptr)
        {
            *prepared = {model, const_cast<goldsrc::studiohdr_t*>(header)};
        }

        out->sequence_count = static_cast<int>(metadata->sequences.size());
        out->skin_family_count = models_.SkinFamilyCount(path);
        out->body_count = models_.BodyCount(path);
        return PreviewModelStatus::Ready;
    }

    bool EnginePreviewModelBackend::GetSequenceInfo(const char* path, int index, PreviewSequenceInfo* out)
    {
        if (!IsModelPath(path))
        {
            return false;
        }

        const ncl_studio::StudioMetadata* metadata = models_.Metadata(path);

        if (metadata == nullptr || index < 0 || static_cast<size_t>(index) >= metadata->sequences.size())
        {
            return false;
        }

        const ncl_studio::StudioSequence& sequence = metadata->sequences[index];
        size_t length = std::min(sequence.name.size(), sizeof(out->name) - 1);
        std::memcpy(out->name, sequence.name.data(), length);
        out->name[length] = '\0';
        out->duration =
            sequence.frame_count > 1 && sequence.fps > 0.0f ? static_cast<float>(sequence.frame_count - 1) / sequence.fps : 1.0f;
        return true;
    }
} // namespace render::preview
