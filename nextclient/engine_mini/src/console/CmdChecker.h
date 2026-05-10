#pragma once
#include <memory>
#include <string>
#include <string_view>

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
};

class CmdChecker
{
public:
    struct SplitData
    {
        std::string_view token{};
        char delimiter{};

        [[nodiscard]] bool has_delimiter() const
        {
            return delimiter != 0;
        }
    };

private:
    std::shared_ptr<CommandLoggerInterface> cmd_logger_;
    nitro_utils::transparent_string_map<CmdBlockType> blocked_commands_;

public:
    explicit CmdChecker(
        std::shared_ptr<CommandLoggerInterface> cmd_logger,
        std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider
    );

    void FilterCmd(const std::string_view& cmd, CommandSource command_source, std::string& out);

private:
    bool FilterSingleCmd(const std::string_view& cmd, CommandSource command_source, std::string& out);
    void LogCmd(bool is_cmd_allowed, const std::string_view& cmd, CommandSource command_source);
    void InitBlockedCommands(std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider);

    bool GetNextSplitToken(const std::string_view& text, const std::function<bool(char)>& is_delim, size_t* cur_pos, SplitData& split_data);
    bool TokenizeCmdForLogger(const std::string_view& text, std::string_view& name, std::string_view& value);
    bool IsCommandFromServer(CommandSource command_source);
    LogCommandType GetBlockedLogCommandType(CommandSource command_source);
};
