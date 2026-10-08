#ifndef CVARTOGGLECHECKBUTTON_H
#define CVARTOGGLECHECKBUTTON_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/CheckButton.h>

namespace vgui2
{
    class Menu;
}

class CCvarToggleCheckButton : public vgui2::CheckButton
{
    DECLARE_CLASS_SIMPLE(CCvarToggleCheckButton, vgui2::CheckButton);

public:
    CCvarToggleCheckButton(vgui2::Panel *parent, const char *panelName, const char *text, char const *cvarname);
    ~CCvarToggleCheckButton(void);

public:
    virtual void SetSelected(bool state);
    virtual void Paint(void);
    virtual void OnMousePressed(vgui2::MouseCode code);

public:
    void Reset(void);
    void ApplyChanges(void);
    bool HasBeenModified(void);
    void ApplySettings(KeyValues *inResourceData);
    void SetDefaultValue(bool value);  // enables the right-click reset item
    bool ResetToDefaultValue();
    const char *get_cvar_name() const { return m_pszCvarName; }

private:
    MESSAGE_FUNC(OnButtonChecked, "CheckButtonChecked");
    MESSAGE_FUNC(OnResetToDefault, "ResetToDefault");

private:
    char *m_pszCvarName;
    bool m_bStartValue;
    bool m_bDefaultValue{};
    bool m_bHasDefaultValue{};
    vgui2::Menu* m_pContextMenu{};
};

#endif