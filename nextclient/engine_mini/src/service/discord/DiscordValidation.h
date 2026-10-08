#pragma once

// Whether a join secret from Discord is an IPv4 address with a port, the only form we publish
bool Discord_IsSafeJoinAddress(const char* address);
