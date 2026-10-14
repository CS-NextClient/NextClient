#pragma once

void ColorChatInConsolePatch();
// Display through the local SayText handler while suppressing its console echo.
int PrintLocalChat(int (*handler)(const char*, int, void*), const char* text);
void PrintPluginConsole(const char* text);
