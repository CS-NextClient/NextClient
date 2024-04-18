#include "../engine.h"

void MSG_BeginReading()
{
    eng()->MSG_BeginReading.InvokeChained();
}

const char* MSG_ReadString()
{
    return eng()->MSG_ReadString.InvokeChained();
}

const char* MSG_ReadStringLine()
{
    return eng()->MSG_ReadStringLine.InvokeChained();
}

void MSG_WriteByte(sizebuf_t *sb, int c)
{
    eng()->MSG_WriteByte.InvokeChained(sb, c);
}

void MSG_WriteShort(sizebuf_t *sb, int c)
{
    eng()->MSG_WriteShort.InvokeChained(sb, c);
}

void MSG_WriteLong(sizebuf_t *sb, int c)
{
    uint32* data = reinterpret_cast<uint32*>(SZ_GetSpace(sb, 4));
    *data = c;
}

void MSG_WriteString(sizebuf_t *sb, const char *s)
{
    eng()->MSG_WriteString.InvokeChained(sb, s);
}

void MSG_WriteBuf(sizebuf_t *sb, int iSize, void *buf)
{
    if (buf)
        SZ_Write(sb, buf, iSize);
}

int MSG_ReadByte()
{
    return eng()->MSG_ReadByte.InvokeChained();
}

int MSG_ReadLong()
{
    return eng()->MSG_ReadLong.InvokeChained();
}

float MSG_ReadFloat()
{
    return eng()->MSG_ReadFloat.InvokeChained();
}
