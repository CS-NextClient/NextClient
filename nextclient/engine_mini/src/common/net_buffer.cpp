#include "net_buffer.h"

#include <cstdint>

#include <goldsrc/engine/net.h>

void MSG_Init(goldsrc::sizebuf_t* sb, const char* pBufName, void* pData, int nBytes)
{
    Q_memset(sb, 0, sizeof(goldsrc::sizebuf_t));
    sb->data = (uint8_t*)pData;
    sb->cursize = 0;
    sb->buffername = pBufName;
    sb->maxsize = nBytes;
}
