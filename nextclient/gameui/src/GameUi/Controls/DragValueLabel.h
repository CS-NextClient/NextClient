#pragma once

#include <vgui_controls/Label.h>

// A caption that also changes the value beside it when dragged sideways, the way a number
// field works in a 3D editor's inspector. Typing stays available in the field itself; this
// is for the case an author is actually in, which is looking at the model and wanting the
// number a little higher.
class CDragValueLabel : public vgui2::Label
{
    DECLARE_CLASS_SIMPLE(CDragValueLabel, vgui2::Label);

public:
    class IListener
    {
    public:
        virtual ~IListener() = default;

        // Horizontal movement since the last call, in pixels, and the modifier state that
        // was in force. Which value the caption belongs to is the caller's own index.
        virtual void OnDragValue(int index, int pixels, bool fine, bool coarse) = 0;

        virtual void OnDragValueBegin(int index) { }
        virtual void OnDragValueEnd(int index) { }
    };

    CDragValueLabel(vgui2::Panel* parent, const char* name, const char* text, IListener* listener, int index);

    void OnMousePressed(vgui2::MouseCode code) override;
    void OnMouseReleased(vgui2::MouseCode code) override;
    void OnCursorMoved(int x, int y) override;

private:
    IListener* m_pListener;
    int m_Index;
    bool m_Dragging;
    int m_LastX;
};
