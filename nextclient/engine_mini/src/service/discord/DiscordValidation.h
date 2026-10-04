#pragma once

// Whether a join secret from Discord is safe to put into a "connect" command:
// letters, digits, '.', ':' and '-' only, 1-63 characters
bool IsSafeServerAddress(const char* address);
