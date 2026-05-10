#include "CmdChecker.h"
#include <utility>
#include <nitro_utils/string_utils.h>

CmdChecker::CmdChecker(
    std::shared_ptr<CommandLoggerInterface> cmd_logger,
    std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider
) :
    cmd_logger_(std::move(cmd_logger))
{
    InitBlockedCommands(config_provider);
}

void CmdChecker::FilterCmd(const std::string_view& cmd, CommandSource command_source, std::string& out)
{
    out.clear();

    if (cmd.empty())
    {
        return;
    }

    if (cmd.length() == 1 && cmd[0] == '\n')
    {
        out = '\n';
        return;
    }

    size_t cur_pos = 0;
    SplitData split_data;

    while (GetNextSplitToken(cmd, [](char ch) { return ch == ';'; }, &cur_pos, split_data))
    {
        if (split_data.token.empty())
        {
            continue;
        }

        bool is_cmd_allowed = FilterSingleCmd(split_data.token, command_source, out);
        if (is_cmd_allowed && split_data.has_delimiter())
        {
            out += split_data.delimiter;
        }

        LogCmd(is_cmd_allowed, split_data.token, command_source);
    }

    if (IsCommandFromServer(command_source) && !out.empty() && !out.ends_with('\n') && !out.ends_with(';'))
    {
        out += ';';
    }
}

bool CmdChecker::FilterSingleCmd(const std::string_view& cmd, CommandSource command_source, std::string& out)
{
    size_t pos = 0;
    SplitData first_cmd_token;
    if (!GetNextSplitToken(cmd, [](char ch) { return ch <= ' ' || ch == ':'; }, &pos, first_cmd_token))
    {
        out.append(cmd);
        return true;
    }

    if (nitro_utils::contains(first_cmd_token.token, "dlfile", nitro_utils::CompareOptions::RegisterIndependent))
    {
        return false;
    }

    auto it = blocked_commands_.find(first_cmd_token.token);
    if (it == blocked_commands_.end())
    {
        out.append(cmd);
        return true;
    }

    CmdBlockType block_type = it->second;
    if ((block_type == CmdBlockType::Any) || ((block_type == CmdBlockType::OnlyServer) && IsCommandFromServer(command_source)))
    {
        return false;
    }

    out.append(cmd);
    return true;
}

void CmdChecker::LogCmd(bool is_cmd_allowed, const std::string_view& cmd, CommandSource command_source)
{
    std::string_view log_cmd_name;
    std::string_view log_cmd_value;
    TokenizeCmdForLogger(cmd, log_cmd_name, log_cmd_value);

    if (is_cmd_allowed)
    {
        if (IsCommandFromServer(command_source) && !log_cmd_name.empty())
        {
            std::string log_cmd_name_string(log_cmd_name);
            std::string log_cmd_value_string(log_cmd_value);

            cmd_logger_->LogCommand(log_cmd_name_string.c_str(), log_cmd_value_string.c_str(), LogCommandType::AllowServerCommand);
        }
    }
    else
    {
        if (!log_cmd_name.empty())
        {
            std::string log_cmd_name_string(log_cmd_name);
            std::string log_cmd_value_string(log_cmd_value);

            LogCommandType log_command = GetBlockedLogCommandType(command_source);
            cmd_logger_->LogCommand(log_cmd_name_string.c_str(), log_cmd_value_string.c_str(), log_command);
        }
    }
}

void CmdChecker::InitBlockedCommands(std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider)
{
    auto blocked_commands = config_provider->get_all_values("cmd_filter");
    if (!blocked_commands)
    {
        return;
    }

    for (const auto& [key, value] : *blocked_commands)
    {
        CmdBlockType block_type;

        if (value == "0")
        {
            block_type = CmdBlockType::Any;
        }
        else if (value == "1" || value == "2")
        {
            block_type = CmdBlockType::OnlyServer;
        }
        else
        {
            continue;
        }

        blocked_commands_[key] = block_type;
    }
}

bool CmdChecker::GetNextSplitToken(
    const std::string_view& text,
    const std::function<bool(char)>& is_delim,
    size_t* cur_pos,
    SplitData& split_data
)
{
    size_t text_length = text.length();

    size_t token_start = 0;
    size_t token_length = 0;

    char delimiter = 0;
    size_t i = 0;

    for (i = *cur_pos; i < text_length;)
    {
        token_start = i;
        delimiter = 0;

        size_t quotes = 0;

        for (; i < text_length; i++)
        {
            if (text[i] == '"')
            {
                quotes++;
            }

            if (quotes % 2 == 0 && is_delim(text[i]))
            {
                delimiter = text[i];
                break; // don't break if inside a quoted string
            }

            if (text[i] == '\n')
            {
                delimiter = '\n';
                break;
            }
        }

        token_length = i - token_start;

        // Necessarily step over \n or other delimiter
        if (i < text_length && (text[i] == '\n' || is_delim(text[i])))
        {
            i++;
        }

        if (token_length > 0)
        {
            break;
        }
    }

    *cur_pos = i;
    if (token_length > 0)
    {
        split_data.token = std::string_view(text).substr(token_start, token_length);
        split_data.delimiter = delimiter;

        return true;
    }
    return false;
}

bool CmdChecker::TokenizeCmdForLogger(const std::string_view& text, std::string_view& name, std::string_view& value)
{
    size_t cur_pos = 0;
    SplitData split_data;

    if (GetNextSplitToken(text, [](char ch) { return ch <= ' ' || ch == ':'; }, &cur_pos, split_data))
    {
        name = split_data.token;
        value = std::string_view(text).substr(cur_pos);

        return true;
    }

    return false;
}

bool CmdChecker::IsCommandFromServer(CommandSource command_source)
{
    return command_source != CommandSource::Console;
}

LogCommandType CmdChecker::GetBlockedLogCommandType(CommandSource command_source)
{
    LogCommandType log_command;

    switch (command_source)
    {
        case CommandSource::Stufftext:
            log_command = LogCommandType::BlockedStufftextCommand;
            break;

        case CommandSource::Director:
            log_command = LogCommandType::BlockedDirectorCommand;
            break;

        default:
            log_command = LogCommandType::BlockedAllCommand;
            break;
    }

    return log_command;
}
