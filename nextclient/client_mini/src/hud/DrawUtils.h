#pragma once

#include "../main.h"

using namespace std;

namespace DrawUtils {

static inline void UnpackRGB(int& r, int& g, int& b, const unsigned long ulRGB) {
	r = (ulRGB & 0xFF0000) >> 16;
	g = (ulRGB & 0xFF00) >> 8;
	b = ulRGB & 0xFF;
}

static inline void ScaleColors(int& r, int& g, int& b, const int a) {
	r *= a / 255.0f;
	g *= a / 255.0f;
	b *= a / 255.0f;
}

static inline int DrawHudNumber(
	int iX, int iY, int number,
	int r, int g, int b,
	int iDrawZero, int maxsize, 
	int digitSpacing = 0, bool legacyPadding = true
) {
	if (maxsize <= 0) {
		maxsize = 1;
		for (int num = 10; (number / num) > 0; num *= 10)
			maxsize++;
	}

	if (maxsize > 255)
		maxsize = 255;

	int index = *gHUD->m_HUD_number_0;
	int width = gHUD->GetSpriteWidth(index);
	bool bShouldDraw = false;

	if(legacyPadding) {
		if(number >= 100) {}
		else if(number >= 10) iX += width;
		else if(number >= 1) iX += width * 2;
	}

	for (int i = 0; i < maxsize; i++) {
		int div = 1;
		for (int j = 0; j < maxsize - i; j++)
			div *= 10;

		int iNum = (number % div * 10) / div;

		if (iNum)
			bShouldDraw = true;

		if (!iDrawZero && !iNum && !bShouldDraw && i != maxsize - 1)
			continue;

		if (!iNum && !bShouldDraw)
			gEngfuncs.pfnSPR_Set(gHUD->GetSprite(index), 100, 100, 100);
		else
			gEngfuncs.pfnSPR_Set(gHUD->GetSprite(index + iNum), r, g, b);

		gEngfuncs.pfnSPR_DrawAdditive(0, iX, iY, &gHUD->GetSpriteRect(index + iNum));
		iX += width + digitSpacing;
	}
	return iX;
}

}