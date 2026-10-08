#ifndef CVARSLIDER_H
#define CVARSLIDER_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Slider.h>

namespace vgui2
{
    class Menu;
}

class CCvarSlider : public vgui2::Slider
{
    DECLARE_CLASS_SIMPLE(CCvarSlider, vgui2::Slider);

public:
    CCvarSlider(vgui2::Panel *parent, const char *panelName);
    CCvarSlider(vgui2::Panel *parent, const char *panelName, char const *caption, float minValue, float maxValue, char const *cvarname, bool bAllowOutOfRange = false);
    ~CCvarSlider(void);

public:
    void SetupSlider(float minValue, float maxValue, const char *cvarname, bool bAllowOutOfRange);
    void SetCVarName(char const *cvarname);
    void SetMinMaxValues(float minValue, float maxValue, bool bSetTickdisplay = true);
    void SetScale(float scale, int print_precision);
    void SetDefaultValue(float value);  // enables the right-click reset item
    bool ResetToDefaultValue();
    const char *get_cvar_name() const { return m_szCvarName; }
    void SetTickColor(Color color);

public:
    virtual void Paint(void);
    virtual void ApplySettings(KeyValues *inResourceData);
    virtual void GetSettings(KeyValues *outResourceData);
    virtual void OnMousePressed(vgui2::MouseCode code);

public:
    void ApplyChanges(void);
    float GetSliderValue(void);
    void SetSliderValue(float fValue);
    void Reset(void);
    bool HasBeenModified(void);

private:
    MESSAGE_FUNC(OnSliderMoved, "SliderMoved");
    MESSAGE_FUNC(OnApplyChanges, "ApplyChanges");
    MESSAGE_FUNC(OnResetToDefault, "ResetToDefault");

private:
    bool m_bAllowOutOfRange;
    bool m_bModifiedOnce;
    float m_fStartValue;
    int m_iStartValue;
    int m_iLastSliderValue;
    float m_fCurrentValue;
    char m_szCvarName[64];
    bool m_bCreatedInCode;
    float m_flMinValue;
    float m_flMaxValue;
    float m_flScale;        // slider int positions per cvar unit
    int m_iPrintPrecision;  // decimals written to the cvar and shown in tick captions
    float m_flDefaultValue;
    bool m_bHasDefaultValue;
    vgui2::Menu* m_pContextMenu;
};

#endif