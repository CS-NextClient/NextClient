#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "graphics/render/StudioPhaseCapture.h"

namespace
{
    std::vector<std::map<std::string, std::string>> PhaseRecords(render::StudioPhaseCapture& recorder)
    {
        recorder.Stop();

        std::ostringstream phases;
        std::ostringstream metadata;
        EXPECT_TRUE(recorder.Export(phases, metadata));

        std::istringstream rows(phases.str());
        std::string row;
        std::string column;

        std::getline(rows, row);
        std::istringstream header(row);
        std::vector<std::string> names;

        while (std::getline(header, column, ','))
        {
            names.push_back(column);
        }

        std::vector<std::map<std::string, std::string>> result;

        while (std::getline(rows, row))
        {
            std::istringstream columns(row);
            std::map<std::string, std::string> record;

            for (const std::string& name : names)
            {
                std::getline(columns, column, ',');
                record[name] = column;
            }

            result.push_back(std::move(record));
        }

        return result;
    }
} // namespace

TEST(StudioPhaseCaptureTest, DisabledRecorderDoesNotObserveInputs)
{
    render::StudioPhaseCapture capture;

    EXPECT_FALSE(capture.is_active());
    EXPECT_FALSE(capture.BeginView({}, 10));
    capture.NotePacket();
    capture.NoteGroup(1, 2, 3);
    capture.NotePhysicalDraw(4);
    EXPECT_EQ(capture.get_view_count(), 0u);

    std::ostringstream phases;
    std::ostringstream metadata;

    EXPECT_FALSE(capture.Export(phases, metadata));
}

TEST(StudioPhaseCaptureTest, CapacityOverflowIsExplicitAndExportOwnsIdentity)
{
    render::StudioPhaseCapture capture;

    ASSERT_TRUE(capture.Start({1}));

    char demo[] = "demo\"test";
    render::StudioPhaseFrame frame;
    frame.demo = demo;
    frame.demo_frame = 77;

    ASSERT_TRUE(capture.BeginView(frame, 10));
    demo[0] = 'X';
    capture.EndView(20);
    EXPECT_FALSE(capture.BeginView(frame, 30));
    EXPECT_TRUE(capture.has_overflow());
    EXPECT_FALSE(capture.is_active());

    std::ostringstream phases;
    std::ostringstream metadata;

    ASSERT_TRUE(capture.Export(phases, metadata));
    EXPECT_NE(metadata.str().find("demo\\\"test"), std::string::npos);
    EXPECT_NE(metadata.str().find("\"overflow\":true"), std::string::npos);
    EXPECT_NE(metadata.str().find("\"dropped\":1"), std::string::npos);
    EXPECT_NE(metadata.str().find("\"completed_views\":1"), std::string::npos);
}

TEST(StudioPhaseCaptureTest, IdentityTimesAndNumericWorkloadSurviveExport)
{
    render::StudioPhaseCapture capture;

    ASSERT_TRUE(capture.Start({4}));

    render::StudioPhaseFrame frame;
    frame.host_frame = 11;
    frame.render_frame = 12;
    frame.output_id = 13;
    frame.demo_frame = 100;
    frame.client_time = 8.25;
    frame.old_client_time = 8.125;

    ASSERT_TRUE(capture.BeginView(frame, 1000));
    capture.BeginPass(1, 1100);
    capture.NotePacket();
    capture.NoteGroup(4, 4, 8);
    capture.NotePhysicalDraw(12);
    capture.EndPass(1500);
    capture.BeginPass(2, 1600);
    capture.EndPass(2500);
    capture.AddWorldTime(700);
    capture.EndView(3000);

    ASSERT_TRUE(capture.BeginView(frame, 5000));
    capture.EndView(6000);

    const std::vector<std::map<std::string, std::string>> rows = PhaseRecords(capture);

    ASSERT_EQ(rows.size(), 2u);

    const std::map<std::string, std::string>& row = rows[0];

    EXPECT_EQ(row.at("host_frame"), "11");
    EXPECT_EQ(row.at("render_frame"), "12");
    EXPECT_EQ(row.at("output_id"), "13");
    EXPECT_EQ(row.at("demo_frame"), "100");
    EXPECT_EQ(row.at("client_time"), "8.25");
    EXPECT_EQ(row.at("old_client_time"), "8.125");
    EXPECT_DOUBLE_EQ(std::stod(row.at("view_cpu_us")), 2);
    EXPECT_DOUBLE_EQ(std::stod(row.at("opaque_cpu_us")), 0.4);
    EXPECT_DOUBLE_EQ(std::stod(row.at("transparent_cpu_us")), 0.9);
    EXPECT_DOUBLE_EQ(std::stod(row.at("world_cpu_us")), 0.7);
    EXPECT_EQ(row.at("packets"), "1");
    EXPECT_EQ(row.at("draws"), "4");
    EXPECT_EQ(row.at("meshes"), "4");
    EXPECT_EQ(row.at("triangles"), "8");
    EXPECT_EQ(row.at("physical_draws"), "1");
    EXPECT_EQ(row.at("submitted_triangles"), "12");
    EXPECT_EQ(row.at("complete"), "1");
    EXPECT_DOUBLE_EQ(std::stod(rows[1].at("frame_interval_us")), 4);
}

