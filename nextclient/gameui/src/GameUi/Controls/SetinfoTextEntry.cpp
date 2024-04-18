#include "SetinfoTextEntry.h"
#include "GameUi.h"
#include <vgui/IVGui.h>
#include "IGameUIFuncs.h"
#include <KeyValues.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const int MAX_CVAR_TEXT = 64;

CSetinfoTextEntry::CSetinfoTextEntry(Panel *parent, const char *panelName, char const *cvarname)
    : TextEntry(parent, panelName)
{
    m_pszSetinfoName = cvarname ? strdup(cvarname) : NULL;
    m_pszStartValue[0] = 0;

    if (m_pszSetinfoName)
    {
        Reset();
    }

    AddActionSignalTarget(this);
}

CSetinfoTextEntry::~CSetinfoTextEntry()
{
    if (m_pszSetinfoName)
    {
        free(m_pszSetinfoName);
    }
}

void CSetinfoTextEntry::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);
    if (GetMaximumCharCount() < 0 || GetMaximumCharCount() > MAX_CVAR_TEXT)
    {
        SetMaximumCharCount(MAX_CVAR_TEXT - 1);
    }
}

void CSetinfoTextEntry::ApplyChanges()
{
    if (!m_pszSetinfoName)
        return;

    char szText[MAX_CVAR_TEXT];
    GetText(szText, MAX_CVAR_TEXT);

    if (!szText[0])
        return;

    engine->PlayerInfo_SetValueForKey(m_pszSetinfoName, szText);
    strcpy(m_pszStartValue, szText);
}

void CSetinfoTextEntry::Reset()
{
    auto value = engine->LocalPlayerInfo_ValueForKey("_pw");
    if (value && value[0])
    {
        SetText(value);
        strcpy(m_pszStartValue, value);
    }
}

bool CSetinfoTextEntry::HasBeenModified()
{
    char szText[MAX_CVAR_TEXT];
    GetText(szText, MAX_CVAR_TEXT);

    return stricmp(szText, m_pszStartValue);
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