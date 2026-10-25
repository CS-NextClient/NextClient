#pragma once

#include "main.h"

// Runs a binding like "+a;+b" itself so that releasing the key releases every button in it
int MultiButtonBinds_KeyEvent(int down, int keynum, const char* binding, HUD_Key_EventNext next);
