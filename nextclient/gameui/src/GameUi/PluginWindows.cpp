#include "PluginWindows.h"
#include "PluginLocalization.h"
#include "BasePanel.h"
#include <nextclient/runtime.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/IScheme.h>
#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/PanelListPanel.h>
#include <algorithm>
#include <map>
#include <memory>
#include <set>

namespace
{
    using Json = tao::json::value;
    std::map<std::string, vgui2::HFont> fontCache;
    class PluginRow : public vgui2::Panel
    {
    public:
        PluginRow(vgui2::Panel* parent, std::vector<vgui2::Panel*> children) :
            Panel(parent, "PluginRow"),
            cells_(std::move(children))
        {
            SetTall(32);
            SetPaintBackgroundEnabled(false);
            for (auto* cell : cells_)
                cell->SetParent(this);
        }
        void PerformLayout() override
        {
            const auto wide = GetWide() / static_cast<int>(cells_.size());
            for (size_t i = 0; i < cells_.size(); ++i)
                cells_[i]->SetBounds(static_cast<int>(i) * wide, 0, std::max(1, wide - 4), GetTall() - 4);
        }

    private:
        std::vector<vgui2::Panel*> cells_;
    };
    class PluginImage : public vgui2::Panel
    {
    public:
        PluginImage(vgui2::Panel* parent, const Json& spec) :
            Panel(parent, "PluginImage")
        {
            std::vector<unsigned char> pixels;
            for (const auto& pixel : spec.at("rgba").get_array())
                pixels.push_back(pixel.as<unsigned char>());
            texture_ = vgui2::surface()->CreateNewTextureID(true);
            vgui2::surface()->DrawSetTextureRGBA(texture_, pixels.data(), spec.at("width").as<int>(), spec.at("height").as<int>(), 1, true);
        }
        ~PluginImage() override
        {
            vgui2::surface()->DeleteTextureByID(texture_);
        }
        void Paint() override
        {
            vgui2::surface()->DrawSetColor(255, 255, 255, 255);
            vgui2::surface()->DrawSetTexture(texture_);
            vgui2::surface()->DrawTexturedRect(0, 0, GetWide(), GetTall());
        }

