#include "EditorPanel.h"

#include "../BasePanel.h"
#include "../GameConsole.h"
#include "../GameUi.h"

#include <IEngineVGui.h>

#include <algorithm>

#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/ISystem.h>
#include <vgui/MouseCode.h>

#include <next_engine_mini/MaterialEditInterface.h>

using namespace vgui2;

namespace
{
    // Docked against the right edge over the full height, which is what lets the world be
    // drawn beside the panel instead of under it. Never wider than a third of the screen -
    // what is left has to stay a view of something - and never narrower than a row of
    // controls needs, even if that overhangs on a screen nothing was designed for.
    constexpr int kDockWide = 460;
    constexpr int kDockMinimumWide = 360;
    constexpr int kDockScreenFraction = 3;

    // The camera builds up speed while it is being flown and drops back to walking pace the
    // moment it is not. A player walks at 250: that is the right speed for placing yourself
    // in front of one weapon, and much too slow for crossing a map, and which of the two is
    // wanted is told by how long the key has been held rather than by asking.
    constexpr float kFlySpeed = 90.0f;
    constexpr float kFlyTopSpeed = 500.0f;

    // Seconds of held movement to reach the top speed.
    constexpr float kFlyRampSeconds = 2.5f;

    // What the modifier is worth: it starts the ramp partway along rather than multiplying
    // the end of it, so shift is a quicker start and not a different top speed.
    constexpr float kFlyFastHeadStart = 0.45f;

    // A frame longer than this is a level load or a stall, not a frame, and integrating it
    // would throw the camera across the map.
    constexpr float kMaxFrameTime = 0.1f;

}

CEditorPanel::CEditorPanel(Panel* parent, const char* name) :
    BaseClass(parent, name)
{
    m_Flying = false;
    m_FlyMoved = false;
    m_FlyRamp = 0.0f;
    m_MouseHeld = false;
    m_LastStepTime = 0.0;
    m_LookAnchorX = 0;
    m_LookAnchorY = 0;

    // Docked: it is not moved and not resized, because where it is is what the world is
    // drawn around.
    SetSizeable(false);
    SetMoveable(false);
}

MaterialEditInterface* CEditorPanel::Editor()
{
    return EngineMini() != nullptr ? EngineMini()->GetMaterialEdit() : nullptr;
}

void CEditorPanel::Activate()
{
    BaseClass::Activate();

    input()->SetAppModalSurface(GetVPanel());
    DockToScreen();

    if (Editor() != nullptr)
    {
        Editor()->SetPanelOpen(true);
    }

    // Asked for rather than set: where the author is looking is the finished view, and the
    // client is the side that has it.
    if (Editor() != nullptr)
    {
        MaterialCamera camera;
        Editor()->GetCamera(&camera);

        camera.active = true;
        camera.seed_requested = true;

        Editor()->SetCamera(&camera);
    }
}

// The console is a panel of the game ui, and the game ui is hidden while anything is shown
// over the running game - which is why asking the engine to toggle it did nothing visible.
// Moved out to the same root this panel is on, it is a window like any other; put back when
// this panel closes, or the menu would lose its console.
void CEditorPanel::ToggleConsole()
{
    if (GameConsole().IsConsoleVisible())
    {
        GameConsole().Hide();
        return;
    }

    GameConsole().SetParent(g_pEngineVGui->GetPanel(PANEL_ROOT));

    // Nothing reaches a panel that is not the modal surface, and the console is the one
    // window an author has to be able to type into while this is open.
    input()->ReleaseAppModalSurface();

    GameConsole().Activate();
}

void CEditorPanel::OnClose()
{
    if (BasePanel() != nullptr)
    {
        GameConsole().SetParent(BasePanel()->GetVPanel());
    }

    // The camera and the strip go with the panel: leaving either behind would leave the
    // author looking through a camera nothing can move, at a third of a screen nothing will
    // give back.
    if (Editor() != nullptr)
    {
        MaterialCamera camera;
        Editor()->GetCamera(&camera);
        camera.active = false;
        Editor()->SetCamera(&camera);

        Editor()->SetViewportInset(0, 0);
        Editor()->SetPanelOpen(false);
    }

    BaseClass::OnClose();
    input()->ReleaseAppModalSurface();

    GameUI().EndPanelOverGame();
}

void CEditorPanel::OnThink()
{
    BaseClass::OnThink();

    // Taken back once the console is gone. Only when nobody holds it: a window of this
    // panel's own - the colour picker - holds it while it is up, and stealing it from there
    // would leave that window unable to be clicked.
    if (input()->GetAppModalSurface() == 0 && !GameConsole().IsConsoleVisible())
    {
        input()->SetAppModalSurface(GetVPanel());
    }

    StepCamera();
}

