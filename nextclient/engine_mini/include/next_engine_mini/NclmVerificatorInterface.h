#pragma once

class NclmVerificatorInterface
{
public:
    virtual ~NclmVerificatorInterface() = default;

    virtual void GetVersion(char* version_out, int size) = 0;
    virtual int DecryptPayload(const char* encrypted_payload, int size, char* decrypted_payload_out, int decrypted_max_size) = 0;
};
