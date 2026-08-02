#pragma once

#include <vgui/KeyCode.h>
#include <vgui_controls/Frame.h>

class MaterialEditInterface;

// A panel docked to the right of the screen with the running game beside it, and the camera
// that game is looked at through.
//
// Nothing here is about materials. Where the panel sits, how much of the frame it leaves the
// world, how the camera is flown while it is open and what a click over the scene means are
// true of any panel that edits something in a live scene; a panel that edits something else
// derives from this and adds only its own controls.
class CEditorPanel : public vgui2::Frame
{
    DECLARE_CLASS_SIMPLE(CEditorPanel, vgui2::Frame);

public:
    CEditorPanel(vgui2::Panel* parent, const char* name);

    void Activate() override;
    void OnClose() override;
    void OnThink() override;

    // Movement keys belong to the flight while it lasts, and to nothing else: they must not
    // reach a field's caret, and they must not fall through to the game's own binds.
    void OnKeyCodePressed(vgui2::KeyCode code) override;
    void OnKeyCodeTyped(vgui2::KeyCode code) override;
    void OnKeyTyped(wchar_t unichar) override;

protected:
    // Something in the scene was clicked, given as the path of the model under the cursor.
    // Empty for a click at nothing, which a panel may still want to hear as "deselect".
    virtual void OnScenePicked(const char* model) {}

    // Puts the panel against the right edge of the screen and tells the renderer how much of
    // the frame it is covering, so the world is drawn beside it rather than under it. Applied
    // on every activation, because the resolution and the ui scale can both have changed
    // since the panel was built.
    void DockToScreen();

    bool IsCursorOverPanel(int x, int y);

    // The engine's editing interface, which is also where the camera and the viewport live.
    static MaterialEditInterface* Editor();

private:
    // Flies the camera while a mouse button is held over the world: the mouse turns it, the
    // movement keys move it, and a press that does neither is a click at what is under it.
    // Polled rather than handled: the panel holds the modal surface, so vgui drops every
    // click that lands outside it and there is nothing to route.
    void StepCamera();
    float StepTime();
    void PickAt(int cursor_x, int cursor_y);
    void ToggleConsole();

    bool m_Flying;

    // When the flight was last stepped, on vgui's clock. Zero while nothing is being flown,
    // so the first step of a flight is not the gap since the last one.
    double m_LastStepTime;

    // Whether the held press has turned or moved anything yet. A press that never does is a
    // click at something rather than a flight.
    bool m_FlyMoved;

    // What the mouse was doing last frame, so a flight can start on the press rather than on
    // finding the button already down.
    bool m_MouseHeld;

    // How far along the build-up to the top speed the flight is, from 0 to 1. Back to zero
    // whenever nothing is pressed, so every flight starts at walking pace.
    float m_FlyRamp;

    // Where the cursor was when the flight began. It is put back there every frame, so the
    // camera can be turned as far as it likes without the pointer walking off the screen.
    int m_LookAnchorX;
    int m_LookAnchorY;
};
