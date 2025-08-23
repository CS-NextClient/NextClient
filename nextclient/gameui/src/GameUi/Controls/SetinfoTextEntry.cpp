#include "SetinfoTextEntry.h"
#include "GameUi.h"
#include <vgui/IVGui.h>
#include "IGameUIFuncs.h"
#include <KeyValues.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CSetinfoTextEntry::CSetinfoTextEntry(Panel *parent, const char *panelName, char const *setinfoName)
    : TextEntry(parent, panelName)
{
    m_pszSetinfoName = setinfoName ? V_strdup(setinfoName) : nullptr;
    m_pszStartValue[0] = 0;

    if (m_pszSetinfoName)
    {
        Reset();
    }

    AddActionSignalTarget(this);
}

CSetinfoTextEntry::~CSetinfoTextEntry()
{
    delete[] m_pszSetinfoName;
}

void CSetinfoTextEntry::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);
    if (GetMaximumCharCount() < 0 || GetMaximumCharCount() > MAX_SETINFO_TEXT)
    {
        SetMaximumCharCount(MAX_SETINFO_TEXT - 1);
    }
}

void CSetinfoTextEntry::ApplyChanges()
{
    if (!m_pszSetinfoName)
        return;

    char szText[MAX_SETINFO_TEXT];
    GetText(szText, MAX_SETINFO_TEXT);

    engine->PlayerInfo_SetValueForKey(m_pszSetinfoName, szText);
    V_strcpy_safe(m_pszStartValue, szText);
}

void CSetinfoTextEntry::Reset()
{
    auto value = engine->LocalPlayerInfo_ValueForKey(m_pszSetinfoName);
    if (value)
    {
        SetText(value);
        V_strcpy_safe(m_pszStartValue, value);
    }
}

bool CSetinfoTextEntry::HasBeenModified()
{
    char szText[MAX_SETINFO_TEXT];
    GetText(szText, MAX_SETINFO_TEXT);

    return V_stricmp(szText, m_pszStartValue);
}


void CSetinfoTextEntry::OnTextChanged()
{
    if (!m_pszSetinfoName)
        return;

    if (HasBeenModified())
    {
        PostActionSignal(new KeyValues("ControlModified"));
    }
}

//-----------------------------------------------------------------------------
// Purpose: Message mapping
//-----------------------------------------------------------------------------
vgui2::MessageMapItem_t CSetinfoTextEntry::m_MessageMap[] =
    {
        MAP_MESSAGE(CSetinfoTextEntry, "TextChanged", OnTextChanged),	// custom message
    };

IMPLEMENT_PANELMAP(CSetinfoTextEntry, BaseClass);