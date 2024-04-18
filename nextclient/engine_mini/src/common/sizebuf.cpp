#include "../engine.h"

void SZ_Clear(sizebuf_t* buf)
{
    eng()->SZ_Clear.InvokeChained(buf);
}

void SZ_Write(sizebuf_t* buf, const void* data, int length)
{
    eng()->SZ_Write.InvokeChained(buf, (char*) data, length);
}

void* SZ_GetSpace(sizebuf_t *buf, int length)
{
    return eng()->SZ_GetSpace.InvokeChained(buf, length);
}
