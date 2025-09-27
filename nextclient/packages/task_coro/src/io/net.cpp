#include <taskcoro/TaskCoro.h>

using namespace concurrencpp;
using namespace taskcoro;

namespace taskcoro::async_io
{
    static ByteBuffer SendAndRecvInternal(SOCKET sock, timeval timeout, ByteBuffer send_data, sockaddr_in address)
    {
        int sent_bytes = sendto(sock, (const char*)send_data.GetBuffer(), (int)send_data.Size(), 0, (const sockaddr*)&address, sizeof(address));
        if (sent_bytes == SOCKET_ERROR)
        {
            //LOG(ERROR) <<  log_tag << " sendto completed with SOCKET_ERROR: " << WSAGetLastError();
            return {};
        }

        fd_set set;
        FD_ZERO(&set);
        FD_SET(sock, &set);

        int ready = select(0, &set, nullptr, nullptr, &timeout);
        if (ready > 0)
        {
            ByteBuffer recv_buffer(65535);
            recv_buffer.Seek(0);

            sockaddr_in from_addr {};
            int from_len = sizeof(from_addr);

            int bytes_received = recvfrom(sock, (char*)recv_buffer.GetBuffer(), (int)recv_buffer.Size(), 0, (SOCKADDR*)&from_addr, &from_len);
            if (bytes_received <= 0)
            {
                return {};
            }

            recv_buffer.Resize(bytes_received);
            return recv_buffer;
        }

        if (ready == SOCKET_ERROR)
        {
            //LOG(ERROR) << log_tag << " select completed with SOCKET_ERROR: " << WSAGetLastError();
            return {};
        }

        return {};
    }

    result<ByteBuffer> SendAndRecv(SOCKET sock, timeval timeout, const ByteBuffer& send_data, sockaddr_in address)
    {
        return TaskCoro::RunIO(&SendAndRecvInternal, sock, timeout, send_data, address);
    }
}
