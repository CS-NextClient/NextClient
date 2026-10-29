#include "graphics/render/RenderCaptureSession.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace render
{
    void RenderCaptureSession::Begin(const char* name, goldsrc::cvar_t& shader, goldsrc::cvar_t* materials)
    {
        if (is_pending())
        {
            return;
        }

        const size_t length = std::min(std::strlen(name), sizeof(name_) - 1);
        std::memcpy(name_, name, length);
        name_[length] = '\0';
        studio_shader_ = &shader;
        studio_materials_ = materials;
        saved_studio_shader_ = shader.string;
        saved_studio_materials_ = materials != nullptr ? materials->string : "";
        state_ = State::PendingOff;
        const uint64_t revision = ++revision_;

        if (materials != nullptr)
        {
            backend_.SetCvar(materials, "0");
        }

        if (revision == revision_)
        {
            backend_.SetCvar(&shader, "0");
        }
    }

    void RenderCaptureSession::Cancel()
    {
        if (state_ == State::Idle)
        {
            return;
        }

        state_ = State::Idle;
        ++revision_;
        restoring_ = true;
        goldsrc::cvar_t* shader = std::exchange(studio_shader_, nullptr);
        goldsrc::cvar_t* materials = std::exchange(studio_materials_, nullptr);
        const std::string saved_shader = std::move(saved_studio_shader_);
        const std::string saved_materials = std::move(saved_studio_materials_);
        backend_.SetCvar(shader, saved_shader.c_str());
        if (materials != nullptr)
        {
            backend_.SetCvar(materials, saved_materials.c_str());
        }
        restoring_ = false;
    }

    void RenderCaptureSession::OnSceneEnd()
    {
        if (state_ == State::Idle)
        {
            return;
        }

        const uint64_t revision = revision_;
        const bool pending_off = state_ == State::PendingOff;
        std::filesystem::path path;
        const bool captured = backend_.Capture(name_, pending_off ? "_off" : "_on", path);
        if (revision != revision_)
        {
            return;
        }

        if (pending_off && captured)
        {
            state_ = State::PendingOn;
            backend_.SetCvar(studio_shader_, "1");
            return;
        }

        Cancel();
        if (captured)
        {
            backend_.ReportCompleted(name_, path);
        }
    }
} // namespace render
