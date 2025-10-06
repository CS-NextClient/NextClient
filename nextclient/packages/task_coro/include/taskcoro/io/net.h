#pragma once
#include <winsock2.h>
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
