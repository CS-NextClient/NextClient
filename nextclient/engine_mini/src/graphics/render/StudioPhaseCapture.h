#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <limits>
#include <memory>

namespace render
{
    struct StudioPhaseLimits
    {
        size_t views = 16384;
    };

    struct StudioPhaseFrame
    {
        int host_frame{};
        int render_frame{};
        uint64_t output_id{};
        int demo_frame{};
        double client_time{};
        double old_client_time{};
        const char* demo{};
    };

    class StudioPhaseCapture
    {
    private:
        struct State;

        std::unique_ptr<State> state_;
        bool active_{};

    public:
        StudioPhaseCapture();
        ~StudioPhaseCapture();

        bool is_active() const
        {
            return active_;
        }

        bool has_recording() const
        {
            return state_ != nullptr;
        }

        bool Start(
            StudioPhaseLimits limits = {},
            int first_demo_frame = 0,
            int last_demo_frame = std::numeric_limits<int>::max()
        );
        void Reset();
        void Stop(const char* reason = "stopped");
        void SessionBoundary(const char* reason = "session_boundary");
        bool BeginView(const StudioPhaseFrame& frame, uint64_t now);
        void EndView(uint64_t now);
        void BeginPass(int pass, uint64_t now);
        void EndPass(uint64_t now);
        void AddWorldTime(uint64_t elapsed);
        void NotePacket();
        void NoteGroup(uint64_t groups, uint64_t meshes, uint64_t triangles);
        void NotePhysicalDraw(uint64_t submitted_triangles);
        bool Export(std::ostream& phases, std::ostream& metadata) const;
        size_t get_view_count() const;
        bool has_overflow() const;
    };

    void StudioPhase_BeginView();
    void StudioPhase_EndView();
    void StudioPhase_BeginPass(int pass);
    void StudioPhase_EndPass();
    uint64_t StudioPhase_BeginWorld();
    void StudioPhase_EndWorld(uint64_t started);
    void StudioPhase_RegisterCommands();
} // namespace render
