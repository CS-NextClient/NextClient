#define DIB_HEADER_MARKER ((WORD)('M' << 8) | 'B')

int LoadBMP(const char *szFilename, unsigned char *buffer, int bufferSize, int *width, int *height);

int GetBMPSize(const char *szFilename, int *width, int *height);