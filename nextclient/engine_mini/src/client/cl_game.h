#pragma once
#include <goldsrc/client_api.h>
#include <goldsrc/types.h>

#include "../hlsdk.h"

void SetCrosshair(goldsrc::HSPRITE_t hspr, goldsrc::wrect_t rc, int r, int g, int b);
void DrawCrosshair(int x, int y);