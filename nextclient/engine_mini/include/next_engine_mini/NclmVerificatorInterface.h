#pragma once
#include <cstdint>

class NclmVerificatorInterface
{
public:
    virtual ~NclmVerificatorInterface() = default;

    virtual void GetVersion(char* version_out, int size) = 0;

    virtual size_t DecryptPayload(
        const uint8_t* encrypted_payload,
        size_t size,
        uint8_t* decrypted_payload_out,
        size_t decrypted_max_size
    ) = 0;
};
