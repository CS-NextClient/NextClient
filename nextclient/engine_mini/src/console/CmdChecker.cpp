#include "CmdChecker.h"
#include <utility>
#include <nitro_utils/string_utils.h>

using namespace nitro_utils;

CmdChecker::CmdChecker(std::shared_ptr<CommandLoggerInterface> cmd_logger,
                       std::shared_ptr<ConfigProviderInterface> config_provider) :
    cmd_logger_(std::move(cmd_logger)),
    config_provider_(std::move(config_provider))
{
}

std::string CmdChecker::GetFilteredCmd(const std::string_view& text, CommandSource command_source)
{
    if (text.empty())
    {
        return "";
    }

    if (text.length() == 1 && text[0] == '\n')
    {
        return "\n";
    }

    if (IsCommandFromServer(command_source) &&
        !text.ends_with('\n') && !text.ends_with(';'))
    {
        return GetFilteredCmdInternal(std::string(text) + ';', command_source);
    }

    return GetFilteredCmdInternal(text, command_source);
}

std::string CmdChecker::GetFilteredCmdInternal(const std::string_view& text, CommandSource command_source)
{
    std::string filtered_cmd;
    auto cmds = SplitToCmds(text);
    for (auto& cmd_data : cmds)
    {
        auto& cmd = cmd_data.token;

        if (cmd.empty())
            continue;

        FilterCmdResult filter_cmd_result = FilterCmd(cmd, command_source);

        std::string_view log_cmd_name;
        std::string_view log_cmd_value;
        TokenizeCmdForLogger(cmd, log_cmd_name, log_cmd_value);

        if (filter_cmd_result.get_result())
        {
            if (filter_cmd_result.get_filtered_cmd() != nullptr)
            {
                filtered_cmd.append(*filter_cmd_result.get_filtered_cmd());
            }
            else
            {
                filtered_cmd.append(cmd);
            }

            if (cmd_data.has_delimiter())
            {
                filtered_cmd.append(std::string(1, cmd_data.delimiter));
            }

            if (IsCommandFromServer(command_source) && !log_cmd_name.empty())
            {
                cmd_logger_->LogCommand(std::string(log_cmd_name), std::string(log_cmd_value), LogCommandType::AllowServerCommand);
            }
        }
        else
        {
            if (!log_cmd_name.empty())
            {
                LogCommandType log_command = GetLogCommandType(command_source);
                cmd_logger_->LogCommand(std::string(log_cmd_name), std::string(log_cmd_value), log_command);
            }
        }
    }

    return filtered_cmd;
}

CmdChecker::FilterCmdResult CmdChecker::FilterCmd(const std::string_view& cmd, CommandSource command_source)
{
    auto cmd_tokens = SplitCmdToTokens(cmd);

    if (cmd_tokens.empty())
        return FilterCmdResult(true); // normally false, but for russian language

    std::string cmd_name(cmd_tokens[0].token);
    nitro_utils::to_lower(cmd_name);

    // block all commands contains 'dlfile'
    if (nitro_utils::contains(cmd_name, "dlfile"))
        return FilterCmdResult(false);

    auto blocked_commands_option = config_provider_->get_all_values("cmd_filter");
    if (!blocked_commands_option)
        return FilterCmdResult(true);

    auto blocked_commands = *blocked_commands_option;
    auto blocked_bind_keys_option = config_provider_->get_all_values("bind_filter");

    // Check command for contains in blocked commands dictionary
    if (blocked_commands.find(cmd_name) != blocked_commands.end())
    {
        auto type = (CmdBlockType)std::stoi(blocked_commands.at(cmd_name));

        if (type == CmdBlockType::Any)
            return FilterCmdResult(false);

        if (type == CmdBlockType::OnlyServer && IsCommandFromServer(command_source))
            return FilterCmdResult(false);

        if (type == CmdBlockType::BindSpecial && IsCommandFromServer(command_source) && blocked_bind_keys_option)
        {
            if (cmd_tokens.size() != 3)
            {
                // Argc may be 2 (ex.: bind F3, show current bind)
                // ERROR: wrong bind command
                return FilterCmdResult(false);
            }

            std::string key(cmd_tokens[1].token);
            std::string value(cmd_tokens[2].token);

            nitro_utils::trim(key, '\"');
            nitro_utils::trim(value, '\"');
            nitro_utils::to_lower(key);

            if (blocked_bind_keys_option->find(key) != blocked_bind_keys_option->end())
                return FilterCmdResult(false); // This bind is blocked

            // Recursion checking
            std::string filtered_bind_value = GetFilteredCmdInternal(value, command_source);

            if (filtered_bind_value.empty())
                return FilterCmdResult(false); // All bind command was banned

            auto filtered_cmd = std::make_shared<std::string>("bind " + key + " \"" + filtered_bind_value + "\"");
            return FilterCmdResult(true, filtered_cmd);
        }
    }

    return FilterCmdResult(true);
}

bool CmdChecker::GetNextSplitToken(
    const std::string_view& text,
    const std::function<bool(char)>& is_delim,
    size_t* cur_pos,
    SplitData& cmd_split_data)
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
                break;  // don't break if inside a quoted string
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
        cmd_split_data.token = std::string_view(text).substr(token_start, token_length);
        cmd_split_data.delimiter = delimiter;

        return true;
    }
    return false;
}

std::vector<CmdChecker::SplitData> CmdChecker::SplitWithNewLineAndQuotesRespect(const std::string_view& text, const std::function<bool(char)>& is_delim)
{
    std::vector<SplitData> cmds;

    size_t cur_pos = 0;
    while (true)
    {
        SplitData cmd_split_data;
        if (GetNextSplitToken(text, is_delim, &cur_pos, cmd_split_data))
        {
            cmds.emplace_back(cmd_split_data);
        }

        if (cur_pos >= text.length())
            break;
    }

    return cmds;
}

std::vector<CmdChecker::SplitData> CmdChecker::SplitToCmds(const std::string_view &text)
{
    return SplitWithNewLineAndQuotesRespect(text, [](char ch)
    { return ch == ';'; });
}

std::vector<CmdChecker::SplitData> CmdChecker::SplitCmdToTokens(const std::string_view &text)
{
    return SplitWithNewLineAndQuotesRespect(text, [](char ch)
    { return ch <= ' ' || ch == ':'; });
}

bool CmdChecker::TokenizeCmdForLogger(const std::string_view &text, std::string_view& name, std::string_view& value)
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

LogCommandType CmdChecker::GetLogCommandType(CommandSource command_source)
{
    LogCommandType log_command;

    switch (command_source)
    {
    case CommandSource::Stufftext:
        log_command = LogCommandType::BlockedStufftextComamnd;
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

bool CmdChecker::IsCommandFromServer(CommandSource command_source)
{
    return command_source != CommandSource::Console;
}
