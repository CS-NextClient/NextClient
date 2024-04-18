#pragma once

#include <GameUi.h>
#include <vgui_controls/HTML.h>
#include <vgui_controls/EditablePanel.h>

class CMainMenuBrowser : public vgui2::EditablePanel {
public:
	DECLARE_CLASS_SIMPLE(CMainMenuBrowser, EditablePanel);

	CMainMenuBrowser(vgui2::Panel *parent);
	virtual ~CMainMenuBrowser();

	void OpenHomePage();
	void OpenURL(const char *URL);
	void SetFullscreen(bool status);

	// IViewportPanel overrides
	const char* GetName() override {
		return "MainMenuBrowser";
	}

	// VGUI functions:
	vgui2::VPANEL GetVPanel() override final {
		return BaseClass::GetVPanel();
	}

	bool IsVisible() override final {
		return BaseClass::IsVisible();
	}

	void SetParent(vgui2::VPANEL parent) override final {
		BaseClass::SetParent(parent);
	}

private:
	bool m_bRenderFullscreen = false;
	vgui2::HTML* m_pHtml;

	virtual void PerformLayout();
	virtual void OnThink();

	void InitializeBrowser();
	std::vector<uint8_t> ReadFile(const std::string& fileName);

	MESSAGE_FUNC_INT_INT( OnScreenSizeChanged, "OnScreenSizeChanged", oldWide, oldTall );
};