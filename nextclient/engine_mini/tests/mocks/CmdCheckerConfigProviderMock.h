#pragma once
#include <utility>

#undef Assert

#include <gmock/gmock.h>
#include <nitro_utils/config_utils.h>

class CmdCheckerConfigProviderMock : public nitro_utils::ConfigProviderInterface
{
    nitro_utils::transparent_string_map<nitro_utils::transparent_string_map<std::string>> key_value_sections_;
    nitro_utils::transparent_string_map<std::vector<std::string>> list_sections_;

public:
    MOCK_METHOD(std::optional<std::vector<std::string>>, get_list, (const std::string& list_section), (override));
    MOCK_METHOD(std::optional<std::string>, get_value, (const std::string& key_value_section, const std::string& key), (override));
    MOCK_METHOD(
        std::optional<nitro_utils::transparent_string_map<std::string>>,
        get_all_values,
        (const std::string& key_value_section),
        (override)
    );

    explicit CmdCheckerConfigProviderMock()
    {
        ON_CALL(*this, get_list).WillByDefault([this](const std::string& list_section)
            -> std::optional<std::vector<std::string>> {
            auto it = list_sections_.find(list_section);
            if (it == list_sections_.end()) return std::nullopt;
            return it->second;
        });

        ON_CALL(*this, get_value).WillByDefault([this](const std::string& key_value_section, const std::string& key)
            -> std::optional<std::string> {
            auto sec_it = key_value_sections_.find(key_value_section);
            if (sec_it == key_value_sections_.end()) return std::nullopt;
            auto val_it = sec_it->second.find(key);
            if (val_it == sec_it->second.end()) return std::nullopt;
            return val_it->second;
        });

        ON_CALL(*this, get_all_values).WillByDefault([this](const std::string& key_value_section)
            -> std::optional<nitro_utils::transparent_string_map<std::string>> {
            auto it = key_value_sections_.find(key_value_section);
            if (it == key_value_sections_.end()) return std::nullopt;
            return it->second;
        });
    }

    void AddListSection(const std::string& section_name, const std::vector<std::string>& values)
    {
        list_sections_[section_name] = values;
    }

    void AddKeyValueSection(const std::string& section_name, const nitro_utils::transparent_string_map<std::string>& values)
    {
        key_value_sections_[section_name] = values;
    }
};
