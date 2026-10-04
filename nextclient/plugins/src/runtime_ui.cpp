#include "runtime_internal.h"
#include <algorithm>

namespace plugins::runtime
{
    namespace
    {
        uint64_t next_window{};
        std::string caption(const Json& value)
        {
            const auto& en = value.at("en").get_string();
            const auto& ru = value.at("ru").get_string();
            if (en.size() > 1024 || ru.size() > 1024 || en.find('\0') != std::string::npos || ru.find('\0') != std::string::npos)
                throw std::runtime_error("Invalid UI text");
            return en;
        }
        void validate_window(Loaded& p, const Json& value)
        {
            if (!permitted(&p, NC_PERMISSION_UI_WINDOWS))
                throw std::runtime_error("UI permission required");
            const bool interactive = value.at("interactive").get_boolean();
            if (interactive && !permitted(&p, NC_PERMISSION_UI_INPUT))
                throw std::runtime_error("Input permission required");
            caption(value.at("title"));
            if (auto menu = value.find("menu"))
            {
                caption(*menu);
                if (!interactive)
                    throw std::runtime_error("Menu entries require input permission");
            }
            const auto surface = value.at("surface").get_string();
            if (surface != "menu" && surface != "game" && surface != "all")
                throw std::runtime_error("Unknown UI surface");
            integer(value.at("width"), 160, 1600);
            integer(value.at("height"), 100, 1200);
            (void)value.at("visible").get_boolean();
            std::set<std::string> ids;
            if (value.at("items").get_array().size() > 64)
                throw std::runtime_error("Widget limit");
            for (const auto& item : value.at("items").get_array())
            {
                auto id = item.at("id").get_string(), kind = item.at("kind").get_string();
                if (!valid_id(id) || !ids.insert(id).second)
                    throw std::runtime_error("Invalid widget ID");
                caption(item.at("text"));
                if (auto row = item.find("row"))
                    integer(*row, 0, 63);
                if (kind != "label" && kind != "image" && kind != "button" && kind != "checkbox" && kind != "slider" && kind != "text" &&
                    kind != "list")
                    throw std::runtime_error("Unknown widget kind");
                if (kind != "label" && kind != "image" && !interactive)
                    throw std::runtime_error("Interactive widget requires input");
                if (kind == "checkbox")
                    (void)item.at("value").get_boolean();
                if (kind == "slider")
                {
                    const auto min = integer(item.at("min"), -1000000, 1000000);
                    const auto max = integer(item.at("max"), min, 1000000);
                    integer(item.at("value"), min, max);
                }
                if (kind == "text" &&
                    (item.at("value").get_string().size() > 1024 || item.at("value").get_string().find('\0') != std::string::npos))
                    throw std::runtime_error("Invalid text or text limit");
                if (kind == "list")
                {
                    if (item.at("options").get_array().size() > 64)
                        throw std::runtime_error("List limit");
                    for (const auto& option : item.at("options").get_array())
                        caption(option);
                    integer(item.at("value"), -1, static_cast<int64_t>(item.at("options").get_array().size()) - 1);
                }
            }
            std::set<std::string> font_ids, texture_ids;
            if (auto fonts = value.find("fonts"))
            {
                if (fonts->get_array().size() > 4)
                    throw std::runtime_error("Font limit");
                for (const auto& font : fonts->get_array())
                {
                    if (!valid_id(font.at("id").get_string()) || !font_ids.insert(font.at("id").get_string()).second ||
                        font.at("name").get_string().empty() || font.at("name").get_string().size() > 64 ||
                        font.at("name").get_string().find('\0') != std::string::npos)
                        throw std::runtime_error("Invalid font");
                    integer(font.at("height"), 8, 72);
                    integer(font.at("weight"), 100, 900);
                }
            }
            if (auto textures = value.find("textures"))
            {
                if (textures->get_array().size() > 4)
                    throw std::runtime_error("Texture limit");
                for (const auto& texture : textures->get_array())
                {
                    if (!valid_id(texture.at("id").get_string()) || !texture_ids.insert(texture.at("id").get_string()).second)
                        throw std::runtime_error("Invalid texture ID");
                    const auto width = integer(texture.at("width"), 1, 64), height = integer(texture.at("height"), 1, 64);
                    if (texture.at("rgba").get_array().size() != static_cast<size_t>(width * height * 4))
                        throw std::runtime_error("Invalid pixels");
                    for (const auto& byte : texture.at("rgba").get_array())
                        integer(byte, 0, 255);
                }
            }
            for (const auto& item : value.at("items").get_array())
            {
                if (auto font = item.find("font"); font && !font_ids.count(font->get_string()))
                    throw std::runtime_error("Unknown font");
                if (item.at("kind") == "image" && !texture_ids.count(item.at("texture").get_string()))
                    throw std::runtime_error("Unknown texture");
            }
        }
    } // namespace
    Json extension_ui(Loaded& p, const std::string& operation, const Json& args)
    {
        if (!permitted(&p, NC_PERMISSION_UI_WINDOWS))
            throw std::runtime_error("UI permission required");
        if (operation == "create")
        {
            if (p.windows.get_object().size() >= 8)
                throw std::runtime_error("Window limit");
            validate_window(p, args);
            const auto id = ++next_window;
            p.windows[std::to_string(id)] = args;
            ++p.ui_revision;
            return Json{{"ok", true}, {"handle", id}};
        }
        const auto id = std::to_string(args.at("handle").as<uint64_t>());
        auto* window = p.windows.find(id);
        if (!window)
            throw std::runtime_error("Unknown window handle");
        if (operation == "destroy")
            p.windows.get_object().erase(id);
        else if (operation == "update")
        {
            validate_window(p, args.at("window"));
            *window = args.at("window");
        }
        else if (operation == "show")
            (*window)["visible"] = args.at("visible").get_boolean();
        else
            throw std::runtime_error("Unknown operation");
        ++p.ui_revision;
        return Json{{"ok", true}};
    }
} // namespace plugins::runtime

