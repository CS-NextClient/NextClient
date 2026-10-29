#pragma once

#include <next_engine_mini/ScenePreviewInterface.h>

#include "graphics/preview/EnginePreviewModelBackend.h"

namespace render::preview
{
    inline constexpr int kMaxPreviewModelEntries = 128;

    bool CanAdmitModel(int loaded_entries, int new_slots, int available_slots);

    class PreviewModelRepository
    {
    private:
        EnginePreviewModelBackend& backend_;
        int loaded_entries_{};

    public:
        explicit PreviewModelRepository(EnginePreviewModelBackend& backend);
        PreviewModelStatus PrepareModel(
            const char* path,
            PreviewModelInfo* out,
            PreparedPreviewModel* prepared = nullptr,
            StudioModelBarrier barrier = {}
        );
        bool GetSequenceInfo(const char* path, int index, PreviewSequenceInfo* out);
        int get_loaded_entries() const;
        const char* get_last_load_error() const;
        void Reset();
    };
} // namespace render::preview
