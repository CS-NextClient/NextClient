#pragma once

#include <cstdint>

#include <vgui/MouseCode.h>

#define DECLARE_CLASS_SIMPLE(class_name, base_class) using BaseClass = base_class

class KeyValues;

namespace vgui2
{
    using VPANEL = uintptr_t;

    class Panel
    {
    private:
        Panel* parent_{};

    public:
        Panel(Panel* parent = nullptr, const char* = "") :
            parent_(parent)
        {}
        virtual ~Panel() = default;
        virtual void Paint() {}
        virtual void PerformLayout() {}
        virtual void OnMouseWheeled(int) {}
        virtual void OnKillFocus() {}
        void SetPaintBackgroundEnabled(bool) {}
        void SetMouseInputEnabled(bool) {}
        void SetKeyBoardInputEnabled(bool) {}
        void RequestFocus() {}
        void PostActionSignal(::KeyValues*) {}
        Panel* GetParent() const
        {
            return parent_;
        }
        void ScreenToLocal(int&, int&) {}
        void LocalToScreen(int&, int&) {}
        int GetTall() const
        {
            return 128;
        }
        int GetWide() const
        {
            return 128;
        }
        void SetBounds(int, int, int, int) {}

        virtual void OnMousePressed(MouseCode) {}
        virtual void OnMouseReleased(MouseCode) {}
        virtual void OnCursorMoved(int, int) {}
        virtual void OnMouseCaptureLost() {}

        VPANEL GetVPanel() const
        {
            return reinterpret_cast<VPANEL>(this);
        }
        void SetCursor(int) {}
    };

    class Label : public Panel
    {
    public:
        Label(Panel* parent, const char* name, const char*) :
            Panel(parent, name)
        {}
        void SetWrap(bool) {}
        void SetText(const char*) {}
    };
} // namespace vgui2
