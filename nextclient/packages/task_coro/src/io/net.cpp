#include <taskcoro/TaskCoro.h>
#include <chrono>

using namespace concurrencpp;
using namespace taskcoro;

namespace taskcoro::async_io
{
    static timeval timeout_to_timeval(std::chrono::microseconds timeout)
    {
        timeval tv{};
        tv.tv_sec = (long)std::chrono::duration_cast<std::chrono::seconds>(timeout).count();
        tv.tv_usec = (long)(timeout.count() - std::chrono::duration_cast<std::chrono::seconds>(timeout).count() * 1000000);
        return tv;
    }

    static std::tuple<SendAndRecvStatus, ByteBuffer> SendAndRecvInternal(
        SOCKET sock,
        std::chrono::milliseconds timeout,
        ByteBuffer send_data,
        sockaddr_in address,
        std::shared_ptr<CancellationToken> cancellation_token
    )
    {
        using clock = std::chrono::steady_clock;
        const auto quantum = std::chrono::milliseconds(50);

        int sent_bytes = sendto(sock, (const char*)send_data.GetBuffer(), (int)send_data.Size(), 0, (const sockaddr*)&address, sizeof(address));
        if (sent_bytes == SOCKET_ERROR)
        {
            return std::make_tuple(SendAndRecvStatus::Error, ByteBuffer{});
        }

        auto deadline = clock::now() + timeout;

        while (true)
        {
            if (cancellation_token)
            {
                cancellation_token->ThrowIfCancelled();
            }

            auto now = clock::now();
            if (now >= deadline)
            {
                return std::make_tuple(SendAndRecvStatus::Timeout, ByteBuffer{});
            }

            auto remain = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now);
            auto slice = std::min(remain, std::chrono::duration_cast<std::chrono::microseconds>(quantum));

            fd_set set;
            FD_ZERO(&set);
            FD_SET(sock, &set);

            timeval tv_remain = timeout_to_timeval(slice);

            int ready = select(0, &set, nullptr, nullptr, &tv_remain);
            if (ready == 0)
            {
                continue;
            }

            if (ready == SOCKET_ERROR)
            {
                return std::make_tuple(SendAndRecvStatus::Error, ByteBuffer{});
            }

            u_long pending = 0;
            bool pending_ok = ioctlsocket(sock, FIONREAD, &pending) == 0;
            size_t recv_capacity = 65535;

            if (pending_ok && pending > 0)
            {
                recv_capacity = (size_t)std::min<u_long>(pending, 65535);
            }

            ByteBuffer recv_buffer(recv_capacity);
            recv_buffer.Seek(0);

            sockaddr_in from_addr{};
            int from_len = sizeof(from_addr);

            int bytes_received = recvfrom(sock, (char*)recv_buffer.GetBuffer(), (int)recv_buffer.Size(), 0, (SOCKADDR*)&from_addr, &from_len);
            if (bytes_received < 0)
            {
                const int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK || err == WSAEINTR)
                {
                    continue;
                }

                return std::make_tuple(SendAndRecvStatus::Error, ByteBuffer{});
            }

            if (from_addr.sin_addr.s_addr != address.sin_addr.s_addr || from_addr.sin_port != address.sin_port)
            {
                continue;
            }

            recv_buffer.Resize((size_t)bytes_received);
            return std::make_tuple(SendAndRecvStatus::Success, std::move(recv_buffer));
        }
    }

    result<std::tuple<SendAndRecvStatus, ByteBuffer>> SendAndRecv(
        SOCKET sock,
        std::chrono::milliseconds timeout,
        ByteBuffer send_data,
        sockaddr_in address,
        std::shared_ptr<CancellationToken> cancellation_token
    )
    {
        return TaskCoro::RunIO(&SendAndRecvInternal, sock, timeout, std::move(send_data), address, std::move(cancellation_token));
    }
}
