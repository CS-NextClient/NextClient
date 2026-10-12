#pragma once

#include <cstdint>

#include <goldsrc/fwd.h>
#include <goldsrc/types.h>

namespace goldsrc
{
    inline constexpr int SIZEBUF_CHECK_OVERFLOW = 0x00000000;
    inline constexpr int SIZEBUF_ALLOW_OVERFLOW = 0x00000001;
    inline constexpr int SIZEBUF_OVERFLOWED = 0x00000002;

    enum clc_commands_e
    {
        clc_bad = 0,
        clc_nop = 1,
        clc_move = 2,
        clc_stringcmd = 3,
        clc_delta = 4,
        clc_resourcelist = 5,
        clc_tmove = 6,
        clc_fileconsistency = 7,
        clc_voicedata = 8,
        clc_hltv = 9,
        clc_cvarvalue = 10,
        clc_cvarvalue2 = 11,
        clc_endoflist = 255
    };

    enum netadrtype_t
    {
        NA_UNUSED = 0,
        NA_LOOPBACK = 1,
        NA_BROADCAST = 2,
        NA_IP = 3,
        NA_IPX = 4,
        NA_BROADCAST_IPX = 5
    };

    enum netsrc_t
    {
        NS_CLIENT = 0,
        NS_SERVER = 1,
        NS_MULTICAST = 2,
        NS_MAX = 3
    };

    enum
    {
        FRAG_NORMAL_STREAM = 0,
        FRAG_FILE_STREAM = 1,
        MAX_STREAMS = 2
    };

    struct downloadtime_t
    {
        qboolean bUsed;
        float fTime;
        int nBytesRemaining;
    };

    struct flowstats_t
    {
        int size;
        double time;
    };

    struct sizebuf_t
    {
        const char* buffername;
        uint16_t flags;
        uint8_t* data;
        int maxsize;
        int cursize;
    };

    struct flow_t
    {
        flowstats_t stats[32];
        int current;
        double nextcompute;
        float kbytespersec;
        float avgkbytespersec;
    };

    struct alignas(4) fragbuf_t
    {
        fragbuf_t* next;
        int bufferid;
        sizebuf_t frag_message;
        unsigned char unmodelled_[1708 - (8 + sizeof(frag_message))];
    };

    struct incomingtransfer_t
    {
        qboolean doneregistering;
        int percent;
        qboolean downloadrequested;
        downloadtime_t rgStats[8];
        int nCurStat;
        int nTotalSize;
        int nTotalToTransfer;
        int nRemainingToTransfer;
        float fLastStatusUpdate;
        qboolean custom;
    };

    struct netadr_t
    {
        netadrtype_t type;
        unsigned char ip[4];
        unsigned char ipx[10];
        unsigned short port;
    };

    struct netchan_t
    {
        netsrc_t sock;
        netadr_t remote_address;
        int player_slot;
        float last_received;
        float connect_time;
        double rate;
        double cleartime;
        int incoming_sequence;
        int incoming_acknowledged;
        int incoming_reliable_acknowledged;
        int incoming_reliable_sequence;
        int outgoing_sequence;
        int reliable_sequence;
        int last_reliable_sequence;
        void* connection_status;
        int (*pfnNetchan_Blocksize)(void*);
        sizebuf_t message;
        uint8_t message_buf[3990];
        int reliable_length;
        uint8_t reliable_buf[3990];
        fragbufwaiting_t* waitlist[2];
        int reliable_fragment[2];
        unsigned int reliable_fragid[2];
        fragbuf_t* fragbufs[2];
        int fragbufcount[2];
        short frag_startpos[2];
        short frag_length[2];
        fragbuf_t* incomingbufs[2];
        qboolean incomingready[2];
        char incomingfilename[260];
        void* tempbuffer;
        int tempbuffersize;
        flow_t flow[2];
    };
} // namespace goldsrc
