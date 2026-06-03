#pragma once

// Makes a 4-byte "packed ID" int out of 4 characters
#define NCL_MAKEID(d,c,b,a)			              ( ((int)(a) << 24) | ((int)(b) << 16) | ((int)(c) << 8) | ((int)(d)) )

#define clc_ncl_message						       3   // clc_stringcmd
#define SVC_NCL_MESSAGE						       57  // SVC_SENDCVARVALUE
#define NCLM_HEADER_OLD						       NCL_MAKEID('n', 'c', 'l', 'a')
#define NCLM_HEADER							       NCL_MAKEID('n', 'c', 'l', 'm')

constexpr size_t RSA_KEY_LENGTH                    = 256;
constexpr size_t NCLM_VERIF_PAYLOAD_SIZE           = 196;
constexpr size_t NCLM_VERIF_PAYLOAD_SIZE_PADDED    = 256;
constexpr size_t NCLM_VERIF_ENCRYPTED_PAYLOAD_SIZE = ((NCLM_VERIF_PAYLOAD_SIZE / RSA_KEY_LENGTH) + 1) * RSA_KEY_LENGTH;

constexpr size_t NCLM_HWID_SIZE                    = 64; // SHA-256 hex
constexpr size_t NCLM_HWID_NONCE_BINDING_SIZE      = 32; // Recoverable signed message: raw HWID hex || SHA-256(verification nonce).
constexpr size_t NCLM_HWID_SIGNED_MESSAGE_SIZE     = NCLM_HWID_SIZE + NCLM_HWID_NONCE_BINDING_SIZE;
constexpr size_t NCLM_HWID_SIGNATURE_SIZE          = NCLM_VERIF_ENCRYPTED_PAYLOAD_SIZE;

enum class NCLM_C2S
{
    /*
        byte		Message header
        string		Prefered RSA public key version
    */
    VERIFICATION_REQUEST = 0x01,

    /*
        byte		Message header
        string		Client version in SemVer notation
        196 bytes	Decrypted message payload
    */
    VERIFICATION_RESPONSE,

    /*
     * Used to tell the server the version of NextClient in use
     * when the private key in the client is not configured.
     */
    DECLARE_VERSION_REQUEST,

    /*
     * HWID identification.
     * Only sent by verified clients (post RSA handshake).
     *
     * Payload:
     *   byte       Message header (this opcode = 0x04)
     *   256 bytes  RSA PKCS#1 v1.5 signature over a 96-byte message:
     *                64 bytes  lowercase hex SHA-256 of hardware identifiers
     *                32 bytes  SHA-256 of the verification nonce (replay binding)
     *
     * The server validates the signature and recovers the message from it.
     */
    HARDWARE_ID,
};

enum class NCLM_S2C
{
    /*
        byte		Message header
        256 bytes	Encrypted message payload
    */
    VERIFICATION_PAYLOAD,
};
