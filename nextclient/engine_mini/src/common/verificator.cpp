#include "verificator.h"
#include <format>
#include <tuple>
#include "../private_key.h"

#define LOGICAL_ASSERT(predicate, ...)             \
	if (!(predicate)) {                            \
		result = { std::format(__VA_ARGS__), {} }; \
		return false;                              \
	}

static EVP_PKEY* GetPrivateKey()
{
	BIO* bio = BIO_new_mem_buf(PRIVATE_KEY_DER, PRIVATE_KEY_DER_SIZE);
	RSA* rsa = d2i_RSAPrivateKey_bio(bio, nullptr);
	EVP_PKEY* pkey = EVP_PKEY_new();
	EVP_PKEY_assign_RSA(pkey, rsa);

	return pkey;
}

bool VerificatorEncrypt(std::vector<uint8_t> in, verificator_result_t& result)
{
	EVP_PKEY* key = GetPrivateKey();

	int res;
	auto ctx = EVP_PKEY_CTX_new(key, NULL);
	res = EVP_PKEY_encrypt_init(ctx);
	LOGICAL_ASSERT(res > 0, "Cannot initialize encrypt context (code {})", res);

	size_t maxOutputLen;
	res = EVP_PKEY_encrypt(ctx, NULL, &maxOutputLen, in.data(), in.size());
	LOGICAL_ASSERT(res > 0, "Cannot perform encrypt operation 1 (code {}, input len {})", res, in.size());

	auto output = &result.data;
	output->assign(maxOutputLen, 0x00);

	res = EVP_PKEY_encrypt(ctx, output->data(), &maxOutputLen, in.data(), in.size());
	LOGICAL_ASSERT(res > 0, "Cannot perform encrypt operation 2 (code {}, input len {})", res, in.size());

	return true;
}

bool VerificatorDecrypt(std::vector<uint8_t> in, verificator_result_t& result)
{
	EVP_PKEY* key = GetPrivateKey();

	int res;
	auto ctx = EVP_PKEY_CTX_new(key, NULL);
	res = EVP_PKEY_decrypt_init(ctx);
	LOGICAL_ASSERT(res > 0, "Cannot initialize decrypt context (code {})", res);

	size_t maxOutputLen;
	res = EVP_PKEY_decrypt(ctx, NULL, &maxOutputLen, in.data(), in.size());
	LOGICAL_ASSERT(res > 0, "Cannot perform decrypt operation 1 (code {}, input len {}, max {})", res, in.size(), maxOutputLen);

	auto output = &result.data;
	output->assign(maxOutputLen, 0x00);

	res = EVP_PKEY_decrypt(ctx, output->data(), &maxOutputLen, in.data(), in.size());
	LOGICAL_ASSERT(res > 0, "Cannot perform decrypt operation 2 (code {}, input len {}, max {})", res, in.size(), maxOutputLen);

	return true;
}

std::string VerificatorGetKeyVersion()
{
	return "";
}
