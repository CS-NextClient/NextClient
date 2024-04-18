#include "MainMenuBrowser.h"
#include <nitro_utils/string_utils.h>
#include <string>
#include <vector>
#include <fstream>
#include <string>
#include <format>
#include <steam/steam_api.h>
#include <data_encoding/aes.h>

auto transparent = Color(0, 0, 0, 0);

CMainMenuBrowser::CMainMenuBrowser(vgui2::Panel* parent) 
	: BaseClass(parent, "MainMenuBrowser") {
	SetVisible(false);
	SetBgColor(transparent);

	OpenHomePage();
}

void CMainMenuBrowser::InitializeBrowser() {
	m_pHtml = new vgui2::HTML(this, "MainMenuBrowserHTML");
	m_pHtml->InitializeBrowser("NextClient CEF1 Main Menu Browser", true);

	std::vector<std::string> languages;
    nitro_utils::split(SteamApps()->GetAvailableGameLanguages(), ",", languages);

	m_pHtml->SetLangSettings(SteamApps()->GetCurrentGameLanguage(), languages);
	m_pHtml->SetSteamId(SteamUser()->GetSteamID().ConvertToUint64());
	m_pHtml->SetContextMenuEnabled(false);

	SetVisible(true);
}

CMainMenuBrowser::~CMainMenuBrowser() {
}

void CMainMenuBrowser::OpenURL(const char *URL) {
	m_pHtml->OpenURL(URL, nullptr);
}

void CMainMenuBrowser::OnThink() {
	BaseClass::OnThink();

	SetBgColor(transparent);
}

void CMainMenuBrowser::OnScreenSizeChanged(int oldWide, int oldTall) {
	InvalidateLayout();
}

void CMainMenuBrowser::SetFullscreen(bool status) {
	m_bRenderFullscreen = status;
}

void CMainMenuBrowser::PerformLayout() {
	BaseClass::PerformLayout();

	int screenWide, screenTall;
	vgui2::surface()->GetScreenSize(screenWide, screenTall);

	if(m_bRenderFullscreen) {
		SetBounds(0, 0, screenWide, screenTall);
		m_pHtml->SetBounds(0, 0, screenWide, screenTall);
	}
	else {
		auto browserWidth = std::clamp(screenWide - 300, 450, 1152);

		SetBounds(screenWide - browserWidth, 0, browserWidth, screenTall);
		m_pHtml->SetBounds(0, 0, browserWidth, screenTall);
	}
}

std::vector<uint8_t> CMainMenuBrowser::ReadFile(const std::string& fileName) {
	std::ifstream file(fileName.c_str(), std::ios::binary);
	if(!file.is_open()) return {};

    file.seekg(0, std::ifstream::end);
    size_t length = file.tellg();
    file.seekg(0, std::ifstream::beg);

    std::vector<uint8_t> fileData(length);
	file.read(reinterpret_cast<char *>(fileData.data()), length);

	return fileData;
}

void CMainMenuBrowser::OpenHomePage() {
	auto rawHomePageUrl = ReadFile("platform/mmb_home_page.html");
	if(!rawHomePageUrl.size()) return;

	InitializeBrowser();

	auto rawErrorPageHtml = ReadFile("platform/mmb_error_page.html");
	if(rawErrorPageHtml.size() > 0) {
		std::string errorPageHtml(rawErrorPageHtml.begin(), rawErrorPageHtml.end());
		nitro_utils::trim(errorPageHtml);
		
		errorPageHtml = std::format(
			"<script>document.body.setAttribute('style', '');document.querySelector('h1').style.display='none'</script>{}", 
			errorPageHtml
		);

		m_pHtml->BrowserErrorStrings("", "", errorPageHtml.c_str(), errorPageHtml.c_str(), 
			errorPageHtml.c_str(), errorPageHtml.c_str(), errorPageHtml.c_str());
	}

	OpenURL(std::string(rawHomePageUrl.begin(), rawHomePageUrl.end()).c_str());
}