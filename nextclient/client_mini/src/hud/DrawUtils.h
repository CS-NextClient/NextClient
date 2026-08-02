#pragma once

#include "main.h"

#include <ncl_math/color.h>

using namespace std;

namespace DrawUtils {

static inline ncl_math::Color UnpackRGB(const unsigned long ulRGB) {
	return ncl_math::Color::FromPacked(ulRGB);
}

static inline ncl_math::Color ScaleColors(ncl_math::Color c, const int a) {
	const float scale = a / 255.0f;
	return {
		(int)ncl_math::Clamp(c.r * scale, 0.0f, 255.0f),
		(int)ncl_math::Clamp(c.g * scale, 0.0f, 255.0f),
		(int)ncl_math::Clamp(c.b * scale, 0.0f, 255.0f),
		c.a
	};
}

static inline int DrawHudNumber(
	int iX, int iY, int number,
	ncl_math::Color color,
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
			gEngfuncs.pfnSPR_Set(gHUD->GetSprite(index + iNum), color.r, color.g, color.b);

		gEngfuncs.pfnSPR_DrawAdditive(0, iX, iY, &gHUD->GetSpriteRect(index + iNum));
		iX += width + digitSpacing;
	}
	return iX;
}

}