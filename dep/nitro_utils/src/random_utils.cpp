#include <vector>
#include <stdexcept>

#ifdef _WIN32
#include <Windows.h>
#include <wincrypt.h>
#endif

namespace nitro_utils
{
#ifdef _WIN32
    std::vector<uint8_t> GenerateRandomBytesSecure(size_t count)
    {
        HCRYPTPROV hCryptProv;

        if (!CryptAcquireContext(
            &hCryptProv,
            NULL,
            TEXT("Microsoft Base Cryptographic Provider v1.0"),
            PROV_RSA_FULL,
            CRYPT_VERIFYCONTEXT))
        {
            throw std::runtime_error("Can't acquire crypto provider");
        }

        std::vector<uint8_t> bytes(count);
        if (!CryptGenRandom(hCryptProv, count, bytes.data()))
        {
            throw std::runtime_error("Can't generate random bytes");
        }

        return bytes;
    }
#endif
}