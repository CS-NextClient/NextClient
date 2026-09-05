#include "GameUi.h"
#include "CvarSlider.h"
#include "cvar_context_menu.h"
#include <stdio.h>
#include "tier1/KeyValues.h"
#include <vgui/IVGui.h>
#include <vgui_controls/propertypage.h>

#define CVARSLIDER_SCALE_FACTOR 100.0f

using namespace vgui2;

DECLARE_BUILD_FACTORY(CCvarSlider);

CCvarSlider::CCvarSlider(Panel *parent, const char *name) : Slider(parent, name)
{
    m_pContextMenu = NULL;

    SetupSlider(0, 1, "", false);
    m_bCreatedInCode = false;

    AddActionSignalTarget(this);
}

CCvarSlider::CCvarSlider(Panel *parent, const char *panelName, char const *caption, float minValue, float maxValue, char const *cvarname, bool bAllowOutOfRange) : Slider(parent, panelName)
{
    m_pContextMenu = NULL;

    AddActionSignalTarget(this);
    SetupSlider(minValue, maxValue, cvarname, bAllowOutOfRange);

    m_bCreatedInCode = true;
}

void CCvarSlider::SetupSlider(float minValue, float maxValue, const char *cvarname, bool bAllowOutOfRange)
{
    m_flMinValue = minValue;
    m_flMaxValue = maxValue;
    m_flScale = CVARSLIDER_SCALE_FACTOR;
    m_iPrintPrecision = 2;

    SetRange(static_cast<int>(m_flScale * minValue), static_cast<int>(m_flScale * maxValue));

    char szMin[32];
    char szMax[32];

    Q_snprintf(szMin, sizeof(szMin), "%.2f", minValue);
    Q_snprintf(szMax, sizeof(szMax), "%.2f", maxValue);

    SetTickCaptions(szMin, szMax);

    Q_strncpy(m_szCvarName, cvarname, sizeof(m_szCvarName));

    m_flDefaultValue = 0.0f;
    m_bHasDefaultValue = false;

    m_bModifiedOnce = false;
    m_bAllowOutOfRange = bAllowOutOfRange;

    Reset();
}

CCvarSlider::~CCvarSlider(void)
{
}

void CCvarSlider::ApplySettings(KeyValues *inResourceData)
{
    BaseClass::ApplySettings(inResourceData);

    if (!m_bCreatedInCode)
    {
        float minValue = inResourceData->GetFloat("minvalue", 0);
        float maxValue = inResourceData->GetFloat("maxvalue", 1);
        const char *cvarname = inResourceData->GetString("cvar_name", "");
        bool bAllowOutOfRange = inResourceData->GetInt("allowoutofrange", 0) != 0;
        SetupSlider(minValue, maxValue, cvarname, bAllowOutOfRange);

        if (GetParent())
        {
            if (dynamic_cast<vgui2::PropertyPage*>(GetParent()) && GetParent()->GetParent())
                GetParent()->GetParent()->AddActionSignalTarget(this);
            else
                GetParent()->AddActionSignalTarget(this);
        }
    }
}

void CCvarSlider::GetSettings(KeyValues *outResourceData)
{
    BaseClass::GetSettings(outResourceData);

    if (!m_bCreatedInCode)
    {
        outResourceData->SetFloat("minvalue", m_flMinValue);
        outResourceData->SetFloat("maxvalue", m_flMaxValue);
        outResourceData->SetString("cvar_name", m_szCvarName);
        outResourceData->SetInt("allowoutofrange", m_bAllowOutOfRange);
    }
}

void CCvarSlider::SetCVarName(char const *cvarname)
{
    Q_strncpy(m_szCvarName, cvarname, sizeof(m_szCvarName));

    m_bModifiedOnce = false;

    Reset();
}

void CCvarSlider::SetDefaultValue(float value)
{
    m_flDefaultValue = value;
    m_bHasDefaultValue = true;
}

bool CCvarSlider::ResetToDefaultValue()
{
    if (!m_bHasDefaultValue)
        return false;

    SetSliderValue(m_flDefaultValue);

    return true;
}

