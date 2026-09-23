#include <nitro_utils/config_utils.h>
#include <nitro_utils/string_utils.h>
#include <fstream>

namespace nitro_utils
{
    // NOTE: all keys and values are stored in lowercase (affected only for key-value sections)
    FileConfigProvider::FileConfigProvider(const std::string& file_path) :
        file_path_(file_path)
    {
        ReloadFromFile();
    }

    void FileConfigProvider::ReloadFromFile()
    {
        key_value_sections_.clear();
        list_sections_.clear();

        ParseFile(file_path_,
                  [this](const std::string& section, const std::string& list_item, int line) { list_sections_[section].emplace_back(list_item); },
                  [this](const std::string& section, const std::string& key, const std::string& value, int line) { key_value_sections_[section][key] = value; });
    }

    void FileConfigProvider::ParseFile(const std::string& file_path,
                                       const std::function<void(const std::string& section, const std::string& list_item, int line)>& on_list_item,
                                       const std::function<void(const std::string& section, const std::string& key, const std::string& value, int line)>& on_key_value)
    {
        std::ifstream file(file_path, std::ios::in);
        if (!file.is_open())
            return;

        std::string current_section;
        SectionType current_section_type = SectionType::KeyValue;

        std::string line;
        int line_num = 0;
        while (std::getline(file, line))
        {
            line_num++;

            RemoveUtf8Bom(line, line_num);
            RemoveComments(line);
            trim(line);

            if (line.empty())
                continue;

            if (TryParseSection(line, current_section, current_section_type))
                continue;

            if (current_section_type == SectionType::List)
            {
                on_list_item(current_section, line, line_num);
            }
            else
            {
                std::string key;
                std::string value;

                std::vector<std::string> args;
                size_t argsNum = split_in_args(line, args, 3);

                if (argsNum == 3)
                    continue;

                if (argsNum > 0)
                    key = args[0];

                if (argsNum > 1)
                    value = args[1];

                to_lower(key);
                to_lower(value);

                on_key_value(current_section, key, value, line_num);
            }
        }

        file.close();
    }

    void FileConfigProvider::RemoveUtf8Bom(std::string& line, int line_num)
    {
        if (line_num != 1)
            return;

        if (line.length() >= 3 &&
            line[0] == '\xEF' &&
            line[1] == '\xBB' &&
            line[2] == '\xBF')
        {
            line = line.substr(3);
        }
    }

    void FileConfigProvider::RemoveComments(std::string &line)
    {
        std::vector<size_t> foundComments;
        foundComments.push_back(line.find("//", 0));
        foundComments.push_back(line.find_first_of(";", 0));
        foundComments.push_back(line.find_first_of("#", 0));

        // TODO lol wtf
        size_t firstCommIdx = 100500;
        for (const auto &commIdx : foundComments)
        {
            if (commIdx != -1)
            {
                if (commIdx < firstCommIdx)
                {
                    firstCommIdx = commIdx;
                }
            }
        }

        if (firstCommIdx != 100500)
        {
            line = line.substr(0, firstCommIdx);
        }
    }

    bool FileConfigProvider::TryParseSection(const std::string &line, std::string &section_name_out, SectionType &section_type_out)
    {
        if (line.length() < 2)
            return false;

        if (line[0] != '[')
            return false;

        if (line[line.length() - 1] != ']')
            return false;

        std::string section_content = line.substr(1, line.length() - 2);
        std::vector<std::string_view> tokens;
        split(section_content, ":", tokens);

        if (tokens.empty())
            return false;

        section_type_out = SectionType::KeyValue;
        section_name_out = tokens[0];

        if (tokens.size() > 1 && tokens[1] == "list")
        {
            section_type_out = SectionType::List;
        }

        return true;
    }

    std::optional<std::vector<std::string>> FileConfigProvider::get_list(const std::string &list_section)
    {
        if (list_sections_.count(list_section) == 0)
            return std::nullopt;

        return list_sections_[list_section];
    }

    std::optional<std::string> FileConfigProvider::get_value(const std::string &key_value_section, const std::string &key)
    {
        if (key_value_sections_.count(key_value_section) == 0)
            return std::nullopt;

        auto& section = key_value_sections_[key_value_section];
        if (section.count(key) == 0)
            return std::nullopt;

        return section[key];
    }

    std::optional<transparent_string_map<std::string>> FileConfigProvider::get_all_values(const std::string& key_value_section)
    {
        if (key_value_sections_.count(key_value_section) == 0)
            return std::nullopt;

        return key_value_sections_[key_value_section];
    }

    bool FileConfigProvider::set_value(const std::string &key_value_section, const std::string &key_set, const std::string &value_set, bool update_config_file)
    {
        key_value_sections_[key_value_section][key_set] = value_set;

        if (!update_config_file)
            return true;

        int section_line = 0;
        int value_line = 0;

        ParseFile(file_path_,
                  [](const std::string& section, const std::string& list_item, int line) { },
                  [key_value_section, key_set, value_set, &section_line, &value_line](const std::string& section, const std::string& key, const std::string& value, int line)
        {
            if (key_value_section == section)
            {
                if (section_line == 0)
                    section_line = line;

                if (key_set == key && value_line == 0)
                    value_line = line;
            }
        });

        std::string key_value_cfg;
        if (contains(value_set, " "))
            key_value_cfg = key_set + " \"" + value_set + "\"";
        else
            key_value_cfg = key_set + " " + value_set;

        std::vector<std::string> lines = ReadAllLinesFromConfig();

        std::fstream file(file_path_, std::ios::out);
        if (!file.is_open())
            return false;

        // in case when section and key is not defined, and it's "zero" section
        if (section_line == 0 && value_line == 0 && key_value_section.empty())
        {
            file << key_value_cfg << std::endl;
        }

        for (int i = 0; i < lines.size(); i++)
        {
            const auto& line = lines[i];
            int line_num = i + 1;

                // in case when section is defined, but key is not
            if (section_line > 0 && section_line == line_num && value_line == 0)
            {
                file << line << std::endl;
                file << key_value_cfg << std::endl;
                continue;
            }
                // in case when section and key is defined
            else if (section_line > 0 && value_line > 0 && value_line == line_num)
            {
                file << key_value_cfg << std::endl;
                continue;
            }

            file << line << std::endl;
        }

        // in case when section and key is not defined, and it's NOT "zero" section
        if (section_line == 0 && value_line == 0 && !key_value_section.empty())
        {
            file << "[" <<key_value_section  << "]" << std::endl;
            file << key_value_cfg << std::endl;
        }

        file.close();

        return true;
    }

    std::vector<std::string> FileConfigProvider::ReadAllLinesFromConfig()
    {
        std::vector<std::string> lines;

        std::fstream file(file_path_, std::ios::in);
        if (!file.is_open())
            return {};

        std::string line;
        while (std::getline(file, line))
            lines.emplace_back(line);

        file.close();

        return lines;
    }
}