    private:
        int texture_{};
    };
    class PluginWindow : public vgui2::Frame
    {
    public:
        PluginWindow(vgui2::VPANEL root, const Json& spec) :
            Frame(nullptr, "PluginWindow"),
            spec_(spec)
        {
            SetParent(root);
            SetDeleteSelfOnClose(false);
            DisableFadeEffect();
            SetMinimumSize(160, 100);
            SetTitle(PluginWide(PluginLocalized(spec.at("title"))).c_str(), true);
            SetSize(spec.at("width").as<int>(), spec.at("height").as<int>());
            const bool interactive = spec.at("interactive").get_boolean();
            SetTitleBarVisible(interactive);
            SetMouseInputEnabled(interactive);
            SetKeyBoardInputEnabled(interactive);
            SetSizeable(interactive);
            SetMoveable(interactive);
            vgui2::surface()->CreatePopup(GetVPanel(), false, false, false, false, false);
            SetVisible(false);
            list_ = new vgui2::PanelListPanel(this, "Contents");
            list_->SetFirstColumnWidth(0);
            std::map<std::string, vgui2::HFont> fonts;
            if (auto specs = spec.find("fonts"))
                for (const auto& font : specs->get_array())
                {
                    const auto key = tao::json::to_string(font);
                    auto found = fontCache.find(key);
                    if (found == fontCache.end() && fontCache.size() < 128)
                    {
                        auto handle = vgui2::surface()->CreateFont();
                        vgui2::surface()->AddGlyphSetToFont(
                            handle,
                            font.at("name").get_string().c_str(),
                            font.at("height").as<int>(),
                            font.at("weight").as<int>(),
                            0,
                            0,
                            vgui2::ISurface::FONTFLAG_ANTIALIAS,
                            0,
                            0
                        );
                        found = fontCache.emplace(key, handle).first;
                    }
                    if (found != fontCache.end())
                        fonts[font.at("id").get_string()] = found->second;
                }
            for (const auto& item : spec.at("items").get_array())
            {
                const auto kind = item.at("kind").get_string(), id = item.at("id").get_string();
                const auto title = PluginWide(PluginLocalized(item.at("text")));
                vgui2::Panel* widget{};
                if (kind == "button")
                {
                    auto* button = new vgui2::Button(this, id.c_str(), "", this, id.c_str());
                    button->SetText(title.c_str());
                    widget = button;
                }
                else if (kind == "checkbox")
                {
                    auto* check = new vgui2::CheckButton(this, id.c_str(), "");
                    check->SetText(title.c_str());
                    check->SetSelected(item.at("value").get_boolean());
                    widget = check;
                }
                else if (kind == "slider")
                {
                    auto* slider = new vgui2::Slider(this, id.c_str());
                    slider->SetRange(item.at("min").as<int>(), item.at("max").as<int>());
                    slider->SetValue(item.at("value").as<int>());
                    widget = slider;
                }
                else if (kind == "text")
                {
                    auto* entry = new vgui2::TextEntry(this, id.c_str());
                    entry->SetMaximumCharCount(1024);
                    entry->SetText(PluginWide(item.at("value").get_string()).c_str());
                    widget = entry;
                }
                else if (kind == "list")
                {
                    auto* list = new vgui2::ComboBox(this, id.c_str(), 8, false);
                    for (const auto& option : item.at("options").get_array())
                        list->AddItem(PluginWide(PluginLocalized(option)).c_str(), nullptr);
                    list->ActivateItem(item.at("value").as<int>());
                    widget = list;
                }
                else if (kind == "image" && spec.find("textures") && item.find("texture"))
                {
                    for (const auto& texture : spec.at("textures").get_array())
                        if (texture.at("id") == item.at("texture"))
                            widget = new PluginImage(this, texture);
                }
                if (!widget)
                    widget = new vgui2::Label(this, id.c_str(), title.c_str());
                if (auto font = item.find("font"))
                    if (auto found = fonts.find(font->get_string()); found != fonts.end())
                        if (auto* label = dynamic_cast<vgui2::Label*>(widget))
                            label->SetFont(found->second);
                widgets_.push_back(widget);
            }
            std::map<int, std::vector<vgui2::Panel*>> rows;
            for (size_t i = 0; i < widgets_.size(); ++i)
            {
                const auto& item = spec_.at("items").at(i);
                rows[item.find("row") ? item.at("row").as<int>() : static_cast<int>(i)].push_back(widgets_[i]);
            }
            for (auto& [id, cells] : rows)
                list_->AddItem(nullptr, new PluginRow(list_, std::move(cells)));
            MoveToCenterOfScreen();
        }
        ~PluginWindow() override
        {
            ReleaseInput();
        }
        void ReleaseInput()
        {
            const auto capture = vgui2::input()->GetMouseCapture();
            if (capture && (capture == GetVPanel() || vgui2::ipanel()->HasParent(capture, GetVPanel())))
                vgui2::input()->SetMouseCapture(0);
        }
        void OnCommand(const char* command) override
        {
            for (const auto& item : spec_.at("items").get_array())
                if (item.at("id") == command && item.at("kind") == "button")
                {
                    nc_runtime_window_action(spec_.at("handle").get_string().c_str(), command, "null");
                    return;
                }
            Frame::OnCommand(command);
        }
        void OnClose() override
        {
            nc_runtime_window_action(spec_.at("handle").get_string().c_str(), "$close", "null");
            ReleaseInput();
            Frame::OnClose();
            SetMouseInputEnabled(false);
            SetKeyBoardInputEnabled(false);
        }
        void OnKeyCodeTyped(vgui2::KeyCode code) override
        {
            if (code == vgui2::KEY_ESCAPE)
                OnClose();
            else
                Frame::OnKeyCodeTyped(code);
        }
        void PerformLayout() override
        {
            Frame::PerformLayout();
            int width, height;
            vgui2::surface()->GetScreenSize(width, height);
            SetSize(std::min(GetWide(), width), std::min(GetTall(), height));
            if (list_)
                list_->SetBounds(12, 32, GetWide() - 24, GetTall() - 44);
        }
        void Poll()
        {
            if (!IsVisible())
                return;
            for (size_t i = 0; i < widgets_.size(); ++i)
            {
                auto& item = spec_["items"].at(i);
                Json value;
                if (auto* check = dynamic_cast<vgui2::CheckButton*>(widgets_[i]))
                    value = check->IsSelected();
                else if (auto* slider = dynamic_cast<vgui2::Slider*>(widgets_[i]))
                    value = slider->GetValue();
                else if (auto* entry = dynamic_cast<vgui2::TextEntry*>(widgets_[i]))
                {
                    wchar_t text[1025]{};
                    entry->GetText(text, sizeof(text));
                    char utf8[4097]{};
                    WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, sizeof(utf8), nullptr, nullptr);
                    value = std::string(utf8);
                }
                else if (auto* list = dynamic_cast<vgui2::ComboBox*>(widgets_[i]))
                    value = list->GetActiveItem();
                else
                    continue;
                if (value != item.at("value"))
                {
                    nc_runtime_window_action(
                        spec_.at("handle").get_string().c_str(), item.at("id").get_string().c_str(), tao::json::to_string(value).c_str()
                    );
                    item["value"] = value;
                }
            }
        }
        Json spec_;

