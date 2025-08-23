#pragma once

#include <vgui_controls/TextEntry.h>

class CSetinfoTextEntry : public vgui2::TextEntry
{
    static const int MAX_SETINFO_TEXT = 64;

public:
    CSetinfoTextEntry(vgui2::Panel *parent, const char *panelName, char const *setinfoName);
    ~CSetinfoTextEntry();

    void			OnTextChanged();
    void			ApplyChanges();
    virtual void	ApplySchemeSettings(vgui2::IScheme *pScheme);
    void            Reset();
    bool            HasBeenModified();

    DECLARE_PANELMAP();

private:
    typedef vgui2::TextEntry BaseClass;

    char			*m_pszSetinfoName;
    char			m_pszStartValue[MAX_SETINFO_TEXT];
};