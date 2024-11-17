//========= Copyright ?1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "CvarTextEntry.h"
#include "GameUi.h"
#include <vgui/IVGui.h>
#include "IGameUIFuncs.h"
#include <KeyValues.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CCvarTextEntry::CCvarTextEntry(Panel *parent, const char *panelName, char const *cvarname)
    : TextEntry(parent, panelName)
{
    m_pszCvarName = cvarname ? V_strdup(cvarname) : nullptr;
    m_pszStartValue[0] = 0;

    if (m_pszCvarName)
    {
        Reset();
    }

    AddActionSignalTarget(this);
}

CCvarTextEntry::~CCvarTextEntry()
{
    delete[] m_pszCvarName;
}

void CCvarTextEntry::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);
    if (GetMaximumCharCount() < 0 || GetMaximumCharCount() > MAX_CVAR_TEXT)
    {
        SetMaximumCharCount(MAX_CVAR_TEXT - 1);
    }
}

void CCvarTextEntry::ApplyChanges(bool immediate)
{
    if (!m_pszCvarName)
        return;

    char szText[MAX_CVAR_TEXT];
    GetText(szText, MAX_CVAR_TEXT);

    if (!szText[0])
        return;

    if (immediate)
    {
        engine->Cvar_Set(m_pszCvarName, szText);
    }
    else
    {
        char szCommand[256];
        V_sprintf_safe(szCommand, "%s \"%s\"\n", m_pszCvarName, szText);
        engine->pfnClientCmd(szCommand);
    }

    V_strcpy_safe(m_pszStartValue, szText);
}

void CCvarTextEntry::Reset()
{
    auto value = engine->pfnGetCvarString(m_pszCvarName);
    if (value && value[0])
    {
        SetText(value);
        V_strcpy_safe(m_pszStartValue, value);
    }
}

bool CCvarTextEntry::HasBeenModified()
{
    char szText[MAX_CVAR_TEXT];
    GetText(szText, MAX_CVAR_TEXT);

    return V_stricmp(szText, m_pszStartValue);
}


void CCvarTextEntry::OnTextChanged()
{
    if (!m_pszCvarName)
        return;

    if (HasBeenModified())
    {
        PostActionSignal(new KeyValues("ControlModified"));
    }
}

//-----------------------------------------------------------------------------
// Purpose: Message mapping 
//-----------------------------------------------------------------------------
vgui2::MessageMapItem_t CCvarTextEntry::m_MessageMap[] =
    {
        MAP_MESSAGE(CCvarTextEntry, "TextChanged", OnTextChanged),	// custom message
    };

IMPLEMENT_PANELMAP(CCvarTextEntry, BaseClass);