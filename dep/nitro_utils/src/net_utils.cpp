#include <string>
#include <algorithm>
#include <format>
#include <ws2tcpip.h>

#ifdef _WIN32
#include <Winsock2.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

namespace nitro_utils
{
    bool ParseAddress(const std::string& ip, uint32_t& nIP, uint16_t& nConnPort, bool bDnsResolve)
    {
        std::string host, port;
        uint32_t uHost, uPort;
        size_t colonPos = ip.find(':');

        if (colonPos == -1)
        {
            host = ip;
            uPort = 27015;
        }
        else
        {
            host = ip.substr(0, colonPos);
            port = ip.substr(colonPos + 1);
            if (!port.empty() && std::all_of(port.begin(), port.end(), ::isdigit))
                uPort = stoi(port);
            else uPort = 27015;
        }

        if (inet_pton(AF_INET, host.c_str(), &uHost) != TRUE)
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
