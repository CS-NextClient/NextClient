#include "GameUi.h"
#include "OptionsSubMultiplayer.h"
#include "MultiplayerAdvancedDialog.h"
#include "OptionsDialog.h"
#include <cstdio>

#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include "tier1/KeyValues.h"
#include <vgui_controls/Label.h>
#include <vgui/ISystem.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/Cursor.h>
#include <vgui_controls/RadioButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/FileOpenDialog.h>
#include <vgui_controls/MessageBox.h>
#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <vgui/IPanel.h>
#include <vgui_controls/MessageBox.h>

#include "CvarTextEntry.h"
#include "CvarToggleCheckButton.h"
#include "CvarSlider.h"
#include "LabeledCommandComboBox.h"
#include "FileSystem.h"
#include "BitmapImagePanel.h"
#include "utlbuffer.h"
#include "ModInfo.h"
#include "LogoFile.h"

#include <csetjmp>
#include <io.h>

#include "ImageLib/LoadBMP.h"

struct ColorItem_t
{
    const char *name;
    int r, g, b;
};

static ColorItem_t itemlist[] =
    {
        { "#Valve_Orange", 255, 120, 24 },
        { "#Valve_Yellow", 225, 180, 24 },
        { "#Valve_Blue", 0, 60, 255 },
        { "#Valve_Ltblue", 0, 167, 255 },
        { "#Valve_Green", 0, 167, 0 },
        { "#Valve_Red", 255, 43, 0 },
        { "#Valve_Brown", 123, 73, 0 },
        { "#Valve_Ltgray", 100, 100, 100 },
        { "#Valve_Dkgray", 36, 36, 36 },
    };

COptionsSubMultiplayer::COptionsSubMultiplayer(vgui2::Panel *parent) : vgui2::PropertyPage(parent, "OptionsSubMultiplayer")
{
    vgui2::Button *cancel = new vgui2::Button(this, "Cancel", "#GameUI_Cancel");
    cancel->SetCommand("Close");

    vgui2::Button *ok = new vgui2::Button(this, "OK", "#GameUI_OK");
    ok->SetCommand("Ok");

    vgui2::Button *apply = new vgui2::Button(this, "Apply", "#GameUI_Apply");
    apply->SetCommand("Apply");

    vgui2::Button *advanced = new vgui2::Button(this, "Advanced", "#GameUI_AdvancedEllipsis");
    advanced->SetCommand("Advanced");

    m_pNameTextEntry = new CCvarTextEntry(this, "NameEntry", "name");
    m_pPasswordTextEntry = new CSetinfoTextEntry(this, "PasswordEntry", "_pw");
    m_pHighQualityModelCheckBox = new CCvarToggleCheckButton(this, "High Quality Models", "#GameUI_HighModels", "cl_himodels");

    m_pLogoList = new CLabeledCommandComboBox(this, "SpraypaintList");
    m_pColorList = new CLabeledCommandComboBox(this, "SpraypaintColor");
    m_LogoName[0] = 0;

    InitLogoColorEntries();
    InitLogoList(m_pLogoList);

    m_pLogoImage = new CBitmapImagePanel(this, "LogoImage");
    m_pLogoImage->AddActionSignalTarget(this);

    m_nLogoR = 255;
    m_nLogoG = 255;
    m_nLogoB = 255;

    vgui2::Button *crosshair = new vgui2::Button(this, "CrosshairSettings", "#GameUI_CrosshairSettingsBtn");
    crosshair->SetCommand("CrosshairSettings");

    LoadControlSettings("Resource\\OptionsSubMultiplayer.res");
}

COptionsSubMultiplayer::~COptionsSubMultiplayer(void)
{
}

void COptionsSubMultiplayer::OnCommand(const char *command)
{
    if (!stricmp(command, "CrosshairSettings"))
    {
        COptionsDialog *options = dynamic_cast<COptionsDialog *>(GetParent()->GetParent());

        if (options)
            options->OpenCrosshairSettings();

        return;
    }

    if (!stricmp(command, "Advanced"))
    {
        if (!m_hMultiplayerAdvancedDialog.Get())
            m_hMultiplayerAdvancedDialog = new CMultiplayerAdvancedDialog(this);

        m_hMultiplayerAdvancedDialog->Activate();
        m_hMultiplayerAdvancedDialog->SetPos(100, 100);
    }

    BaseClass::OnCommand(command);
}

