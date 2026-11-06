#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include <goldsrc/cvar.h>

namespace render
{
    class RenderCaptureBackend
    {
    public:
        virtual ~RenderCaptureBackend() = default;
        virtual void SetCvar(goldsrc::cvar_t* cvar, const char* value) = 0;
        virtual bool Capture(const char* name, const char* suffix, std::filesystem::path& path) = 0;
        virtual void ReportCompleted(const char* name, const std::filesystem::path& path) = 0;
    };

    class RenderCaptureSession
    {
    private:
        enum class State
        {
            Idle,
            PendingOff,
            PendingOn
        };
        RenderCaptureBackend& backend_;
        State state_ = State::Idle;
        bool restoring_{};
        uint64_t revision_{};
        char name_[64]{};
        goldsrc::cvar_t* studio_shader_{};
        goldsrc::cvar_t* studio_materials_{};
        std::string saved_studio_shader_;
        std::string saved_studio_materials_;

    public:
        explicit RenderCaptureSession(RenderCaptureBackend& backend) :
            backend_(backend)
        {}
        RenderCaptureSession(const RenderCaptureSession&) = delete;
        RenderCaptureSession& operator=(const RenderCaptureSession&) = delete;
        bool is_pending() const
        {
            return state_ != State::Idle || restoring_;
        }
        void Begin(const char* name, goldsrc::cvar_t& shader, goldsrc::cvar_t* materials);
        void Cancel();
        void OnSceneEnd();
    };

    RenderCaptureBackend& GetRenderCaptureBackend();
} // namespace render
