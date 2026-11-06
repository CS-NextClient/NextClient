#pragma once

#include <goldsrc/client_api.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/studio_api.h>

namespace render::preview
{
    struct PreviewEngineBindings
    {
        goldsrc::r_studio_interface_t* studio{};
        goldsrc::cl_entity_t** current_entity{};
        float* blend{};
        goldsrc::model_t* known_models{};
        int* known_count{};
        goldsrc::model_t* (*load_model)(const char*, goldsrc::qboolean, goldsrc::qboolean){};
        void* (*model_data)(goldsrc::model_t*){};
        goldsrc::client_static_t* client{};
        int (*screen_info)(goldsrc::SCREENINFO*){};
        void (*print)(const char*, ...){};
    };

    struct PreparedPreviewModel
    {
        goldsrc::model_t* model{};
        goldsrc::studiohdr_t* header{};
    };
} // namespace render::preview
