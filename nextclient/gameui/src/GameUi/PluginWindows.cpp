#include "PluginWindows.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>

#include <nextclient/runtime.h>
#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PanelListPanel.h>

#include "BasePanel.h"
#include "PluginControls.h"
#include "PluginLocalization.h"
#include "PluginWindowState.h"

#undef CreateFont

namespace
{
    using Json = tao::json::value;
    std::map<std::string, vgui2::HFont> g_FontCache;
    class PluginRow : public vgui2::Panel
    {
    public:
        PluginRow(vgui2::Panel* parent, std::vector<vgui2::Panel*> children) :
            Panel(parent, "PluginRow"),
            cells_(std::move(children))
        {
            int height = 24;
            for (auto* cell : cells_)
                height = std::max(height, cell->GetTall());
            SetTall(PluginRowHeight(height));
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
                    const auto key = PluginFontKey(font);
                    auto found = g_FontCache.find(key);
                    if (found == g_FontCache.end() && g_FontCache.size() < 128)
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
                        found = g_FontCache.emplace(key, handle).first;
                    }
                    if (found != g_FontCache.end())
                    {
                        fonts[font.at("id").get_string()] = found->second;
                    }
                }
            for (const auto& item : spec.at("items").get_array())
            {
                const auto kind = item.at("kind").get_string(), id = item.at("id").get_string();
                const auto title = PluginWide(PluginLocalized(item.at("text")));
                vgui2::Panel* widget = PluginControls_Create(this, item, this, id.c_str());
                if (kind == "image" && spec.find("textures") && item.find("texture"))
                {
                    for (const auto& texture : spec.at("textures").get_array())
                        if (texture.at("id") == item.at("texture"))
                            widget = new PluginImage(this, texture);
                }
                if (!widget)
                    widget = new vgui2::Label(this, id.c_str(), title.c_str());
                int content_height = 24;
                if (kind == "image")
                    for (const auto& texture : spec.at("textures").get_array())
                        if (texture.at("id") == item.at("texture"))
                            content_height = texture.at("height").as<int>();
                if (auto font = item.find("font"))
                    if (auto found = fonts.find(font->get_string()); found != fonts.end())
                        if (auto* label = dynamic_cast<vgui2::Label*>(widget))
                        {
                            label->SetFont(found->second);
                            content_height = std::max(content_height, vgui2::surface()->GetFontTall(found->second));
                        }
                widget->SetTall(content_height);
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
                auto value = PluginControls_Read(widgets_[i], item.at("kind").get_string());
                if (!value)
                    continue;
                if (*value != item.at("value"))
                {
                    if (nc_runtime_window_action(
                            spec_.at("handle").get_string().c_str(),
                            item.at("id").get_string().c_str(),
                            tao::json::to_string(*value).c_str()
                        ))
                        item["value"] = std::move(*value);
                }
            }
        }
        Json spec_;

    private:
        std::vector<vgui2::Panel*> widgets_;
        vgui2::PanelListPanel* list_{};
    };
    std::map<std::string, std::unique_ptr<PluginWindow>> g_Windows;
    std::string g_Snapshot, g_Language;
    Json g_Specifications = tao::json::empty_array;
} // namespace
void PluginWindowsFrame(vgui2::VPANEL root, bool menu_visible)
{
    try
    {
        const std::string_view next = nc_runtime_windows();
        const auto current_language = PluginLanguage();
        const bool changed = next != g_Snapshot;
        const bool translated = current_language != g_Language;
        if (changed)
        {
            g_Specifications = tao::json::from_string(next);
            g_Snapshot = next;
        }
        if (translated)
        {
            g_Windows.clear();
            g_Language = current_language;
        }
        const auto& specs = g_Specifications;
        std::set<std::string> current;
        for (const auto& spec : specs.get_array())
        {
            auto id = spec.at("handle").get_string();
            current.insert(id);
            auto& window = g_Windows[id];
            if (window && changed)
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
            const bool visible =
                spec.at("visible").get_boolean() && (surface == "all" || (surface == "menu" ? menu_visible : !menu_visible));
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
        for (auto it = g_Windows.begin(); it != g_Windows.end();)
        {
            if (!current.count(it->first))
            {
                it = g_Windows.erase(it);
            }
            else
            {
                ++it;
            }
        }
        if (changed || translated)
            BasePanel()->UpdatePluginMenus(specs);
    }
    catch (...)
    {}
}
void PluginWindowsShutdown()
{
    g_Windows.clear();
    g_Snapshot.clear();
    g_Specifications = tao::json::empty_array;
}