void CCvarSlider::OnMousePressed(vgui2::MouseCode code)
{
    if (code == vgui2::MOUSE_RIGHT && CvarContextMenu_Show(this, m_pContextMenu, m_szCvarName, m_bHasDefaultValue))
    {
        return;
    }

    BaseClass::OnMousePressed(code);
}

void CCvarSlider::OnResetToDefault(void)
{
    if (ResetToDefaultValue())
        PostActionSignal(new KeyValues("ControlModified"));
}

void CCvarSlider::SetMinMaxValues(float minValue, float maxValue, bool bSetTickDisplay)
{
    SetRange(static_cast<int>(m_flScale * minValue), static_cast<int>(m_flScale * maxValue));

    if (bSetTickDisplay)
    {
        char szMin[32];
        char szMax[32];

        Q_snprintf(szMin, sizeof(szMin), "%.2f", minValue);
        Q_snprintf(szMax, sizeof(szMax), "%.2f", maxValue);

        SetTickCaptions(szMin, szMax);
    }

    Reset();
}

void CCvarSlider::SetScale(float scale, int print_precision)
{
    m_flScale = scale;
    m_iPrintPrecision = print_precision;

    SetRange(static_cast<int>(m_flScale * m_flMinValue), static_cast<int>(m_flScale * m_flMaxValue));

    char szMin[32];
    char szMax[32];

    Q_snprintf(szMin, sizeof(szMin), "%.*f", print_precision, m_flMinValue);
    Q_snprintf(szMax, sizeof(szMax), "%.*f", print_precision, m_flMaxValue);

    SetTickCaptions(szMin, szMax);

    Reset();
}

void CCvarSlider::SetTickColor(Color color)
{
    m_TickColor = color;
}

void CCvarSlider::Paint(void)
{
    float curvalue = engine->pfnGetCvarFloat(m_szCvarName);

    if (curvalue != m_fStartValue)
    {
        int val = static_cast<int>(m_flScale * curvalue);
        m_fStartValue = curvalue;
        m_fCurrentValue = curvalue;

        SetValue(val, false);
        m_iStartValue = GetValue();
        m_iLastSliderValue = m_iStartValue;

        PostActionSignal(new KeyValues("CvarChanged"));
    }

    BaseClass::Paint();
}

void CCvarSlider::ApplyChanges(void)
{
    if (m_bModifiedOnce)
    {
        m_iStartValue = GetValue();

        if (m_bAllowOutOfRange)
            m_fStartValue = m_fCurrentValue;
        else
            m_fStartValue = static_cast<float>(m_iStartValue) / m_flScale;

        char value[128];
        Q_snprintf(value, sizeof(value), "%.*f", m_iPrintPrecision, m_fStartValue);
        engine->Cvar_Set(m_szCvarName, value);
    }
}

float CCvarSlider::GetSliderValue(void)
{
    if (m_bAllowOutOfRange)
        return m_fCurrentValue;
    else
        return static_cast<float>(GetValue()) / m_flScale;
}

void CCvarSlider::SetSliderValue(float fValue)
{
    int nVal = static_cast<int>(m_flScale * fValue);
    SetValue(nVal, false);

    m_iLastSliderValue = GetValue();

    if (m_fCurrentValue != fValue)
    {
        m_fCurrentValue = fValue;
        m_bModifiedOnce = true;
    }
}

void CCvarSlider::Reset(void)
{
    m_fStartValue = engine->pfnGetCvarFloat(m_szCvarName);
    m_fCurrentValue = m_fStartValue;

    int value = static_cast<int>(m_flScale * m_fStartValue);
    SetValue(value, false);

    m_iStartValue = GetValue();
    m_iLastSliderValue = m_iStartValue;
}

bool CCvarSlider::HasBeenModified(void)
{
    if (GetValue() != m_iStartValue)
        m_bModifiedOnce = true;

    return m_bModifiedOnce;
}

void CCvarSlider::OnSliderMoved(void)
{
    if (HasBeenModified())
    {
        if (m_iLastSliderValue != GetValue())
        {
            m_iLastSliderValue = GetValue();
            m_fCurrentValue = static_cast<float>(m_iLastSliderValue) / m_flScale;
        }

        PostActionSignal(new KeyValues("ControlModified"));
    }
}

void CCvarSlider::OnApplyChanges(void)
{
    if (!m_bCreatedInCode)
        ApplyChanges();
}