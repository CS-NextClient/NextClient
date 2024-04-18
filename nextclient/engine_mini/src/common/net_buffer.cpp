#include "net_buffer.h"

void MSG_Init(sizebuf_t *sb, const char *pBufName, void *pData, int nBytes)
{
    Q_memset(sb, 0, sizeof(sizebuf_t));
    sb->data = (byte*)pData;
    sb->cursize = 0;
    sb->buffername = pBufName;
    sb->maxsize = nBytes;
}
