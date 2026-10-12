#pragma once

// Winsock's SOCKET/SOCKET_ERROR have no POSIX equivalent: BSD sockets are plain file
// descriptors (int), and -1 is used directly to signal an error.
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <netinet/in.h>
    using SOCKET = int;
    #define SOCKET_ERROR (-1)
#endif

#include <data_types/ByteBuffer.h>

namespace taskcoro::async_io
{
    enum class SendAndRecvStatus
    {
        Success,
        Timeout,
        Error,
    };

    concurrencpp::result<std::tuple<SendAndRecvStatus, ByteBuffer>> SendAndRecv(
        SOCKET sock,
        std::chrono::milliseconds timeout,
        ByteBuffer send_data,
        sockaddr_in address,
        std::shared_ptr<CancellationToken> cancellation_token = nullptr
    );
}
