#pragma once

#include <string>

// The client never receives the server's name, so it is asked for it over A2S_INFO,
// the same query the server browser sends
void DiscordHostname_Init();
void DiscordHostname_Shutdown();

// Starts a query whenever the client lands on a new server; call it from the main thread
void DiscordHostname_Update();

// Raw name as the server sent it: may be empty, and is not guaranteed to be valid UTF-8
const std::string& DiscordHostname_Get();
