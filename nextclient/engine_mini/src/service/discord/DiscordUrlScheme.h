#pragma once

// Registers the discord-<app_id>:// scheme for the current user, so Discord can start
// the game when a friend clicks Join while it isn't running
void DiscordUrlScheme_Register(const char* app_id);