TEST(StudioPhaseCaptureTest, DemoRangeWaitsForItsFirstFrameAndStopsAfterItsLastFrame)
{
    render::StudioPhaseCapture capture;

    ASSERT_TRUE(capture.Start({4}, 10, 12));
    capture.SessionBoundary();

    render::StudioPhaseFrame frame;
    frame.demo_frame = 9;

    EXPECT_FALSE(capture.BeginView(frame, 1));
    EXPECT_TRUE(capture.is_active());

    frame.demo_frame = 10;

    EXPECT_TRUE(capture.BeginView(frame, 2));
    capture.EndView(3);

    frame.demo_frame = 12;

    EXPECT_TRUE(capture.BeginView(frame, 4));
    capture.EndView(5);

    frame.demo_frame = 13;

    EXPECT_FALSE(capture.BeginView(frame, 6));
    EXPECT_FALSE(capture.is_active());
    EXPECT_EQ(capture.get_view_count(), 2u);
}

TEST(StudioPhaseCaptureTest, SessionInvalidationDoesNotCompleteAnAbortedView)
{
    render::StudioPhaseCapture capture;

    ASSERT_TRUE(capture.Start({4}));

    ASSERT_TRUE(capture.BeginView({}, 1));
    capture.SessionBoundary("context_invalidated");
    capture.EndView(2);
    EXPECT_FALSE(capture.is_active());

    std::ostringstream phases;
    std::ostringstream metadata;

    ASSERT_TRUE(capture.Export(phases, metadata));
    EXPECT_NE(metadata.str().find("\"completed_views\":0"), std::string::npos);
    EXPECT_NE(metadata.str().find("context_invalidated"), std::string::npos);
    EXPECT_EQ(PhaseRecords(capture)[0].at("complete"), "0");
}

TEST(StudioPhaseCaptureTest, NestedViewsKeepTheirPassTimesAndWorkload)
{
    render::StudioPhaseCapture capture;

    ASSERT_TRUE(capture.Start({4}));

    ASSERT_TRUE(capture.BeginView({}, 1000));
    capture.BeginPass(1, 1100);
    capture.NotePacket();
    ASSERT_TRUE(capture.BeginView({}, 1200));
    capture.BeginPass(2, 1300);
    capture.NoteGroup(2, 2, 5);
    capture.EndPass(1500);
    capture.EndView(1600);
    capture.EndPass(1700);
    capture.EndView(1800);

    const std::vector<std::map<std::string, std::string>> rows = PhaseRecords(capture);

    ASSERT_EQ(rows.size(), 2u);
    EXPECT_DOUBLE_EQ(std::stod(rows[0].at("opaque_cpu_us")), 0.6);
    EXPECT_EQ(rows[0].at("packets"), "1");
    EXPECT_EQ(rows[0].at("draws"), "0");
    EXPECT_EQ(rows[0].at("complete"), "1");
    EXPECT_DOUBLE_EQ(std::stod(rows[1].at("transparent_cpu_us")), 0.2);
    EXPECT_EQ(rows[1].at("draws"), "2");
    EXPECT_EQ(rows[1].at("triangles"), "5");
    EXPECT_EQ(rows[1].at("complete"), "1");
}

TEST(StudioPhaseCaptureTest, HostBoundaryLeavesAbandonedViewsIncompleteAndResetAllowsRestart)
{
    render::StudioPhaseCapture capture;

    ASSERT_TRUE(capture.Start({4}));

    ASSERT_TRUE(capture.BeginView({}, 100));

    render::StudioPhaseFrame frame;
    frame.host_frame = 1;
    ASSERT_TRUE(capture.BeginView(frame, 200));
    capture.EndView(300);

    const std::vector<std::map<std::string, std::string>> rows = PhaseRecords(capture);

    EXPECT_EQ(rows[0].at("complete"), "0");
    EXPECT_EQ(rows[1].at("complete"), "1");
    EXPECT_FALSE(capture.Start({4}));

    capture.Reset();
    EXPECT_FALSE(capture.has_recording());
    EXPECT_TRUE(capture.Start({4}));
}

TEST(StudioPhaseCaptureTest, InvalidLimitsAndExportFailureRetainRecording)
{
    render::StudioPhaseCapture capture;

    EXPECT_FALSE(capture.Start({0}));
    EXPECT_FALSE(capture.Start({std::numeric_limits<size_t>::max()}));
    EXPECT_FALSE(capture.Start({4}, -1));
    EXPECT_FALSE(capture.Start({4}, 2, 1));
    ASSERT_TRUE(capture.Start({4}));

    ASSERT_TRUE(capture.BeginView({}, 1));

    std::ostringstream phases;
    std::ostringstream metadata;

    EXPECT_FALSE(capture.Export(phases, metadata));
    capture.EndView(2);
    capture.Stop();
    phases.setstate(std::ios::badbit);
    EXPECT_FALSE(capture.Export(phases, metadata));
    EXPECT_TRUE(capture.has_recording());
    phases.clear();
    EXPECT_TRUE(capture.Export(phases, metadata));
}