void COptionsSubMultiplayer::InitLogoList(CLabeledCommandComboBox *cb)
{
    FileFindHandle_t fh;
    char directory[512];

    g_pFullFileSystem->RemoveFile("logos/remapped.bmp", NULL);

    const char *logofile = engine->pfnGetCvarString("cl_logofile");
    sprintf(directory, "logos/*.bmp");
    const char *fn = g_pFullFileSystem->FindFirst(directory, &fh);
    int i = 0, initialItem = 0;

    cb->DeleteAllItems();

    while (fn)
    {
        if (stricmp(fn, "remapped.bmp"))
        {
            if (fn[0] && fn[0] != '.')
            {
                char filename[512];
                strcpy(filename, fn);

                if (strlen(filename) >= 4)
                    filename[strlen(filename) - 4] = 0;

                if (!stricmp(filename, logofile))
                    initialItem = i;

                cb->AddItem(filename, "");

                if (m_LogoName[0] == 0)
                    strcpy(m_LogoName, filename);
            }

            i++;
        }

        fn = g_pFullFileSystem->FindNext(fh);
    }

    g_pFullFileSystem->FindClose(fh);
    cb->SetInitialItem(initialItem);
}

void COptionsSubMultiplayer::InitLogoColorEntries(void)
{
    char const *currentcolor = engine->pfnGetCvarString("cl_logocolor");
    int count = sizeof(itemlist) / sizeof(itemlist[0]);
    int selected = 0;

    for (int i = 0; i < count; i++)
    {
        if (currentcolor && !stricmp(currentcolor, itemlist[i].name))
            selected = i;

        char command[256];
        sprintf(command, "cl_logocolor %s\n", itemlist[i].name);
        m_pColorList->AddItem(itemlist[i].name, command);
    }

    m_pColorList->SetInitialItem(selected);
    m_pColorList->AddActionSignalTarget(this);
}

void COptionsSubMultiplayer::RemapLogo(void)
{
    char logoname[256];
    m_pLogoList->GetText(logoname, sizeof(logoname));

    if (!logoname[0])
        return;

    int r, g, b;
    const char *colorname = m_pColorList->GetActiveItemCommand();

    if (!colorname || !colorname[0])
        return;

    colorname += strlen("cl_logocolor ");

    ColorForName(colorname, r, g, b);
    RemapLogoPalette(logoname, r, g, b);

    m_pLogoImage->setTexture("logos/remapped", true);
}

void COptionsSubMultiplayer::OnTextChanged(vgui2::Panel *panel)
{
    if (panel == m_pNameTextEntry || panel == m_pPasswordTextEntry)
        return;

    if (panel == m_pLogoList || panel == m_pColorList)
        RemapLogo();
}

void COptionsSubMultiplayer::OnSliderMoved(KeyValues *data)
{
}

void COptionsSubMultiplayer::OnApplyButtonEnable(void)
{
    PostMessage(GetParent(), new KeyValues("ApplyButtonEnable"));
    InvalidateLayout();
}

void COptionsSubMultiplayer::ColorForName(char const *pszColorName, int &r, int &g, int &b)
{
    r = g = b = 0;

    int count = sizeof(itemlist) / sizeof(itemlist[0]);

    for (int i = 0; i < count; i++)
    {
        if (!strnicmp(pszColorName, itemlist[i].name, strlen(itemlist[i].name)))
        {
            r = itemlist[i].r;
            g = itemlist[i].g;
            b = itemlist[i].b;
            return;
        }
    }
}

#include <Windows.h>
void COptionsSubMultiplayer::RemapLogoPalette(char *filename, int r, int g, int b)
{
    char infile[256];
    char outfile[256];

    CUtlBuffer outbuffer(16384, 16384, false);
    sprintf(infile, "logos/%s.bmp", filename);
    sprintf(outfile, "logos/remapped.bmp");

    FileHandle_t file = g_pFullFileSystem->Open(infile, "rb");

    if (file == FILESYSTEM_INVALID_HANDLE)
        return;

    BITMAPFILEHEADER bmfHeader;
    DWORD dwBitsSize, dwFileSize;
    LPBITMAPINFO lpbmi;
    LPBITMAPCOREINFO lpbmc;

    dwFileSize = g_pFullFileSystem->Size(file);
    g_pFullFileSystem->Read(&bmfHeader, sizeof(bmfHeader), file);
    outbuffer.Put(&bmfHeader, sizeof(bmfHeader));

    if (bmfHeader.bfType == DIB_HEADER_MARKER)
    {
        dwBitsSize = dwFileSize - sizeof(bmfHeader);

        HGLOBAL hDIB = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, dwBitsSize);
        char *pDIB = (LPSTR)GlobalLock((HGLOBAL)hDIB);
        {
            g_pFullFileSystem->Read(pDIB, dwBitsSize, file);
            lpbmi = (LPBITMAPINFO)pDIB;
            lpbmc = (LPBITMAPCOREINFO)pDIB;

            bool bWinStyleDIB = true;
            float f = 0;

            for (int i = 0; i < 256; i++)
            {
                float t = f / 256.0;

                if (bWinStyleDIB)
                {
                    lpbmi->bmiColors[i].rgbRed = (unsigned char)(r * t);
                    lpbmi->bmiColors[i].rgbGreen = (unsigned char)(g * t);
                    lpbmi->bmiColors[i].rgbBlue = (unsigned char)(b * t);
                }
                else
                {
                    lpbmc->bmciColors[i].rgbtRed = (unsigned char)(r * t);
                    lpbmc->bmciColors[i].rgbtGreen = (unsigned char)(g * t);
                    lpbmc->bmciColors[i].rgbtBlue = (unsigned char)(b * t);
                }

                f++;
            }

            outbuffer.Put(pDIB, dwBitsSize);
        }

        GlobalUnlock(hDIB);
        GlobalFree((HGLOBAL) hDIB);
    }

    g_pFullFileSystem->Close(file);
    g_pFullFileSystem->RemoveFile(outfile, NULL);

    g_pFullFileSystem->CreateDirHierarchy("logos", NULL);
    file = g_pFullFileSystem->Open(outfile, "wb");

    if (file != FILESYSTEM_INVALID_HANDLE)
    {
        g_pFullFileSystem->Write(outbuffer.Base(), outbuffer.TellPut(), file);
        g_pFullFileSystem->Close(file);
    }
}