    private:
        std::vector<vgui2::Panel*> widgets_;
        vgui2::PanelListPanel* list_{};
    };
    std::map<std::string, std::unique_ptr<PluginWindow>> windows;
    std::string snapshot, language;
    Json specifications = tao::json::empty_array;
} // namespace
void PluginWindowsFrame(vgui2::VPANEL root, bool menuVisible)
{
    try
    {
        const std::string next = nc_runtime_windows();
        const auto currentLanguage = PluginLanguage();
        if (next != snapshot)
        {
            specifications = tao::json::from_string(next);
            snapshot = next;
        }
        if (currentLanguage != language)
        {
            windows.clear();
            language = currentLanguage;
        }
        const auto& specs = specifications;
        std::set<std::string> current;
        for (const auto& spec : specs.get_array())
        {
            auto id = spec.at("handle").get_string();
            current.insert(id);
            auto& window = windows[id];
            if (window)
            {
                auto visible = window->spec_;
                visible["visible"] = spec.at("visible");
                if (visible != spec)
                    window.reset();
                else
                    window->spec_ = spec;
            }
            if (!window)
                window = std::make_unique<PluginWindow>(root, spec);
            const auto surface = spec.at("surface").get_string();
            const bool visible = spec.at("visible").get_boolean() && (surface == "all" || (surface == "menu" ? menuVisible : !menuVisible));
            const bool input = visible && spec.at("interactive").get_boolean();
            const bool opening = visible && !window->IsVisible();
            if (!visible)
                window->ReleaseInput();
            window->SetVisible(visible);
            window->SetMouseInputEnabled(input);
            window->SetKeyBoardInputEnabled(input);
            if (opening && input)
            {
                window->MoveToFront();
                window->RequestFocus();
            }
            window->Poll();
        }
        for (auto it = windows.begin(); it != windows.end();)
            if (!current.count(it->first))
                it = windows.erase(it);
            else
                ++it;
        BasePanel()->UpdatePluginMenus(specs);
    }
    catch (...)
    {}
}
void PluginWindowsShutdown()
{
    windows.clear();
    snapshot.clear();
    specifications = tao::json::empty_array;
}
