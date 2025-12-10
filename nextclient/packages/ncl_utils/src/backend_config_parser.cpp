#include <fstream>
#include <tao/json.hpp>
#include <ncl_utils/backend_config_data/Config.h>

using namespace ncl_utils::backend_config_data;

namespace ncl_utils
{
    template<typename T>
    struct BackendJsonParserTraits : tao::json::traits<T> { };

    template<>
    struct BackendJsonParserTraits<Payload>
    {
        static Payload from_json(const tao::json::value& v)
        {
            Payload p;

            auto* addresses_obj = v.find("addresses");
            if (addresses_obj != nullptr && addresses_obj->is_array())
            {
                const auto& addresses_array = addresses_obj->get_array();
                for (const auto& item : addresses_array)
                {
                    p.addresses.push_back(item.get_string());
                }
            }

            return p;
        }
    };

    template<>
    struct BackendJsonParserTraits<Config>
    {
        static Config from_json(const tao::json::value& v)
        {
            Config c;
            c.payload = BackendJsonParserTraits<Payload>::from_json(v.at("payload"));

            auto* signature = v.find("signature");
            if (signature != nullptr)
            {
                c.signature = signature->get_string();
            }
            return c;
        }
    };

    Config ParseBackendConfig(const std::string& config_path)
    {
        try
        {
            std::ifstream file(config_path);
            if (!file)
            {
                return {};
            }

            std::string json_content((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());

            tao::json::value v = tao::json::from_string(json_content);
            Config data = BackendJsonParserTraits<Config>::from_json(v);

            return data;
        }
        catch (const std::exception& e)
        {
            return {};
        }
    }
}
