#include <key_binding/key_binding.h>

#include <format>

namespace
{
    std::string_view Trim(std::string_view text)
    {
        constexpr std::string_view kBlanks = " \t\r";

        size_t first = text.find_first_not_of(kBlanks);
        if (first == std::string_view::npos) {
            return {};
        }

        size_t last = text.find_last_not_of(kBlanks);
        return text.substr(first, last - first + 1);
    }

    bool IsButton(std::string_view command)
    {
        return command.starts_with('+');
    }
}

namespace key_binding
{
    std::vector<std::string_view> SplitCommands(std::string_view binding)
    {
        std::vector<std::string_view> commands;

        bool quoted = false;
        size_t start = 0;
        for (size_t i = 0; i <= binding.size(); i++) {
            if (i < binding.size()) {
                char c = binding[i];
                if (c == '"') {
                    quoted = !quoted;
                }

                if (c != '\n' && (c != ';' || quoted)) {
                    continue;
                }
            }

            std::string_view command = Trim(binding.substr(start, i - start));
            if (!command.empty()) {
                commands.push_back(command);
            }
            start = i + 1;
        }

        return commands;
    }

    bool IsMultiButton(std::string_view binding)
    {
        return binding.starts_with('+') && SplitCommands(binding).size() > 1;
    }

    std::string PressCommands(std::string_view binding, int key)
    {
        std::string text;
        for (std::string_view command : SplitCommands(binding)) {
            if (IsButton(command)) {
                text += std::format("{} {};", command, key);
            } else {
                text += std::format("{};", command);
            }
        }

        if (!text.empty()) {
            text.back() = '\n';
        }
        return text;
    }

    std::string ReleaseCommands(std::string_view binding, int key)
    {
        std::string text;
        for (std::string_view command : SplitCommands(binding)) {
            if (IsButton(command)) {
                text += std::format("-{} {};", command.substr(1), key);
            }
        }

        if (!text.empty()) {
            text.back() = '\n';
        }
        return text;
    }
}
