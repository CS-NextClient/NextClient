#include <gtest/gtest.h>
#include <next_launcher/video_settings.h>

#include "next_launcher/CommandLine.h"

TEST(VideoSettingsTest, EngineDefaultPreservesExternalLaunchFlags)
{
    CCommandLine source;
    source.CreateCmdLine("cstrike.exe -nomsaa -nofbo -windowed -w 1280");
    CCommandLine session;

    video_settings::PrepareEngineCommandLine(session, source, -1);

    EXPECT_STREQ(session.GetCmdLine(), "cstrike.exe -nomsaa -nofbo -windowed -w 1280");
    EXPECT_STREQ(source.GetCmdLine(), "cstrike.exe -nomsaa -nofbo -windowed -w 1280");
}

TEST(VideoSettingsTest, DisabledMsaaAppliesOnlyToEngineSession)
{
    CCommandLine source;
    source.CreateCmdLine("cstrike.exe -windowed -w 1280");
    CCommandLine session;

    video_settings::PrepareEngineCommandLine(session, source, 0);

    ASSERT_NE(session.CheckParm("-nomsaa"), nullptr);
    EXPECT_STREQ(source.GetCmdLine(), "cstrike.exe -windowed -w 1280");
}

TEST(VideoSettingsTest, FourSamplesOverridesConflictingExternalFlags)
{
    CCommandLine source;
    source.CreateCmdLine("cstrike.exe -nomsaa -nofbo -windowed -w 1280");
    CCommandLine session;

    video_settings::PrepareEngineCommandLine(session, source, 4);

    EXPECT_EQ(session.CheckParm("-nomsaa"), nullptr);
    EXPECT_EQ(session.CheckParm("-nofbo"), nullptr);
    EXPECT_NE(session.CheckParm("-windowed"), nullptr);
    EXPECT_NE(session.CheckParm("-w 1280"), nullptr);
    EXPECT_STREQ(source.GetCmdLine(), "cstrike.exe -nomsaa -nofbo -windowed -w 1280");
}

TEST(VideoSettingsTest, ReturningToDefaultRestoresOriginalDisabledMsaa)
{
    CCommandLine source;
    source.CreateCmdLine("cstrike.exe -nomsaa -nofbo");
    CCommandLine session;

    video_settings::PrepareEngineCommandLine(session, source, 4);
    video_settings::PrepareEngineCommandLine(session, source, -1);

    EXPECT_STREQ(session.GetCmdLine(), "cstrike.exe -nomsaa -nofbo");
}

TEST(VideoSettingsTest, ReturningToDefaultDiscardsInjectedDisabledMsaa)
{
    CCommandLine source;
    source.CreateCmdLine("cstrike.exe -windowed");
    CCommandLine session;

    video_settings::PrepareEngineCommandLine(session, source, 0);
    video_settings::PrepareEngineCommandLine(session, source, -1);

    EXPECT_STREQ(session.GetCmdLine(), "cstrike.exe -windowed");
}

TEST(VideoSettingsTest, UnsupportedPersistedSampleCountUsesEngineDefault)
{
    CCommandLine source;
    source.CreateCmdLine("cstrike.exe -nomsaa -nofbo");
    CCommandLine session;

    video_settings::PrepareEngineCommandLine(session, source, 8);

    EXPECT_STREQ(session.GetCmdLine(), "cstrike.exe -nomsaa -nofbo");
    EXPECT_EQ(video_settings::NormalizeMsaaMode(8), -1);
    EXPECT_EQ(video_settings::NormalizeMsaaMode(-2), -1);
}
