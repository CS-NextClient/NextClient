#pragma once

#include <cstdint>

#include <goldsrc/fwd.h>
#include <goldsrc/types.h>

namespace goldsrc
{
    enum ALERT_TYPE
    {
        at_notice = 0,
        at_console = 1,
        at_aiconsole = 2,
        at_warning = 3,
        at_error = 4,
        at_logged = 5
    };

    enum FORCE_TYPE
    {
        force_exactfile = 0,
        force_model_samebounds = 1,
        force_model_specifybounds = 2,
        force_model_specifybounds_if_avail = 3
    };

    enum PRINT_TYPE
    {
        print_console = 0,
        print_center = 1,
        print_chat = 2
    };

    struct enginefuncs_t
    {
        int (*pfnPrecacheModel)(const char*);
        int (*pfnPrecacheSound)(const char*);
        void (*pfnSetModel)(edict_t*, const char*);
        int (*pfnModelIndex)(const char*);
        int (*pfnModelFrames)(int);
        void (*pfnSetSize)(edict_t*, const float*, const float*);
        void (*pfnChangeLevel)(const char*, const char*);
        void (*pfnGetSpawnParms)(edict_t*);
        void (*pfnSaveSpawnParms)(edict_t*);
        float (*pfnVecToYaw)(const float*);
        void (*pfnVecToAngles)(const float*, float*);
        void (*pfnMoveToOrigin)(edict_t*, const float*, float, int);
        void (*pfnChangeYaw)(edict_t*);
        void (*pfnChangePitch)(edict_t*);
        edict_t* (*pfnFindEntityByString)(edict_t*, const char*, const char*);
        int (*pfnGetEntityIllum)(edict_t*);
        edict_t* (*pfnFindEntityInSphere)(edict_t*, const float*, float);
        edict_t* (*pfnFindClientInPVS)(edict_t*);
        edict_t* (*pfnEntitiesInPVS)(edict_t*);
        void (*pfnMakeVectors)(const float*);
        void (*pfnAngleVectors)(const float*, float*, float*, float*);
        edict_t* (*pfnCreateEntity)();
        void (*pfnRemoveEntity)(edict_t*);
        edict_t* (*pfnCreateNamedEntity)(int);
        void (*pfnMakeStatic)(edict_t*);
        int (*pfnEntIsOnFloor)(edict_t*);
        int (*pfnDropToFloor)(edict_t*);
        int (*pfnWalkMove)(edict_t*, float, float, int);
        void (*pfnSetOrigin)(edict_t*, const float*);
        void (*pfnEmitSound)(edict_t*, int, const char*, float, float, int, int);
        void (*pfnEmitAmbientSound)(edict_t*, float*, const char*, float, float, int, int);
        void (*pfnTraceLine)(const float*, const float*, int, edict_t*, TraceResult*);
        void (*pfnTraceToss)(edict_t*, edict_t*, TraceResult*);
        int (*pfnTraceMonsterHull)(edict_t*, const float*, const float*, int, edict_t*, TraceResult*);
        void (*pfnTraceHull)(const float*, const float*, int, int, edict_t*, TraceResult*);
        void (*pfnTraceModel)(const float*, const float*, int, edict_t*, TraceResult*);
        const char* (*pfnTraceTexture)(edict_t*, const float*, const float*);
        void (*pfnTraceSphere)(const float*, const float*, int, float, edict_t*, TraceResult*);
        void (*pfnGetAimVector)(edict_t*, float, float*);
        void (*pfnServerCommand)(const char*);
        void (*pfnServerExecute)();
        void (*pfnClientCommand)(edict_t*, const char*, ...);
        void (*pfnParticleEffect)(const float*, const float*, float, float);
        void (*pfnLightStyle)(int, const char*);
        int (*pfnDecalIndex)(const char*);
        int (*pfnPointContents)(const float*);
        void (*pfnMessageBegin)(int, int, const float*, edict_t*);
        void (*pfnMessageEnd)();
        void (*pfnWriteByte)(int);
        void (*pfnWriteChar)(int);
        void (*pfnWriteShort)(int);
        void (*pfnWriteLong)(int);
        void (*pfnWriteAngle)(float);
        void (*pfnWriteCoord)(float);
        void (*pfnWriteString)(const char*);
        void (*pfnWriteEntity)(int);
        void (*pfnCVarRegister)(cvar_t*);
        float (*pfnCVarGetFloat)(const char*);
        const char* (*pfnCVarGetString)(const char*);
        void (*pfnCVarSetFloat)(const char*, float);
        void (*pfnCVarSetString)(const char*, const char*);
        void (*pfnAlertMessage)(ALERT_TYPE, const char*, ...);
        void (*pfnEngineFprintf)(void*, const char*, ...);
        void* (*pfnPvAllocEntPrivateData)(edict_t*, int32_t);
        void* (*pfnPvEntPrivateData)(edict_t*);
        void (*pfnFreeEntPrivateData)(edict_t*);
        const char* (*pfnSzFromIndex)(int);
        int (*pfnAllocString)(const char*);
        entvars_s* (*pfnGetVarsOfEnt)(edict_t*);
        edict_t* (*pfnPEntityOfEntOffset)(int);
        int (*pfnEntOffsetOfPEntity)(const edict_t*);
        int (*pfnIndexOfEdict)(const edict_t*);
        edict_t* (*pfnPEntityOfEntIndex)(int);
        edict_t* (*pfnFindEntityByVars)(entvars_s*);
        void* (*pfnGetModelPtr)(edict_t*);
        int (*pfnRegUserMsg)(const char*, int);
        void (*pfnAnimationAutomove)(const edict_t*, float);
        void (*pfnGetBonePosition)(const edict_t*, int, float*, float*);
        uint32_t (*pfnFunctionFromName)(const char*);
        const char* (*pfnNameForFunction)(uint32_t);
        void (*pfnClientPrintf)(edict_t*, PRINT_TYPE, const char*);
        void (*pfnServerPrint)(const char*);
        const char* (*pfnCmd_Args)();
        const char* (*pfnCmd_Argv)(int);
        int (*pfnCmd_Argc)();
        void (*pfnGetAttachment)(const edict_t*, int, float*, float*);
        void (*pfnCRC32_Init)(CRC32_t*);
        void (*pfnCRC32_ProcessBuffer)(CRC32_t*, void*, int);
        void (*pfnCRC32_ProcessByte)(CRC32_t*, unsigned char);
        CRC32_t (*pfnCRC32_Final)(CRC32_t);
        int32_t (*pfnRandomLong)(int32_t, int32_t);
        float (*pfnRandomFloat)(float, float);
        void (*pfnSetView)(const edict_t*, const edict_t*);
        float (*pfnTime)();
        void (*pfnCrosshairAngle)(const edict_t*, float, float);
        uint8_t* (*pfnLoadFileForMe)(const char*, int*);
        void (*pfnFreeFile)(void*);
        void (*pfnEndSection)(const char*);
        int (*pfnCompareFileTime)(char*, char*, int*);
        void (*pfnGetGameDir)(char*);
        void (*pfnCvar_RegisterVariable)(cvar_t*);
        void (*pfnFadeClientVolume)(const edict_t*, int, int, int, int);
        void (*pfnSetClientMaxspeed)(edict_t*, float);
        edict_t* (*pfnCreateFakeClient)(const char*);
        void (*pfnRunPlayerMove)(edict_t*, const float*, float, float, float, unsigned short, uint8_t, uint8_t);
        int (*pfnNumberOfEntities)();
        char* (*pfnGetInfoKeyBuffer)(edict_t*);
        char* (*pfnInfoKeyValue)(char*, const char*);
        void (*pfnSetKeyValue)(char*, const char*, const char*);
        void (*pfnSetClientKeyValue)(int, char*, const char*, const char*);
        int (*pfnIsMapValid)(const char*);
        void (*pfnStaticDecal)(const float*, int, int, int);
        int (*pfnPrecacheGeneric)(const char*);
        int (*pfnGetPlayerUserId)(edict_t*);
        void (*pfnBuildSoundMsg)(edict_t*, int, const char*, float, float, int, int, int, int, const float*, edict_t*);
        int (*pfnIsDedicatedServer)();
        cvar_t* (*pfnCVarGetPointer)(const char*);
        unsigned int (*pfnGetPlayerWONId)(edict_t*);
        void (*pfnInfo_RemoveKey)(char*, const char*);
        const char* (*pfnGetPhysicsKeyValue)(const edict_t*, const char*);
        void (*pfnSetPhysicsKeyValue)(const edict_t*, const char*, const char*);
        const char* (*pfnGetPhysicsInfoString)(const edict_t*);
        unsigned short (*pfnPrecacheEvent)(int, const char*);
        void (*pfnPlaybackEvent)(int, const edict_t*, unsigned short, float, float*, float*, float, float, int, int, int, int);
        unsigned char* (*pfnSetFatPVS)(float*);
        unsigned char* (*pfnSetFatPAS)(float*);
        int (*pfnCheckVisibility)(edict_t*, unsigned char*);
        void (*pfnDeltaSetField)(delta_t*, const char*);
        void (*pfnDeltaUnsetField)(delta_t*, const char*);
        void (*pfnDeltaAddEncoder)(const char*, void (*)(delta_t*, const unsigned char*, const unsigned char*));
        int (*pfnGetCurrentPlayer)();
        int (*pfnCanSkipPlayer)(const edict_t*);
        int (*pfnDeltaFindField)(delta_t*, const char*);
        void (*pfnDeltaSetFieldByIndex)(delta_t*, int);
        void (*pfnDeltaUnsetFieldByIndex)(delta_t*, int);
        void (*pfnSetGroupMask)(int, int);
        int (*pfnCreateInstancedBaseline)(int, entity_state_t*);
        void (*pfnCvar_DirectSet)(cvar_t*, const char*);
        void (*pfnForceUnmodified)(FORCE_TYPE, float*, float*, const char*);
        void (*pfnGetPlayerStats)(const edict_t*, int*, int*);
        void (*pfnAddServerCommand)(const char*, void (*)());
        qboolean (*pfnVoice_GetClientListening)(int, int);
        qboolean (*pfnVoice_SetClientListening)(int, int, qboolean);
        const char* (*pfnGetPlayerAuthId)(edict_t*);
        sequenceEntry_s* (*pfnSequenceGet)(const char*, const char*);
        sentenceEntry_s* (*pfnSequencePickSentence)(const char*, int, int*);
        int (*pfnGetFileSize)(const char*);
        unsigned int (*pfnGetApproxWavePlayLen)(const char*);
        int (*pfnIsCareerMatch)();
        int (*pfnGetLocalizedStringLength)(const char*);
        void (*pfnRegisterTutorMessageShown)(int);
        int (*pfnGetTimesTutorMessageShown)(int);
        void (*pfnProcessTutorMessageDecayBuffer)(int*, int);
        void (*pfnConstructTutorMessageDecayBuffer)(int*, int);
        void (*pfnResetTutorMessageDecayData)();
        void (*pfnQueryClientCvarValue)(const edict_t*, const char*);
        void (*pfnQueryClientCvarValue2)(const edict_t*, const char*, int);
        int (*pfnEngCheckParm)(const char*, char**);
    };
} // namespace goldsrc
