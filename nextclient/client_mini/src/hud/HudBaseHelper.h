#pragma once

#include "nitroapi/NitroApiHelper.h"
#include "nitroapi/NitroApiInterface.h"

class HudBaseHelper : public nitroapi::NitroApiHelper {
	float g_ColorBlue[3] = { 0.6, 0.8, 1.0 };
	float g_ColorRed[3] = { 1.0, 0.25, 0.25 };
	float g_ColorGrey[3] = { 0.8, 0.8, 0.8 };

protected:
	float& m_flTime;
	double& m_flTimeDelta;
	int& m_iHideHUDDisplay;
	bool& m_fPlayerDead;
	int& m_iWeaponBits;
	int& m_iFontHeight;

	pfnEngSrc_pfnSPR_Set_t SPR_Set;
	pfnEngSrc_pfnSPR_Draw_t SPR_Draw;
	pfnEngSrc_pfnSPR_DrawAdditive_t SPR_DrawAdditive;
	pfnEngSrc_pfnSPR_DrawHoles_t SPR_DrawHoles;
	pfnEngSrc_pfnSPR_Height_t SPR_Height;
	pfnEngSrc_pfnSPR_Width_t SPR_Width;
	pfnEngSrc_pfnFillRGBA_t FillRGBA;
	pfnEngSrc_pfnFillRGBABlend_t FillRGBABlend;

public:
	HudBaseHelper(nitroapi::NitroApiInterface* nitro_api)
		: nitroapi::NitroApiHelper(nitro_api)
		, m_flTimeDelta(*gHUD()->m_flTimeDelta)
		, m_flTime(*gHUD()->m_flTime)
		, m_iHideHUDDisplay(*gHUD()->m_iHideHUDDisplay)
		, m_fPlayerDead(*gHUD()->m_fPlayerDead)
		, m_iWeaponBits(*gHUD()->m_iWeaponBits)
		, m_iFontHeight(*gHUD()->m_iFontHeight) {
		SPR_Width = cl_enginefunc()->pfnSPR_Width;
		SPR_Height = cl_enginefunc()->pfnSPR_Height;
		SPR_Set = cl_enginefunc()->pfnSPR_Set;
		SPR_DrawHoles = cl_enginefunc()->pfnSPR_DrawHoles;
		SPR_Draw = cl_enginefunc()->pfnSPR_Draw;
		SPR_DrawAdditive = cl_enginefunc()->pfnSPR_DrawAdditive;
		FillRGBA = cl_enginefunc()->pfnFillRGBA;
		FillRGBABlend = cl_enginefunc()->pfnFillRGBABlend;
	}

	inline HSPRITE_t LoadSprite(const char* pszName) {
		char sz[256];
		Q_snprintf(sz, sizeof(sz), pszName, 640);
		return cl_enginefunc()->pfnSPR_Load(sz);
	}

	inline void GetScreenResolution(int& w, int& h) {
		SCREENINFO screen;
		screen.iSize = sizeof(SCREENINFO);
		cl_enginefunc()->pfnGetScreenInfo(&screen);
		w = screen.iWidth;
		h = screen.iHeight;
	}

	void GetAllPlayersInfo() {
		cl()->CHudHealth__GetAllPlayersInfo(gHUD()->m_Health);
	}

	inline bool IsValidClientIndex(int clientIndex) {
		return clientIndex > 0 && clientIndex <= MAX_PLAYERS;
	}

	float* GetClientColor(int clientIndex) {
		switch (cl()->g_PlayerExtraInfo[clientIndex].teamnumber) {
		case TEAM_TERRORIST:
			return g_ColorRed;
		case TEAM_CT:
			return g_ColorBlue;
		default:
			return g_ColorGrey;
		}
	}

	int DrawConsoleStringLen(const char *string) {
		int width, height;
		cl_enginefunc()->pfnDrawConsoleStringLen(string, &width, &height);
		return width;
	}

	int DrawConsoleStringHeight() {
		int width, height;
		cl_enginefunc()->pfnDrawConsoleStringLen("x", &width, &height);
		return height;
	}

	void DrawSetTextColor(vec3_t color) {
		cl_enginefunc()->pfnDrawSetTextColor(color[0], color[1], color[2]);
	}

	int DrawConsoleString(const char *string, int x, int y) {
		return cl_enginefunc()->pfnDrawConsoleString(x, y, const_cast<char*>(string));
	}

	inline void DrawRect(int x1, int y1, int x2, int y2,
		byte r, byte g, byte b, byte a
	) {
		FillRGBABlend(x1, y1, (x2 - x1), (y2 - y1), r, g, b, a);
	}

	void DrawOutlinedRect(int x1, int y1, int x2, int y2, 
		byte r, byte g, byte b, byte a, int outline_width, 
		byte outline_r, byte outline_g, byte outline_b, byte outline_a
	) {
		DrawRect(x1, y1, x2, y2, r, g, b, a);

		DrawRect(x1 - outline_width, y1 - outline_width, x2 + outline_width, y1,
			outline_r, outline_g, outline_b, outline_a);

		DrawRect(x2 + outline_width, y1, x2, y2 + outline_width,
			outline_r, outline_g, outline_b, outline_a);

		DrawRect(x2, y2 + outline_width, x1 - outline_width, y2,
			outline_r, outline_g, outline_b, outline_a);

		DrawRect(x1 - outline_width, y2, x1, y1,
			outline_r, outline_g, outline_b, outline_a);
	}
};