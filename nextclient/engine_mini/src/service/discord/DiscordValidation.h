#pragma once

#include <string>

// Whether a join secret from Discord is safe to put into a "connect" command:
// letters, digits, '.', ':' and '-' only, 1-63 characters
bool IsSafeServerAddress(const char* address);

// Whether text is well-formed UTF-8; Discord rejects the whole activity over a broken string
bool IsValidUtf8(const std::string& text);
