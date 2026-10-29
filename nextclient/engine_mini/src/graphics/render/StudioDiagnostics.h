#pragma once

#include <ostream>
#include <vector>
#include <ncl_math/color.h>

#include "graphics/render/StudioDump.h"
#include "graphics/render/StudioEngineBindings.h"

namespace render
{
    class StudioDiagnostics
    {
    private:
        friend class StudioDiagnosticsTestAccess;
        struct Record
        {
            StudioSubmission fields{};
            char model[64]{};
            char header[64]{};
            char reason[256]{};
            bool drawn{};
        };
        struct ElightWatch
        {
            double until{};
            int frames{};
            int frames_with_light{};
            int most_at_once{};
            bool seen{};
            ncl_math::Vector3 origin;
            float radius{};
            ncl_math::Color color;
            unsigned int key{};
        };
        StudioEngineBindings& engine_;
        bool armed_{};
        bool await_light_{};
        char name_[64] = "studio_dump";
        std::vector<Record> records_;
        int count_{};
        int dropped_{};
        ElightWatch watch_;

    public:
        explicit StudioDiagnostics(StudioEngineBindings& engine) :
            engine_(engine)
        {}
        bool is_armed() const
        {
            return armed_;
        }
        bool needs_scene_end() const
        {
            return armed_ || watch_.until != 0.0;
        }
        void Arm(const char* name, bool await_light);
        void Watch(double seconds, double time);
        void Clear();
        void Note(const StudioSubmission& submission, const StudioOutcome& outcome);
        void OnSceneEnd(const char* directory, double time);

    private:
        int CollectReasons(const char** reasons, int* counts, int capacity);
        int DrawnCount();
        void WriteEntityLights(std::ostream& file, char* line, size_t size);
        void WriteFile(const char* directory);
        void SampleEntityLights(double time);
        int AliveEntityLights();
        static void FormatRecord(int index, const Record& record, char* out, size_t size);
    };
} // namespace render
