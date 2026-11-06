#include "graphics/preview/PreviewModelRepository.h"

namespace render::preview
{
    bool CanAdmitModel(int loaded_entries, int new_slots, int available_slots)
    {
        return new_slots <= available_slots && new_slots <= kMaxPreviewModelEntries - loaded_entries;
    }

    PreviewModelRepository::PreviewModelRepository(EnginePreviewModelBackend& backend) :
        backend_(backend)
    {}

    PreviewModelStatus PreviewModelRepository::PrepareModel(
        const char* path,
        PreviewModelInfo* out,
        PreparedPreviewModel* prepared,
        StudioModelBarrier barrier
    )
    {
        *out = PreviewModelInfo{};
        if (prepared != nullptr)
        {
            *prepared = {};
        }

        if (!backend_.is_renderer_available())
        {
            backend_.RecordLoadError(path, "studio interface is unavailable");
            return PreviewModelStatus::RendererUnavailable;
        }

        int new_slots = 0;
        PreviewModelStatus status = backend_.InspectModel(path, &new_slots);

        if (status != PreviewModelStatus::Ready)
        {
            return status;
        }

        if (new_slots != 0 && !CanAdmitModel(loaded_entries_, new_slots, backend_.get_available_slots()))
        {
            backend_.RecordLoadError(path, "preview model capacity exhausted");
            return PreviewModelStatus::CapacityExceeded;
        }

        // Loading may retain engine table entries even when metadata preparation fails.
        loaded_entries_ += new_slots;
        PreviewModelInfo prepared_info{};
        status = backend_.LoadModel(path, &prepared_info, prepared, barrier);

        if (status == PreviewModelStatus::Ready)
        {
            *out = prepared_info;
        }

        return status;
    }

bool PreviewModelRepository::GetSequenceInfo(const char* path, int index, PreviewSequenceInfo* out)
{
    *out = PreviewSequenceInfo{};
    return backend_.is_renderer_available() && backend_.GetSequenceInfo(path, index, out);
}

int PreviewModelRepository::get_loaded_entries() const
{
    return loaded_entries_;
}

void PreviewModelRepository::Reset()
{
    loaded_entries_ = 0;
}

const char* PreviewModelRepository::get_last_load_error() const
{
    return backend_.get_last_load_error();
}
} // namespace render::preview
