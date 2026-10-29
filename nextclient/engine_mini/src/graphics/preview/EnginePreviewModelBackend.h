#pragma once

#include <string>

#include <next_engine_mini/ScenePreviewInterface.h>

#include "graphics/preview/PreviewEngineBindings.h"
#include "graphics/render/model/StudioModelCache.h"

namespace render
{
    class StudioModelCache;
    class MaterialFileBackend;
} // namespace render

namespace render::preview
{
    class EnginePreviewModelBackend
    {
    private:
        StudioModelCache& models_;
        PreviewEngineBindings& engine_;
        MaterialFileBackend& files_;
        std::string model_path_;
        std::string texture_path_;
        std::string last_load_error_;

    public:
        EnginePreviewModelBackend(StudioModelCache& models, PreviewEngineBindings& engine, MaterialFileBackend& files);
        bool is_renderer_available() const;
        int get_available_slots() const;
        PreviewModelStatus InspectModel(const char* path, int* new_slots);
        PreviewModelStatus LoadModel(
            const char* path,
            PreviewModelInfo* out,
            PreparedPreviewModel* prepared,
            StudioModelBarrier barrier = {}
        );
        bool GetSequenceInfo(const char* path, int index, PreviewSequenceInfo* out);
        void RecordLoadError(const char* path, const std::string& reason);
        const char* get_last_load_error() const;

    private:
        goldsrc::model_t* FindKnown(const char* path) const;
        PreviewModelStatus ReadHeader(const char* path, goldsrc::studiohdr_t* out);
    };
} // namespace render::preview
