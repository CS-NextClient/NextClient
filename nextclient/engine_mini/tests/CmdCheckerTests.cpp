#include <gtest/gtest.h>

#include "console/CmdChecker.h"

#include "mocks/LoggerMock.h"
#include "mocks/CmdCheckerConfigProviderMock.h"

using ::testing::_;
using ::testing::StrEq;

class ParseCmdTest : public testing::Test
{
protected:
    std::shared_ptr<LoggerMock> logger_mock_;
    std::shared_ptr<CmdCheckerConfigProviderMock> config_provider_mock_;

    nitro_utils::transparent_string_map<std::string> blocked_commands{
        {"bind", std::to_string((int)CmdBlockType::OnlyServer)},
        {"disconnect", std::to_string((int)CmdBlockType::OnlyServer)},
        {"cl_forwardspeed", std::to_string((int)CmdBlockType::OnlyServer)},
        {"connect", std::to_string((int)CmdBlockType::OnlyServer)},
        {"bad_cmd", std::to_string((int)CmdBlockType::Any)},
    };

    void SetUp() override
    {
        logger_mock_ = std::make_shared<LoggerMock>();
        config_provider_mock_ = std::make_shared<CmdCheckerConfigProviderMock>();
        config_provider_mock_->AddKeyValueSection("cmd_filter", blocked_commands);
    }

    std::string Filter(const std::string& input, CommandSource source = CommandSource::Stufftext)
    {
        EXPECT_CALL(*config_provider_mock_, get_all_values(_));

        std::string result;
        CmdChecker(logger_mock_, config_provider_mock_).FilterCmd(input, source, result);
        return result;
    }
};

// ====================================
//
// A. Edge cases
//
// ====================================

TEST_F(ParseCmdTest, EmptyInput_ReturnsEmpty)
{
    EXPECT_EQ("", Filter(""));
}

TEST_F(ParseCmdTest, SingleNewline_ReturnsNewline)
{
    EXPECT_EQ("\n", Filter("\n"));
}

// ====================================
//
// B. CommandSource blocking behaviour
//
// ====================================

TEST_F(ParseCmdTest, Console_AllowsOnlyServerBlockedCommand)
{
    EXPECT_EQ("bind x", Filter("bind x", CommandSource::Console));
}

TEST_F(ParseCmdTest, Stufftext_BlocksOnlyServerCommand)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("bind"), StrEq("x"), LogCommandType::BlockedStufftextCommand));

    EXPECT_EQ("", Filter("bind x"));
}

TEST_F(ParseCmdTest, Director_BlocksOnlyServerCommand)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("bind"), StrEq("x"), LogCommandType::BlockedDirectorCommand));

    EXPECT_EQ("", Filter("bind x", CommandSource::Director));
}

TEST_F(ParseCmdTest, ConnectionlessPacket_BlocksOnlyServerCommand)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("bind"), StrEq("x"), LogCommandType::BlockedAllCommand));

    EXPECT_EQ("", Filter("bind x", CommandSource::ConnectionlessPacket));
}

TEST_F(ParseCmdTest, Console_BlocksAnyCommand)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("bad_cmd"), StrEq(""), LogCommandType::BlockedAllCommand));

    EXPECT_EQ("", Filter("bad_cmd", CommandSource::Console));
}

// ====================================
//
// C. Hardcoded dlfile block
//
// ====================================

TEST_F(ParseCmdTest, Dlfile_BlockedForConsole)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("dlfile"), StrEq("model.mdl"), LogCommandType::BlockedAllCommand));

    EXPECT_EQ("", Filter("dlfile model.mdl", CommandSource::Console));
}

TEST_F(ParseCmdTest, Dlfile_BlockedForStufftext)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("dlfile"), StrEq("model.mdl"), LogCommandType::BlockedStufftextCommand));

    EXPECT_EQ("", Filter("dlfile model.mdl"));
}

TEST_F(ParseCmdTest, Dlfile_BlockedForDirector)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("dlfile"), StrEq("model.mdl"), LogCommandType::BlockedDirectorCommand));

    EXPECT_EQ("", Filter("dlfile model.mdl", CommandSource::Director));
}

TEST_F(ParseCmdTest, Dlfile_BlockedForConnectionlessPacket)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("dlfile"), StrEq("model.mdl"), LogCommandType::BlockedAllCommand));

    EXPECT_EQ("", Filter("dlfile model.mdl", CommandSource::ConnectionlessPacket));
}

TEST_F(ParseCmdTest, Dlfile_Substring_AlsoBlocked)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("mydlfile"), StrEq("x"), LogCommandType::BlockedStufftextCommand));
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("dlfile2"), StrEq("x"), LogCommandType::BlockedStufftextCommand));

    EXPECT_EQ("", Filter("mydlfile x"));
    EXPECT_EQ("", Filter("dlfile2 x"));
}

// ====================================
//
// D. Delimiter handling
//
// ====================================

TEST_F(ParseCmdTest, SemicolonSeparator)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("a"), StrEq(""), LogCommandType::AllowServerCommand));
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("b"), StrEq(""), LogCommandType::AllowServerCommand));

    EXPECT_EQ("a;b;", Filter("a;b"));
}

