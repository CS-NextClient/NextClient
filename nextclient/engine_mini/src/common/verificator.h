#pragma once

#include <vector>
#include <string>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>

struct verificator_result_t
{
    std::string error;
    std::vector<uint8_t> data;
};

std::string VerificatorGetKeyVersion();
bool VerificatorEncrypt(std::vector<uint8_t> in, verificator_result_t& result);
bool VerificatorDecrypt(std::vector<uint8_t> in, verificator_result_t& result);
