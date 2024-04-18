#pragma once

#include <steam/steam_api.h>

typedef void (*NextSteamProxy_ExceptionFunc)(void* exception_pointers);

typedef void (*NextSteamProxy_SetSEHFunc)(NextSteamProxy_ExceptionFunc exception_callback);

S_API void NextSteamProxy_SetSEH(NextSteamProxy_ExceptionFunc exception_callback);
