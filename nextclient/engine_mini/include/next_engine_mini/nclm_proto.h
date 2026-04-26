#pragma once

// Makes a 4-byte "packed ID" int out of 4 characters
#define NCL_MAKEID(d,c,b,a)					( ((int)(a) << 24) | ((int)(b) << 16) | ((int)(c) << 8) | ((int)(d)) )

#define clc_ncl_message						3			// clc_stringcmd
#define SVC_NCL_MESSAGE						57			// SVC_SENDCVARVALUE
#define NCLM_HEADER_OLD						NCL_MAKEID('n', 'c', 'l', 'a')
#define NCLM_HEADER							NCL_MAKEID('n', 'c', 'l', 'm')

constexpr size_t RSA_KEY_LENGTH =			256;
constexpr size_t NCLM_VERIF_PAYLOAD_SIZE =	196;
constexpr size_t NCLM_VERIF_ENCRYPTED_PAYLOAD_SIZE = ((NCLM_VERIF_PAYLOAD_SIZE / RSA_KEY_LENGTH) + 1) * RSA_KEY_LENGTH;

// Tamanho do payload HWID: SHA-256 em hex = 64 caracteres ASCII + null terminator.
constexpr size_t NCLM_HWID_SIZE = 64;

enum class NCLM_C2S {
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
     * HWID identification — sent AFTER RSA verification is complete.
     *
     * Payload:
     *   byte      Message header (this opcode = 0x04)
     *   string    64-char lowercase hex SHA-256 of hardware identifiers
     *
     * Clients that do not support HWID will never send this opcode.
     * The server MUST NOT kick clients that omit it — treat them as "no HWID".
     */
    HARDWARE_ID                     // = 0x04  (sequential after DECLARE_VERSION_REQUEST = 0x03)
};

enum class NCLM_S2C {
    /*
        byte		Message header
        256 bytes	Encrypted message payload
    */
    VERIFICATION_PAYLOAD
};
