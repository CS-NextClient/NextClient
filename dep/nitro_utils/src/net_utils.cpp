#include <string>
#include <charconv>
#include <format>
#include <string_view>

#ifdef _WIN32
#include <Winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

namespace nitro_utils
{
    namespace
    {
        constexpr uint16_t kDefaultPort = 27015;

        // kDefaultPort for text that is not a whole port number, a number above 65535 included
        uint16_t ParsePort(std::string_view text)
        {
            uint16_t port;
            std::from_chars_result read = std::from_chars(text.data(), text.data() + text.size(), port);

            if (read.ec != std::errc{} || read.ptr != text.data() + text.size())
            {
                return kDefaultPort;
            }

            return port;
        }
    }

    bool ParseAddress(const std::string& ip, uint32_t& nIP, uint16_t& nConnPort, bool bDnsResolve)
    {
        std::string host;
        uint32_t uHost;
        uint16_t uPort = kDefaultPort;
        size_t colonPos = ip.find(':');

        if (colonPos == std::string::npos)
        {
            host = ip;
        }
        else
        {
            host = ip.substr(0, colonPos);
            uPort = ParsePort(std::string_view(ip).substr(colonPos + 1));
        }

        if (inet_pton(AF_INET, host.c_str(), &uHost) != 1)
        {
            uHost = INADDR_NONE;
        }

        if (bDnsResolve && uHost == INADDR_NONE)
        {
            struct hostent* h = gethostbyname(host.c_str());
            if (h == nullptr) return false;
            uHost = *(uint32_t*) h->h_addr_list[0];
        }

        if (uHost != INADDR_NONE)
        {
            nIP = ntohl(uHost);
            nConnPort = uPort;

            return true;
        }
        return false;
    }

}
