#include "graphics/render/StudioPhaseCapture.h"

#include <array>
#include <cstring>
#include <iomanip>
#include <new>
#include <ostream>

namespace render
{
    namespace
    {
        constexpr size_t kMaxBytes = 128 * 1024 * 1024;
        constexpr size_t kViewDepth = 8;

        void JsonString(std::ostream& out, const char* value)
        {
            out << '"';

            for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p)
            {
                if (*p == '"' || *p == '\\')
                {
                    out << '\\' << static_cast<char>(*p);
                }
                else if (*p >= 32)
                {
                    out << static_cast<char>(*p);
                }
            }

            out << '"';
        }
    } // namespace

    struct StudioPhaseCapture::State
    {
        struct View
        {
            StudioPhaseFrame frame;
            uint64_t started{};
            uint64_t interval{};
            uint64_t duration{};
            uint64_t opaque{};
            uint64_t transparent{};
            uint64_t world{};
            uint64_t packets{};
            uint64_t draws{};
            uint64_t meshes{};
            uint64_t triangles{};
            uint64_t physical_draws{};
            uint64_t submitted_triangles{};
            bool complete{};
        };

        struct ViewScope
        {
            size_t index{};
            int pass{};
            uint64_t pass_started{};
        };

        StudioPhaseLimits limits;
        std::unique_ptr<View[]> views;
        std::array<ViewScope, kViewDepth> stack{};
        size_t depth{};
        size_t view_count{};
        size_t completed{};
        size_t allocated{};
        uint64_t previous_start{};
        uint64_t dropped{};
        int first_demo_frame{};
        int last_demo_frame{};
        bool overflow{};
        char stop_reason[64]{};
        char demo[260]{};

        View* get_current_view()
        {
            return depth ? &views[stack[depth - 1].index] : nullptr;
        }
    };

    StudioPhaseCapture::StudioPhaseCapture() = default;
    StudioPhaseCapture::~StudioPhaseCapture() = default;

    bool StudioPhaseCapture::Start(StudioPhaseLimits limits, int first_demo_frame, int last_demo_frame)
    {
        if (active_ || (state_ && state_->view_count))
        {
            return false;
        }

        if (first_demo_frame < 0 || last_demo_frame < first_demo_frame || !limits.views ||
            limits.views > (kMaxBytes - sizeof(State)) / sizeof(State::View))
        {
            return false;
        }

        const size_t bytes = sizeof(State) + limits.views * sizeof(State::View);

        try
        {
            state_ = std::make_unique<State>();
            state_->views = std::make_unique<State::View[]>(limits.views);
        }
        catch (const std::bad_alloc&)
        {
            state_.reset();

            return false;
        }

        state_->limits = limits;
        state_->first_demo_frame = first_demo_frame;
        state_->last_demo_frame = last_demo_frame;
        state_->allocated = bytes;
        active_ = true;

        return true;
    }

    void StudioPhaseCapture::Stop(const char* reason)
    {
        active_ = false;

        if (state_)
        {
            if (!state_->stop_reason[0])
            {
                std::strncpy(state_->stop_reason, reason, sizeof(state_->stop_reason) - 1);
            }

            state_->depth = 0;
        }
    }

    void StudioPhaseCapture::Reset()
    {
        active_ = false;
        state_.reset();
    }

    void StudioPhaseCapture::SessionBoundary(const char* reason)
    {
        if (!state_)
        {
            return;
        }

        state_->depth = 0;

        if (state_->view_count)
        {
            Stop(reason);
        }
    }

    bool StudioPhaseCapture::BeginView(const StudioPhaseFrame& frame, uint64_t now)
    {
        if (!active_)
        {
            return false;
        }

        State& state = *state_;

        if (frame.demo_frame < state.first_demo_frame)
        {
            return false;
        }

        if (frame.demo_frame > state.last_demo_frame)
        {
            Stop("range_complete");

            return false;
        }

        if (state.depth && state.views[state.stack[state.depth - 1].index].frame.host_frame != frame.host_frame)
        {
            state.depth = 0;
        }

        if (state.view_count == state.limits.views || state.depth == state.stack.size())
        {
            state.overflow = true;
            ++state.dropped;
            Stop();

            return false;
        }

        State::ViewScope& scope = state.stack[state.depth++];
        scope = {};
        scope.index = state.view_count++;

        State::View& view = state.views[scope.index];

        view.frame = frame;
        view.frame.demo = nullptr;
        view.started = now;
        view.interval = state.previous_start ? now - state.previous_start : 0;
        state.previous_start = now;

        if (frame.demo && !state.demo[0])
        {
            std::strncpy(state.demo, frame.demo, sizeof(state.demo) - 1);
        }

        return true;
    }

    void StudioPhaseCapture::EndView(uint64_t now)
    {
        if (!active_ || !state_->depth)
        {
            return;
        }

        State& state = *state_;
        State::View& view = *state.get_current_view();

        view.duration = now - view.started;
        view.complete = true;
        ++state.completed;
        --state.depth;
    }

    void StudioPhaseCapture::BeginPass(int pass, uint64_t now)
    {
        if (active_ && state_->depth)
        {
            State::ViewScope& scope = state_->stack[state_->depth - 1];
            scope.pass = pass;
            scope.pass_started = now;
        }
    }

    void StudioPhaseCapture::EndPass(uint64_t now)
    {
        if (active_ && state_->depth)
        {
            State::ViewScope& scope = state_->stack[state_->depth - 1];
            State::View& view = *state_->get_current_view();

            if (scope.pass == 1)
            {
                view.opaque += now - scope.pass_started;
            }
            else if (scope.pass == 2)
            {
                view.transparent += now - scope.pass_started;
            }

            scope.pass = 0;
        }
    }

    void StudioPhaseCapture::AddWorldTime(uint64_t elapsed)
    {
        if (active_ && state_->depth)
        {
            state_->get_current_view()->world += elapsed;
        }
    }

    void StudioPhaseCapture::NotePacket()
    {
        if (active_ && state_->depth)
        {
            ++state_->get_current_view()->packets;
        }
    }

    void StudioPhaseCapture::NoteGroup(uint64_t groups, uint64_t meshes, uint64_t triangles)
    {
        if (active_ && state_->depth)
        {
            State::View& view = *state_->get_current_view();

            view.draws += groups;
            view.meshes += meshes;
            view.triangles += triangles;
        }
    }

    void StudioPhaseCapture::NotePhysicalDraw(uint64_t submitted_triangles)
    {
        if (active_ && state_->depth)
        {
            State::View& view = *state_->get_current_view();

            ++view.physical_draws;
            view.submitted_triangles += submitted_triangles;
        }
    }

    size_t StudioPhaseCapture::get_view_count() const
    {
        return state_ ? state_->view_count : 0;
    }

    bool StudioPhaseCapture::has_overflow() const
    {
        return state_ && state_->overflow;
    }

    bool StudioPhaseCapture::Export(std::ostream& phases, std::ostream& metadata) const
    {
        if (!state_ || active_)
        {
            return false;
        }

        const State& state = *state_;

        phases << std::setprecision(17);
        metadata << std::setprecision(17);
        phases
            << "view_id,host_frame,render_frame,output_id,demo_frame,client_time,old_client_time,view_cpu_us,frame_interval_us,"
               "opaque_cpu_us,transparent_cpu_us,world_cpu_us,packets,draws,meshes,triangles,complete,physical_draws,submitted_triangles\n";
        for (size_t i = 0; i < state.view_count; ++i)
        {
            const State::View& view = state.views[i];
            phases << i + 1 << ',' << view.frame.host_frame << ',' << view.frame.render_frame << ',' << view.frame.output_id << ','
                   << view.frame.demo_frame << ',' << view.frame.client_time << ',' << view.frame.old_client_time << ','
                   << static_cast<double>(view.duration) / 1000.0 << ',' << static_cast<double>(view.interval) / 1000.0 << ','
                   << static_cast<double>(view.opaque) / 1000.0 << ',' << static_cast<double>(view.transparent) / 1000.0 << ','
                   << static_cast<double>(view.world) / 1000.0 << ',' << view.packets << ',' << view.draws << ',' << view.meshes << ','
                   << view.triangles << ',' << view.complete << ',' << view.physical_draws << ',' << view.submitted_triangles << '\n';
        }

        metadata << "{\"schema\":2,\"active\":false,\"overflow\":" << (state.overflow ? "true" : "false")
                 << ",\"dropped\":" << state.dropped << ",\"completed_views\":" << state.completed
                 << ",\"first_demo_frame\":" << (state.view_count ? state.views[0].frame.demo_frame : 0)
                 << ",\"last_demo_frame\":" << (state.view_count ? state.views[state.view_count - 1].frame.demo_frame : 0)
                 << ",\"first_client_time\":" << (state.view_count ? state.views[0].frame.client_time : 0)
                 << ",\"last_client_time\":" << (state.view_count ? state.views[state.view_count - 1].frame.client_time : 0)
                 << ",\"demo\":";
        JsonString(metadata, state.demo);
        metadata << ",\"identity_provenance\":\"host_frame=NitroApi;render_frame=engine;output_id=unavailable;view_id=capture\","
                 << "\"max_bytes\":" << kMaxBytes << ",\"allocated_bytes\":" << state.allocated << ",\"stop_reason\":";
        JsonString(metadata, state.stop_reason);
        metadata << "}\n";

        return phases.good() && metadata.good();
    }
} // namespace render
