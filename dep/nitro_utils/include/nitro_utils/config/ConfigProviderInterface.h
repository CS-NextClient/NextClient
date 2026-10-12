#pragma once
#include <stdexcept>
#include <string>
#include <vector>
#include <optional>

#include <nitro_utils/transparent_string_lookup.h>

namespace nitro_utils
{
    class ConfigProviderInterface
    {
    public:
        [[nodiscard]] virtual std::optional<std::vector<std::string>> get_list(const std::string& list_section) = 0;
        [[nodiscard]] virtual std::optional<std::string> get_value(const std::string& key_value_section, const std::string& key) = 0;
        [[nodiscard]] virtual std::optional<transparent_string_map<std::string>> get_all_values(const std::string& key_value_section) = 0;

        [[nodiscard]] std::string get_value_string(
            const std::string& key_value_section,
            const std::string& key,
            const std::string& default_value
        )
        {
            auto option = get_value(key_value_section, key);
            if (option)
            {
                return *option;
            }

            return default_value;
        }

        [[nodiscard]] std::string get_value_string(const std::string& key, const std::string& default_value)
        {
            return get_value_string("", key, default_value);
        }

        [[nodiscard]] int32_t get_value_int(const std::string& key_value_section, const std::string& key, int32_t default_value)
        {
            int32_t value = default_value;

            auto option = get_value(key_value_section, key);
            if (option)
            {
                try
                {
                    value = std::stoi(*option);
                }
                catch (const std::out_of_range& e)
                {}
                catch (const std::invalid_argument& e)
                {}
            }

            return value;
        }

        [[nodiscard]] int32_t get_value_int(const std::string& key, int32_t default_value)
        {
            return get_value_int("", key, default_value);
        }
    };
} // namespace nitro_utils
