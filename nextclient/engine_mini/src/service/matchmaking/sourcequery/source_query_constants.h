#pragma once

#define SQ_SIMPLE_PACKET_MAX_SIZE 1400

#define SQ_HEADER_SIMPLE 0xFFFFFFFF
#define SQ_HEADER_MULTI 0xFFFFFFFE

#define S2C_CHALLENGE 'A'
#define S2A_PLAYER 'D'
#define S2A_RULES 'E'
#define S2A_INFO 'I'
#define S2A_LOG 'R'
#define S2A_RULES_GS 'm'

#define A2S_INFO 'T'
#define A2S_INFO_PAYLOAD "Source Engine Query"
#define A2S_PLAYER 'U'
#define A2S_RULES 'V'

#define A2S_REQUEST_CHALLENGE 0xFFFFFFFF