using namespace plugins;
using namespace plugins::runtime;

const char* nc_runtime_windows()
{
    static std::string result;
    static std::vector<std::pair<uint64_t, uint64_t>> previous;
    std::vector<std::pair<uint64_t, uint64_t>> revision;
    for (const auto& p : loaded)
        if (permitted(p.get(), NC_PERMISSION_UI_WINDOWS))
            revision.emplace_back(p->token, p->ui_revision);
    if (revision == previous && !result.empty())
        return result.c_str();
    Json windows = tao::json::empty_array;
    for (const auto& p : loaded)
        if (permitted(p.get(), NC_PERMISSION_UI_WINDOWS))
            for (const auto& [id, value] : p->windows.get_object())
            {
                auto window = value;
                window["handle"] = id;
                windows.push_back(std::move(window));
            }
    result = tao::json::to_string(windows);
    previous = std::move(revision);
    return result.c_str();
}
void nc_runtime_window_action(const char* raw, const char* action, const char* payload)
{
    try
    {
        const auto id = text(raw, 24), name = text(action, 96);
        auto value = parse(text(payload, 4096));
        for (auto& p : loaded)
            if (permitted(p.get(), NC_PERMISSION_UI_WINDOWS | NC_PERMISSION_UI_INPUT))
                if (auto* window = p->windows.find(id))
                {
                    if (name == "$open" || name == "$close")
                        (*window)["visible"] = name == "$open";
                    else
                    {
                        auto next = *window;
                        bool found{};
                        for (auto& item : next["items"].get_array())
                            if (item.at("id") == name)
                            {
                                item["value"] = value;
                                found = true;
                            }
                        if (!found)
                            return;
                        validate_window(*p, next);
                        *window = std::move(next);
                    }
                    ++p->ui_revision;
                    notify(*p, "sdk.ui", Json{{"handle", std::stoull(id)}, {"id", name}, {"value", value}}, NC_PERMISSION_UI_INPUT);
                    return;
                }
    }
    catch (...)
    {}
}
