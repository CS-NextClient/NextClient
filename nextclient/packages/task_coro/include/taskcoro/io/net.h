#pragma once
#include <winsock2.h>
#include <data_types/ByteBuffer.h>

namespace taskcoro::async_io
{
    concurrencpp::result<ByteBuffer> SendAndRecv(SOCKET sock, timeval timeout, const ByteBuffer& send_data, sockaddr_in address);
}
