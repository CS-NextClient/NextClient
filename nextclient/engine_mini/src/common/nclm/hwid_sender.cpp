// =============================================================================
// hwid_sender.cpp  --  zSteam HWID System (client-side)
// =============================================================================
// Envia o HWID ao servidor via protocolo NCLM usando o mesmo canal reliable
// (cls->netchan.message) utilizado por VERIFICATION_RESPONSE.
//
// Formato do pacote:
//   byte        clc_ncl_message     (= 3)
//   int32       NCLM_HEADER         ('nclm')
//   [NclmBodyWriter payload]
//     byte      NCLM_C2S::HARDWARE_ID  (= 0x04)
//     string    hwid hex string (64 chars + null)
// =============================================================================

#include "hwid_sender.h"
#include "hwid_collector.h"
#include "NclmBodyWriter.h"

#include <next_engine_mini/nclm_proto.h>
#include <common.h>   // MSG_WriteByte, MSG_WriteLong

namespace hwid
{
    bool SendToServer(sizebuf_t* msgbuf)
    {
        if (!msgbuf)
            return false;

        const std::string& hwidStr = hwid::Collect();
        if (hwidStr.size() != 64)
            return false;

        MSG_WriteByte(msgbuf, clc_ncl_message);
        MSG_WriteLong(msgbuf, NCLM_HEADER);

        NclmBodyWriter writer(msgbuf);
        writer.WriteByte(static_cast<uint8_t>(NCLM_C2S::HARDWARE_ID));
        writer.WriteString(hwidStr.c_str());
        writer.Send();

        return true;
    }

}
