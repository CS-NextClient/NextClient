#pragma once

#include <cstdint>
#include "graphics/render/render_context.h"
#include "graphics/render/RenderDevice.h"

namespace tex
{
    struct TextureLoadGuard
    {
        void* native_context{};
        const uint64_t* preparation_epoch{};
        uint64_t expected_preparation_epoch{};
        const uint32_t* shader_generation{};
        uint32_t expected_shader_generation{};
        const uint32_t* texture_revision{};
        uint32_t expected_texture_revision{};
        render::RenderDevice* device{};
        uint32_t expected_device_generation{};

        bool is_epoch_current() const
        {
            return (device == nullptr || (device->is_context_alive() && device->get_generation() == expected_device_generation)) &&
                   (preparation_epoch == nullptr || *preparation_epoch == expected_preparation_epoch) &&
                   (shader_generation == nullptr || *shader_generation == expected_shader_generation) &&
                   (texture_revision == nullptr || *texture_revision == expected_texture_revision);
        }
        bool is_current() const
        {
            return is_epoch_current() && (native_context == nullptr || render::context::Current() == native_context);
        }
    };
} // namespace tex
