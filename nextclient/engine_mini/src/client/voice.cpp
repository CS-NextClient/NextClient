// #include "../engine.h"
// #include <steam/steam_api.h>
// #include "../common/sys_dll.h"
//
// static uint32 Voice_GetCompressedData(char* pchDest, int nCount, bool bFinal)
// {
//     //  only steam voice supported
//     if (!g_bUsingSteamVoice)
//         return 0;
//
//     uint32 cbCompressedWritten = 0;
//     uint32 cbUncompressedWritten = 0;
//     uint32 cbCompressed = 0;
//     uint32 cbUncompressed = 0;
//
//     if (SteamUser()->GetAvailableVoice(&cbCompressed, cbUncompressed, g_pUncompressedFileData != 0 ? 11025 : 0) ||
//         SteamUser()->GetVoice(true,
//             pchDest,
//             nCount,
//             cbCompressedWritten,
//             g_pUncompressedFileData != 0,
//             g_pUncompressedFileData,
//             0x100000 - g_nUncompressedDataBytes,
//             cbUncompressedWritten,
//             11025))
//     {
//
//     }
//     else
//     {
//         g_nUncompressedDataBytes += cbUncompressedWritten;
//     }
//
//     return cbCompressedWritten;
// }
//
// static void Voice_AddIncomingData(int nChannel, const char* pchData, int nCount)
// {
//     double profile_start = 0;
//     char decompressed[8192];
//
//     if (g_bInTweakMode)
//     {
//         g_VoiceVolume = 0;
//         if (nChannel != TWEAKMODE_CHANNELINDEX)
//             return;
//
//         nChannel = 0;
//     }
//
//     if (!nCount)
//         return;
//
//     CVoiceChannel* channel = GetVoiceChannel(nChannel);
//     if (!channel)
//         return;
//
//     // channel + 22090
//     channel->m_bStarved = false; // This only really matters if you call Voice_AddIncomingData between the time the mixer
//                                  // asks for data and Voice_Idle is called.
//
//     if (voice_profile->value != 0.0)
//         profile_start = Sys_FloatTime();
//
//     int nDecompressed = 0;
//
//     if (g_bUsingSteamVoice)
//     {
//         uint32 nBytesWritten = 0;
//         EVoiceResult decompressResult = SteamUser()->DecompressVoice(pchData, nCount, decompressed, sizeof(decompressed), &nBytesWritten, 11025);
//         if (decompressResult != k_EVoiceResultOK)
//             return;
//
//         nDecompressed = nBytesWritten / BYTES_PER_SAMPLE;
//     }
//     else
//     {
//         // only steam voice supported
//         return;
//     }
//
//     if (g_bInTweakMode)
//     {
//         short* data = (short*)decompressed;
//
//         // Find the highest value
//         for (int i = 0; i < nDecompressed; ++i)
//             g_VoiceVolume = std::max(std::abs(data[i]), g_VoiceVolume);
//
//         // Smooth it out
//         g_VoiceVolume &= 0xFE00u;
//     }
//
//     if (voice_profile->value != 0.0)
//     {
//         g_DecompressTime = Sys_FloatTime() - profile_start + g_DecompressTime;
//         profile_start = Sys_FloatTime();
//     }
//
//     channel->m_AutoGain.ProcessSamples(decompressed, nDecompressed);
//
//     if (voice_profile->value != 0.0)
//     {
//         g_GainTime = Sys_FloatTime() - profile_start + g_GainTime;
//         profile_start = Sys_FloatTime();
//     }
//
//     // Upsample into the dest buffer. We could do this in a mixer but it complicates the mixer.
//     channel->m_LastFraction = UpsampleIntoBuffer(
//         decompressed,
//         nDecompressed,
//         &channel->m_Buffer,
//         channel->m_LastFraction,
//         *eng()->SampleRate / 11025.0);
//
//     channel->m_LastSample = decompressed[nDecompressed];
//
//     if (!voice_profile->value != 0.0)
//         g_UpsampleTime = Sys_FloatTime() - profile_start + g_UpsampleTime;
//
//     // Write to our file buffer..
//     if (g_pDecompressedFileData)
//     {
//         int nToWrite = std::min(nDecompressed * BYTES_PER_SAMPLE, MAX_WAVEFILEDATA_LEN - g_nDecompressedDataBytes);
//         std::memcpy(g_nDecompressedDataBytes[g_pDecompressedFileData], decompressed, nToWrite);
//         g_nDecompressedDataBytes += nToWrite;
//     }
//
//     // https://github.com/mastercomfig/tf2-patches/blob/adce75185fe5822309f356424ea449dee029e2d8/src/engine/audio/private/voice.cpp#L1298
//     // g_VoiceWriter.AddDecompressedData( pChannel, (const byte *)decompressed, nDecompressed * 2 );
//
//     if (voice_showincoming->value != 0.0)
//         Con_Printf("Voice - %d incoming samples added to channel %d\n", nDecompressed);
// }
//
// void CL_AddVoiceToDatagram(qboolean bFinal)
// {
//     if (cls->state != ca_active || !Voice_IsRecording())
//         return;
//
//     char uchVoiceData[2048];
//     uint32 iSize = Voice_GetCompressedData(uchVoiceData, sizeof(uchVoiceData), bFinal);
//
//     if (iSize > 0 && iSize + cls->datagram.cursize + 3 < cls->datagram.maxsize)
//     {
//         MSG_WriteByte(&cls->datagram, 8);
//         MSG_WriteShort(&cls->datagram, (int) iSize);
//         MSG_WriteBuf(&cls->datagram, (int) iSize, uchVoiceData);
//     }
// }
//
// void CL_Parse_VoiceData()
// {
//     char chReceived[8192];
//
//     int startReadCount = *pMsg_readcount;
//
//     int client = MSG_ReadByte() + 1;
//     int iSize = MSG_ReadShort();
//     if (iSize > sizeof(chReceived))
//         iSize = sizeof(chReceived);
//     MSG_ReadBuf(iSize, chReceived);
//
//     cl->frames[cl->parsecountmod].voicebytes += msg_readcount - startReadCount;
//
//     if (client >= 0 && client < cl->num_entities)
//     {
//         if (cl->playernum + 1 == client)
//             Voice_LocalPlayerTalkingAck();
//
//         if (iSize)
//         {
//             int channel = Voice_GetChannel(client);
//
//             if (channel == -1 && (channel = Voice_AssignChannel(client), channel == -1))
//                 Con_DPrintf("CL_ParseVoiceData: Voice_AssignChannel failed for client %d!\n", client - 1);
//             else
//                 Voice_AddIncomingData(channel, chReceived, iSize);
//         }
//     }
// }
