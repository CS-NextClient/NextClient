#pragma once
#include "ConfigProviderInterface.h"

#include <unordered_map>
#include <vector>
#include <string>
#include <functional>

#include <nitro_utils/transparent_string_lookup.h>

namespace nitro_utils
{
    class FileConfigProvider : public ConfigProviderInterface
    {
        enum class SectionType
        {
            KeyValue,
            List
        };

        transparent_string_map<transparent_string_map<std::string>> key_value_sections_;
        transparent_string_map<std::vector<std::string>> list_sections_;

        std::string file_path_;

    public:
        explicit FileConfigProvider(const std::string& file_path);

        void ReloadFromFile();

        [[nodiscard]] std::optional<std::vector<std::string>> get_list(const std::string &list_section) override;
        [[nodiscard]] std::optional<std::string>get_value(const std::string &key_value_section, const std::string &key) override;
        [[nodiscard]] std::optional<transparent_string_map<std::string>> get_all_values(const std::string& key_value_section) override;

        bool set_value(const std::string &key_value_section, const std::string &key, const std::string &value, bool update_config_file = false);

        [[nodiscard]] std::string file_path() const { return file_path_; }

    private:
        void ParseFile(const std::string& file_path,
                       const std::function<void(const std::string& section, const std::string& list_item, int line)>& on_list_item,
                       const std::function<void(const std::string& section, const std::string& key, const std::string& value, int line)>& on_key_valuestatic );
        void RemoveUtf8Bom(std::string& line, int line_num);
        void RemoveComments(std::string& linestatic);
        bool TryParseSection(const std::string& line, std::string & section_name_out, SectionType& section_type_out);

        std::vector<std::string> ReadAllLinesFromConfig();
    };
}