namespace
{
    bool IsFlyKey(vgui2::KeyCode code)
    {
        switch (code)
        {
        case KEY_W:
        case KEY_A:
        case KEY_S:
        case KEY_D:
        case KEY_SPACE:
        case KEY_LCONTROL:
        case KEY_LSHIFT:
            return true;

        default:
            return false;
        }
    }
}

void CEditorPanel::OnKeyCodePressed(KeyCode code)
{
    if (m_Flying && IsFlyKey(code))
    {
        return;
    }

    BaseClass::OnKeyCodePressed(code);
}

void CEditorPanel::OnKeyCodeTyped(KeyCode code)
{
    if (m_Flying && IsFlyKey(code))
    {
        return;
    }

    // The console is a panel like this one, and this one holds the modal surface, so nothing
    // reaches the console until that is handed back. Handed back here rather than never:
    // an editor that cannot be typed a command into is an editor with half the game missing.
    if (code == KEY_BACKQUOTE)
    {
        ToggleConsole();
        return;
    }

    BaseClass::OnKeyCodeTyped(code);
}

void CEditorPanel::OnKeyTyped(wchar_t unichar)
{
    // Every character, not just the flight's own: while the camera is being flown there is
    // no caret anywhere that a keystroke was meant for.
    if (m_Flying)
    {
        return;
    }

    BaseClass::OnKeyTyped(unichar);
}

// How long since this was last asked, in seconds.
//
// Measured here rather than taken from vgui, whose GetFrameTime is not a frame time at all:
// it returns the current time, so clamping it to a sane step handed back the clamp itself
// every frame and the camera flew at that times the frame rate.
float CEditorPanel::StepTime()
{
    double now = system()->GetCurrentTime();
    double elapsed = m_LastStepTime > 0.0 ? now - m_LastStepTime : 0.0;

    m_LastStepTime = now;

    return std::clamp(static_cast<float>(elapsed), 0.0f, kMaxFrameTime);
}

void CEditorPanel::PickAt(int cursor_x, int cursor_y)
{
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr)
    {
        return;
    }

    int screen_wide = 0;
    int screen_tall = 0;
    surface()->GetScreenSize(screen_wide, screen_tall);

    int panel_wide = 0;
    int panel_tall = 0;
    GetSize(panel_wide, panel_tall);

    // As a fraction of the part of the screen the world is drawn into, which is the screen
    // less what this panel covers. The renderer is given no pixels because the pixels it
    // draws into are not the ones vgui counts in.
    int view_wide = screen_wide - panel_wide;

    if (view_wide <= 0 || screen_tall <= 0)
    {
        return;
    }

    OnScenePicked(editor->PickModel(
        static_cast<float>(cursor_x) / static_cast<float>(view_wide),
        static_cast<float>(cursor_y) / static_cast<float>(screen_tall)
    ));
}