TEST_F(ParseCmdTest, NewlineSeparator)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("a"), StrEq(""), LogCommandType::AllowServerCommand));
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("b"), StrEq(""), LogCommandType::AllowServerCommand));

    EXPECT_EQ("a\nb;", Filter("a\nb"));
}

TEST_F(ParseCmdTest, MixedSeparators)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("a"), StrEq(""), LogCommandType::AllowServerCommand));
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("b"), StrEq(""), LogCommandType::AllowServerCommand));
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("c"), StrEq(""), LogCommandType::AllowServerCommand));

    EXPECT_EQ("a;b\nc;", Filter("a;b\nc"));
}

TEST_F(ParseCmdTest, SemicolonInsideQuotes_IsNotDelimiter)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("cmd"), StrEq("\"a;b\""), LogCommandType::AllowServerCommand));

    EXPECT_EQ("cmd \"a;b\";", Filter("cmd \"a;b\""));
}

TEST_F(ParseCmdTest, NewLineInsideQuotes_IsDelimiter)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("cmd"), StrEq("\"a"), LogCommandType::AllowServerCommand));
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("b\""), StrEq(""), LogCommandType::AllowServerCommand));

    EXPECT_EQ("cmd \"a\nb\";", Filter("cmd \"a\nb\""));
}

TEST_F(ParseCmdTest, TrailingNewline_PreservesNewline)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("a"), StrEq(""), LogCommandType::AllowServerCommand));

    EXPECT_EQ("a\n", Filter("a\n"));
}

TEST_F(ParseCmdTest, TrailingSemicolon_PreservesSemicolon)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("a"), StrEq(""), LogCommandType::AllowServerCommand));

    EXPECT_EQ("a;", Filter("a;"));
}

// ====================================
//
// E. Result string post-processing
//
// ====================================

TEST_F(ParseCmdTest, ServerSource_AppendsSemicolonWhenMissing)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("cmd"), StrEq(""), LogCommandType::AllowServerCommand));

    EXPECT_EQ("cmd;", Filter("cmd"));
}

TEST_F(ParseCmdTest, ConsoleSource_DoesNotAppendSemicolon)
{
    EXPECT_EQ("cmd", Filter("cmd", CommandSource::Console));
}

TEST_F(ParseCmdTest, BlockedCommand_RemovesItsDelimiter)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("bind"), StrEq("x"), LogCommandType::BlockedStufftextCommand));
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("allowed"), StrEq(""), LogCommandType::AllowServerCommand));

    EXPECT_EQ("allowed;", Filter("bind x;allowed"));
}

TEST_F(ParseCmdTest, BlockedCommandAtEnd_NoTrailingDelimiter)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("bind"), StrEq("x"), LogCommandType::BlockedStufftextCommand));
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("allowed"), StrEq(""), LogCommandType::AllowServerCommand));

    EXPECT_EQ("allowed;", Filter("allowed;bind x"));
}

// ====================================
//
// F. Config absence
//
// ====================================

TEST_F(ParseCmdTest, NoConfig_AllowsEverything)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("bind"), StrEq("x"), LogCommandType::AllowServerCommand));

    auto config_provider = std::make_shared<CmdCheckerConfigProviderMock>();
    EXPECT_CALL(*config_provider, get_all_values(_));

    std::string result;
    CmdChecker(logger_mock_, config_provider).FilterCmd("bind x", CommandSource::Stufftext, result);

    EXPECT_EQ("bind x;", result);
}

// ====================================
//
// G. Complex scenarios
//
// ====================================

TEST_F(ParseCmdTest, MixedStufftext_FilterAndLog)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("bind"), StrEq("\"END\" BLOCKED"), LogCommandType::BlockedStufftextCommand));
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("disconnect"), StrEq(""), LogCommandType::BlockedStufftextCommand));
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("cl_forwardspeed"), StrEq("400"), LogCommandType::BlockedStufftextCommand));
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("cmd_from_server1"), StrEq(""), LogCommandType::AllowServerCommand));
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("cmd_from_server2"), StrEq("\"val2\""), LogCommandType::AllowServerCommand));
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("cmd_from_server3"), StrEq("val3"), LogCommandType::AllowServerCommand));

    std::string input =
        "bind \"END\" BLOCKED;cmd_from_server1\ndisconnect\ncl_forwardspeed 400;cmd_from_server2 \"val2\"\n\n;cmd_from_server3 val3";
    std::string expected = "cmd_from_server1\ncmd_from_server2 \"val2\"\ncmd_from_server3 val3;";

    EXPECT_EQ(expected, Filter(input));
}

TEST_F(ParseCmdTest, SingleCommandWithTrailingNewline)
{
    EXPECT_CALL(*logger_mock_, LogCommand(StrEq("connect_set_nick"), StrEq(""), LogCommandType::AllowServerCommand));

    EXPECT_EQ("connect_set_nick\n", Filter("connect_set_nick\n"));
}
