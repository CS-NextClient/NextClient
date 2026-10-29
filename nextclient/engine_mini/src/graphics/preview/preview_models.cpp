#include "graphics/preview/preview_model.h"

#include "graphics/render/RendererState.h"

PreviewModelStatus PreviewModel_Prepare(const char* path, PreviewModelInfo* out)
{
    return render::GetRendererState().preview_models.PrepareModel(path, out);
}

bool PreviewModel_GetSequenceInfo(const char* path, int index, PreviewSequenceInfo* out)
{
    return render::GetRendererState().preview_models.GetSequenceInfo(path, index, out);
}

int PreviewModel_LoadedEntries()
{
    return render::GetRendererState().preview_models.get_loaded_entries();
}

void PreviewModel_ResetModels()
{
    render::GetRendererState().preview_models.Reset();
}

const char* PreviewModel_LastLoadError()
{
    return render::GetRendererState().preview_models.get_last_load_error();
}
