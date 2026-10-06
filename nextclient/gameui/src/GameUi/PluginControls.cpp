#include "PluginControls.h"

#include <string>

#include <nextclient/plugin.h>
#include <tier1/strtools.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/TextEntry.h>

#include "PluginLocalization.h"
#include "PluginWindowState.h"

const char* PluginControls_SettingKind(unsigned kind)
{
    switch (kind)
    {
        case NC_CHECKBOX:
            return "checkbox";
        case NC_SLIDER:
            return "slider";
        case NC_CHOICE:
            return "list";
        default:
            return "button";
    }
}

tao::json::value PluginControls_SettingsSpec(const tao::json::value& spec)
{
    const unsigned kind = spec.at("kind").as<unsigned>();
    tao::json::value result = spec;
    result["kind"] = PluginControls_SettingKind(kind);
    result["text"] = tao::json::value{{"en", spec.at("en")}, {"ru", spec.at("ru")}};
    if (kind == NC_CHECKBOX)
    {
        result["value"] = spec.at("value").as<int>() != 0;
    }
    else if (kind == NC_BUTTON)
    {
        const std::string text = PluginTokenText("#NextPlugins_Run");
        result["text"] = tao::json::value{{"en", text}, {"ru", text}};
    }
    else if (kind == NC_CHOICE)
    {
        const std::string choices = PluginLocalized(spec, "choices_en", "choices_ru");
        result["options"] = tao::json::empty_array;
        size_t start = 0;
        do
        {
            const size_t end = choices.find('\n', start);
            const std::string text = choices.substr(start, end == std::string::npos ? end : end - start);
            result["options"].push_back(tao::json::value{{"en", text}, {"ru", text}});
            if (end == std::string::npos)
            {
                break;
            }
            start = end + 1;
        } while (start <= choices.size());
    }
    return result;
}

vgui2::Panel* PluginControls_Create(vgui2::Panel* parent, const tao::json::value& spec, vgui2::Panel* target, const char* command)
{
    const std::string& kind = spec.at("kind").get_string();
    const char* id = spec.at("id").get_string().c_str();
    vgui2::Panel* widget = nullptr;
    if (kind == "button")
    {
        vgui2::Button* button = new vgui2::Button(parent, id, "", target, command);
        button->SetText(PluginWide(PluginLocalized(spec.at("text"))).c_str());
        widget = button;
    }
    else if (kind == "checkbox")
    {
        vgui2::CheckButton* check = new vgui2::CheckButton(parent, id, "");
        check->SetText(PluginWide(PluginLocalized(spec.at("text"))).c_str());
        widget = check;
    }
    else if (kind == "slider")
    {
        vgui2::Slider* slider = new vgui2::Slider(parent, id);
        slider->SetRange(spec.at("min").as<int>(), spec.at("max").as<int>());
        widget = slider;
    }
    else if (kind == "text")
    {
        vgui2::TextEntry* entry = new vgui2::TextEntry(parent, id);
        entry->SetMaximumCharCount(1024);
        widget = entry;
    }
    else if (kind == "list")
    {
        vgui2::ComboBox* combo = new vgui2::ComboBox(parent, id, 8, false);
        for (const auto& option : spec.at("options").get_array())
        {
            combo->AddItem(PluginWide(PluginLocalized(option)).c_str(), nullptr);
        }
        widget = combo;
    }
    if (widget)
    {
        if (const auto* value = spec.find("value"))
        {
            PluginControls_SetValue(widget, kind, *value);
        }
    }
    return widget;
}

void PluginControls_SetValue(vgui2::Panel* widget, std::string_view kind, const tao::json::value& value)
{
    if (kind == "checkbox")
    {
        static_cast<vgui2::CheckButton*>(widget)->SilentSetSelected(value.is_boolean() ? value.get_boolean() : value.as<int>() != 0);
    }
    else if (kind == "slider")
    {
        static_cast<vgui2::Slider*>(widget)->SetValue(value.as<int>(), false);
    }
    else if (kind == "list")
    {
        static_cast<vgui2::ComboBox*>(widget)->SilentActivateItemByRow(value.as<int>());
    }
    else if (kind == "text")
    {
        static_cast<vgui2::TextEntry*>(widget)->SetText(PluginWide(value.get_string()).c_str());
    }
}

std::optional<tao::json::value> PluginControls_Read(vgui2::Panel* widget, std::string_view kind)
{
    struct Reader
    {
        vgui2::Panel* widget;
        bool Checked() const
        {
            return static_cast<vgui2::CheckButton*>(widget)->IsSelected();
        }
        int SliderValue() const
        {
            return static_cast<vgui2::Slider*>(widget)->GetValue();
        }
        int Selection() const
        {
            return static_cast<vgui2::ComboBox*>(widget)->GetActiveItem();
        }
        std::string Text() const
        {
            vgui2::TextEntry* entry = static_cast<vgui2::TextEntry*>(widget);
            wchar_t text[1025]{};
            entry->GetText(text, sizeof(text));
            char utf8[4097]{};
            V_UnicodeToUTF8(text, utf8, sizeof(utf8));
            const std::string bounded = PluginInputText(utf8);
            if (bounded != utf8)
            {
                entry->SetText(PluginWide(bounded).c_str());
            }
            return bounded;
        }
    } reader{widget};
    return ReadPluginInput(kind, reader);
}