void CEditorPanel::StepCamera()
{
    MaterialEditInterface* editor = Editor();

    // Not while another window is taking input. A window of this panel's own - the colour
    // picker - is not inside the panel's rectangle, so testing that rectangle alone counts a
    // drag on the shade square as a drag on the world: the camera turns and the cursor is
    // pulled back to the anchor under the author's hand. Asking who holds the modal surface
    // answers it for every such window, including ones added later.
    //
    // Nobody holding it counts as this panel: that is the state left behind after the
    // console was let through, and the camera has to keep flying afterwards.
    vgui2::VPANEL modal = input()->GetAppModalSurface();

    if (editor == nullptr || (modal != 0 && modal != GetVPanel()))
    {
        m_Flying = false;
        return;
    }

    // Either button: there is nothing else a click over the world could mean, and insisting
    // on one of the two is a rule an author would have to be told.
    bool held = input()->IsMouseDown(MOUSE_LEFT) || input()->IsMouseDown(MOUSE_RIGHT);

    // Only the frame the button goes down starts anything. Held is not pressed: a drag that
    // began on a slider or a caption inside the panel is still held when it wanders over the
    // scene, and treating that as a press flew the camera away under the author's hand.
    bool pressed = held && !m_MouseHeld;
    m_MouseHeld = held;

    // And a control that took the mouse keeps it. Capture is how vgui says a gesture belongs
    // to someone, and it says so for every control, including ones added later.
    if (input()->GetMouseCapture() != 0)
    {
        pressed = false;
    }

    int cursor_x = 0;
    int cursor_y = 0;
    input()->GetCursorPos(cursor_x, cursor_y);

    if (!m_Flying)
    {
        // Only a press that begins over the world starts a flight, so releasing a slider
        // whose drag wandered off the panel does not swing the camera.
        if (pressed && !IsCursorOverPanel(cursor_x, cursor_y))
        {
            m_Flying = true;
            m_FlyMoved = false;
            m_FlyRamp = 0.0f;
            m_LastStepTime = 0.0;
            m_LookAnchorX = cursor_x;
            m_LookAnchorY = cursor_y;

            // Taken off whatever had it, which is the other half of not typing into a field
            // while flying: the swallowing below only sees what is routed here, and what is
            // routed to a text entry never reaches it.
            RequestFocus();
        }

        return;
    }

    if (!held)
    {
        // A press that neither turned nor moved anything was not a flight, it was a click at
        // something: the same press does both, told apart by whether it went anywhere.
        if (!m_FlyMoved)
        {
            PickAt(cursor_x, cursor_y);
        }

        m_Flying = false;
        m_FlyRamp = 0.0f;
        return;
    }

    MaterialCamera camera;
    editor->GetCamera(&camera);

    // Nothing to fly yet: the camera is still waiting to be told where the author is, and
    // moving now would fly away from the middle of the map.
    if (!camera.active || camera.seed_requested)
    {
        return;
    }

    int dx = cursor_x - m_LookAnchorX;
    int dy = cursor_y - m_LookAnchorY;

    if (dx != 0 || dy != 0)
    {
        // The game's own sensitivity, so this turns at the speed the author is used to and
        // follows their inverted look if they have one.
        float sensitivity = engine->pfnGetCvarFloat("sensitivity");
        float yaw_scale = engine->pfnGetCvarFloat("m_yaw");
        float pitch_scale = engine->pfnGetCvarFloat("m_pitch");

        camera.angles[1] -= dx * yaw_scale * sensitivity;
        camera.angles[0] = std::clamp(camera.angles[0] + dy * pitch_scale * sensitivity, -89.0f, 89.0f);

        m_FlyMoved = true;
        input()->SetCursorPos(m_LookAnchorX, m_LookAnchorY);
    }

    // The keyboard belongs to the panel the rest of the time, so this is part of the same
    // held gesture: a W meant for a texture path must not be a step forward.
    struct FlyKey
    {
        vgui2::KeyCode key;
        int axis;
        float sign;
    };

    // Axis 0 is where the camera looks, 1 is to its right, 2 is straight up - up rather than
    // the camera's own up, because an author nosed down at a weapon still means "higher".
    static const FlyKey kKeys[6] = {
        {KEY_W, 0, 1.0f},
        {KEY_S, 0, -1.0f},
        {KEY_D, 1, 1.0f},
        {KEY_A, 1, -1.0f},
        {KEY_SPACE, 2, 1.0f},
        {KEY_LCONTROL, 2, -1.0f},
    };

    float wanted[3] = {0.0f, 0.0f, 0.0f};

    for (const FlyKey& key : kKeys)
    {
        if (input()->IsKeyDown(key.key))
        {
            wanted[key.axis] += key.sign;
        }
    }

    if (wanted[0] == 0.0f && wanted[1] == 0.0f && wanted[2] == 0.0f)
    {
        m_FlyRamp = 0.0f;

        editor->SetCamera(&camera);
        return;
    }

    m_FlyMoved = true;

    float forward[3];
    float right[3];
    float up[3];
    engine->pfnAngleVectors(camera.angles, forward, right, up);

    float frametime = StepTime();

    if (input()->IsKeyDown(KEY_LSHIFT))
    {
        m_FlyRamp = std::max(m_FlyRamp, kFlyFastHeadStart);
    }

    m_FlyRamp = std::min(m_FlyRamp + frametime / kFlyRampSeconds, 1.0f);

    // Squared, so the first moment of a press is still a nudge and the build-up is felt
    // rather than jumped through.
    float step = (kFlySpeed + (kFlyTopSpeed - kFlySpeed) * m_FlyRamp * m_FlyRamp) * frametime;

    for (int i = 0; i < 3; i++)
    {
        camera.origin[i] += (forward[i] * wanted[0] + right[i] * wanted[1]) * step;
    }

    camera.origin[2] += wanted[2] * step;

    editor->SetCamera(&camera);
}

bool CEditorPanel::IsCursorOverPanel(int x, int y)
{
    int left = 0;
    int top = 0;
    LocalToScreen(left, top);

    int wide = 0;
    int tall = 0;
    GetSize(wide, tall);

    return x >= left && y >= top && x < left + wide && y < top + tall;
}

void CEditorPanel::DockToScreen()
{
    int screen_wide = 0;
    int screen_tall = 0;
    surface()->GetScreenSize(screen_wide, screen_tall);

    int wide = std::max(std::min(kDockWide, screen_wide / kDockScreenFraction),
                        std::min(kDockMinimumWide, screen_wide));

    SetBounds(screen_wide - wide, 0, wide, screen_tall);

    // In the panel's own coordinates, not pixels: the renderer scales it against the frame
    // it is about to draw, which is the only side that knows what that frame is.
    if (Editor() != nullptr)
    {
        Editor()->SetViewportInset(wide, screen_wide);
    }
}
