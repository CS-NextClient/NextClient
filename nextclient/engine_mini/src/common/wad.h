#pragma once

/// Copies a WAD lump name from in to out, lowercasing it and padding
/// with zeros to the length of lumpinfo_t::name (16 chars).
/// Names are padded so that lookups can compare 4 chars at a time
/// and names can be printed nicely in tables.
/// Can safely be performed in place (in == out).
void W_CleanupName(const char* in, char* out);

void* W_GetLumpName(int wad, const char *name);