void COptionsSubMultiplayer::OnPageShow(void)
{
    m_pNameTextEntry->Reset();
    m_pNameTextEntry->GotoTextEnd();
    m_pPasswordTextEntry->Reset();
    m_pPasswordTextEntry->GotoTextEnd();
}

void COptionsSubMultiplayer::OnResetData(void)
{
    m_pNameTextEntry->Reset();
    m_pNameTextEntry->GotoTextEnd();
    m_pPasswordTextEntry->Reset();
    m_pPasswordTextEntry->GotoTextEnd();
    m_pLogoList->Reset();
    m_pColorList->Reset();
    m_pHighQualityModelCheckBox->Reset();
}

void COptionsSubMultiplayer::OnApplyChanges(void)
{
    m_pLogoList->ApplyChanges();
    m_pLogoList->GetText(m_LogoName, sizeof(m_LogoName));
    m_pColorList->ApplyChanges();
    m_pHighQualityModelCheckBox->ApplyChanges();
    m_pNameTextEntry->ApplyChanges();
    m_pPasswordTextEntry->ApplyChanges();

    for (int i = 0; i < m_cvarToggleCheckButtons.GetCount(); ++i)
    {
        CCvarToggleCheckButton *toggleButton = m_cvarToggleCheckButtons[i];

        if (toggleButton->IsVisible() && toggleButton->IsEnabled())
            toggleButton->ApplyChanges();
    }

    const char *colorname = m_pColorList->GetActiveItemCommand();

    if (colorname && colorname[0])
    {
        colorname += strlen("cl_logocolor ");

        char cmd[512];
        _snprintf(cmd, sizeof(cmd) - 1, "cl_logofile %s\n", m_LogoName);
        engine->pfnClientCmd(cmd);

        int r, g, b;
        ColorForName(colorname, r, g, b);

        char infile[256];
        sprintf(infile, "logos/remapped.bmp");
        FileHandle_t file = g_pFullFileSystem->Open(infile, "rb");

        if (file != FILESYSTEM_INVALID_HANDLE)
        {
            BITMAPFILEHEADER bmfHeader;
            DWORD dwBitsSize, dwFileSize;

            dwFileSize = g_pFullFileSystem->Size(file);
            g_pFullFileSystem->Read(&bmfHeader, sizeof(bmfHeader), file);

            if (bmfHeader.bfType == DIB_HEADER_MARKER)
            {
                dwBitsSize = dwFileSize - sizeof(bmfHeader);
                HGLOBAL hDIB = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, dwBitsSize);

                char *pDIB = (LPSTR)GlobalLock((HGLOBAL)hDIB);
                g_pFullFileSystem->Read(pDIB, dwBitsSize, file);
                GlobalUnlock((HGLOBAL)hDIB);
                UpdateLogoWAD((void *)hDIB, r, g, b);
                GlobalFree((HGLOBAL)hDIB);
            }

            g_pFullFileSystem->Close(file);
        }
    }
}

vgui2::Panel *COptionsSubMultiplayer::CreateControlByName(const char *controlName)
{
    if (!Q_stricmp("CCvarToggleCheckButton", controlName))
    {
        CCvarToggleCheckButton *newButton = new CCvarToggleCheckButton(this, controlName, "", "");
        m_cvarToggleCheckButtons.AddElement(newButton);
        return newButton;
    }
    else
        return BaseClass::CreateControlByName(controlName);
}
