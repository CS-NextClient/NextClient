#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <optional>

#include <nitroapi/NitroApiInterface.h>
#include <nitro_utils/config_utils.h>
#include <next_engine_mini/CommandLoggerInterface.h>

enum class CommandSource
{
    Console,
    Director,
    Stufftext,
    ConnectionlessPacket,
};

enum class CmdBlockType
{
    Any = 0,
    OnlyServer = 1,
    BindSpecial = 2
};

// TODO check to conformity https://github.com/valvesoftware/halflife/issues/1497
// TODO fix for bind like: bind "END" "ALLOWED\nALLOWED2"
// TODO check https://github.com/ValveSoftware/halflife/issues/2106#issuecomment-475200434
// TODO see extra mirror for useful things
class CmdChecker
{
    class FilterCmdResult
    {
        std::shared_ptr<std::string> filtered_command_ = nullptr;
        bool result_;

    public:
        explicit FilterCmdResult(bool result) :
                result_(result) { }

        FilterCmdResult(bool result, std::shared_ptr<std::string> filtered_command) :
                result_(result), filtered_command_(std::move(filtered_command)) { }

        [[nodiscard]] bool get_result() const { return result_; }
        [[nodiscard]] std::shared_ptr<std::string> get_filtered_cmd() const { return filtered_command_; }
    };

    struct SplitData
    {
        std::string_view token;
        char delimiter;
        [[nodiscard]] bool has_delimiter() const { return delimiter != 0; }
    };

    std::shared_ptr<CommandLoggerInterface> cmd_logger_;
    std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider_;

    std::string GetFilteredCmdInternal(const std::string_view& text, CommandSource command_source);
    FilterCmdResult FilterCmd(const std::string_view& cmd, CommandSource command_source);

    // TODO move to any utils
    static bool GetNextSplitToken(const std::string_view& text, const std::function<bool(char)>& is_delim, size_t* cur_pos, CmdChecker::SplitData& cmd_split_data);
    static std::vector<SplitData> SplitWithNewLineAndQuotesRespect(const std::string_view& text, const std::function<bool(char)>& is_delim);
    static std::vector<SplitData> SplitToCmds(const std::string_view& text);
    static std::vector<SplitData> SplitCmdToTokens(const std::string_view& text);
    static bool TokenizeCmdForLogger(const std::string_view &text, std::string_view& name, std::string_view& value);
    static LogCommandType GetLogCommandType(CommandSource command_source);
    static bool IsCommandFromServer(CommandSource command_source);

public:
    explicit CmdChecker(std::shared_ptr<CommandLoggerInterface> cmd_logger,
                        std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider);

    std::string GetFilteredCmd(const std::string_view& text, CommandSource command_source);
};