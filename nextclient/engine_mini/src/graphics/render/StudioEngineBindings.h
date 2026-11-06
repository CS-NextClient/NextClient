#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <goldsrc/client_api.h>
#include <goldsrc/studio_api.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/entity.h>
#include <goldsrc/view.h>
#include <goldsrc/engine/client_state.h>

#include "graphics/render/RenderConfig.h"
#include "graphics/render/StudioShaderInterface.h"

namespace render
{
    struct StudioViewInputs
    {
        bool network_session{};
        bool shader_enabled{};
        bool materials_enabled = true;
        bool queue_enabled{};
        float blend = 1.0f;
        float gamma = 2.5f;
        float right_hand{};
        float shell_frequency{};
        double time{};
        int view_entity{};
        float origin[3]{};
        float angles[3]{};
    };

    struct StudioEngineBindings
    {
        goldsrc::client_static_t* client{};
        goldsrc::client_state_t* state{};
        goldsrc::refdef_t* view{};
        goldsrc::dlight_t* lights{};
        size_t light_count{};
        float* blend{};
        goldsrc::cvar_t** gamma{};
        RenderConfig* config{};
        int* current_texture{};
        const int32_t* linear_gamma{};
        const int32_t* light_gamma{};
        goldsrc::cvar_t* right_hand{};
        goldsrc::cvar_t* shell_frequency{};
        bool controls_resolved{};

        bool is_network_session() const;
        StudioViewInputs CaptureView();
        std::array<float, kStudioLightGammaEntries> CaptureLightGamma() const;
        void BindCapturedTexture(unsigned int texture);
        unsigned int CaptureSkin(const goldsrc::engine_studio_api_t& api, const goldsrc::studiohdr_t* header, int skin);
    };
} // namespace render
