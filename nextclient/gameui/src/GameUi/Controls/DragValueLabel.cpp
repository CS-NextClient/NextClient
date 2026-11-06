#include "GameUi/Controls/DragValueLabel.h"

#include <vgui/Cursor.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/MouseCode.h>

using namespace vgui2;

namespace
{
    constexpr int kEdge = 2;
}

CDragValueLabel::CDragValueLabel(Panel* parent, const char* name, const char* text, IListener* listener, int index) :
    BaseClass(parent, name, text)
{
    m_pListener = listener;
    m_Index = index;
    m_Dragging = false;
    m_LastX = 0;

    // The cursor is the whole of the discoverability: nothing else on the row says that a
    // caption can be dragged.
    SetCursor(dc_sizewe);
}

void CDragValueLabel::OnMousePressed(MouseCode code)
{
    if (code != MOUSE_LEFT || m_pListener == nullptr)
    {
        BaseClass::OnMousePressed(code);
        return;
    }

    m_Dragging = true;

    int y = 0;
    input()->GetCursorPos(m_LastX, y);

    // Captured, or the drag stops the moment the pointer leaves the caption, which for a
    // caption a hundred pixels wide is immediately.
    input()->SetMouseCapture(GetVPanel());

    m_pListener->OnDragValueBegin(m_Index);
}

void CDragValueLabel::OnMouseReleased(MouseCode code)
{
    if (!m_Dragging)
    {
        BaseClass::OnMouseReleased(code);
        return;
    }

    if (input()->GetMouseCapture() == GetVPanel())
    {
        input()->SetMouseCapture(0);
    }

    EndDrag();
}

void CDragValueLabel::OnCursorMoved(int x, int y)
{
    if (!m_Dragging)
    {
        BaseClass::OnCursorMoved(x, y);
        return;
    }

    // Screen coordinates rather than the ones the message carries: with the mouse captured
    // those are relative to this panel, and a panel that is being dragged over is not a
    // stable frame of reference.
    int cursor_x = 0;
    int cursor_y = 0;
    input()->GetCursorPos(cursor_x, cursor_y);

    int delta = cursor_x - m_LastX;
    m_LastX = cursor_x;

    // Round the screen rather than stop at it: the pointer cannot leave, so a drag that needs
    // more room than the screen has would end against the edge with the value half set.
    int screen_wide = 0;
    int screen_tall = 0;
    surface()->GetScreenSize(screen_wide, screen_tall);

    if (cursor_x <= kEdge || cursor_x >= screen_wide - kEdge - 1)
    {
        m_LastX = cursor_x <= kEdge ? screen_wide - kEdge - 2 : kEdge + 1;
        input()->SetCursorPos(m_LastX, cursor_y);
    }

    if (delta == 0)
    {
        return;
    }

    bool fine = input()->IsKeyDown(KEY_LSHIFT) || input()->IsKeyDown(KEY_RSHIFT);
    bool coarse = input()->IsKeyDown(KEY_LCONTROL) || input()->IsKeyDown(KEY_RCONTROL);

    m_pListener->OnDragValue(m_Index, delta, fine, coarse);
}

void CDragValueLabel::OnMouseCaptureLost()
{
    EndDrag();
    BaseClass::OnMouseCaptureLost();
}

void CDragValueLabel::EndDrag()
{
    if (!m_Dragging)
    {
        return;
    }

    m_Dragging = false;
    m_pListener->OnDragValueEnd(m_Index);
}
