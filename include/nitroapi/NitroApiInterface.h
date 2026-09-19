#pragma once

#if NO_HLSDK_ARCHTYPES
#include <steam/steamtypes.h>
#endif

#include <FileSystem.h>
#include <iregistry.h>
#include <icommandline.h>

#include "chain/HandlersChainInterface.h"
#include "NitroFunction.h"
#include "modules/engine/EngineData.h"
#include "modules/client/ClientData.h"
#include "modules/windows/WindowsData.h"
#include "modules/sdl2/SDL2Data.h"
#include "ChainExtensions.h"
#include "GlobalExceptionHandler.h"
#include "BuildVersion.h"

namespace nitroapi
{
    class NitroApiInterface : IBaseInterface
    {
    public:
        // writes log to nitro_api.log, feel free to use this function before initialization
        virtual void WriteLog(const char* tag, const char* message) = 0;
        // feel free to use this function before initialization
        virtual void SetSEHCallback(GlobalExceptionHandlerFunc callback) = 0;
        // feel free to use this function before initialization
        virtual GlobalExceptionHandlerFunc GetSEHCallback() = 0;

        virtual bool Initialize(ICommandLine *command_line, IFileSystem* file_system, IRegistry *registry) = 0;
        virtual bool UnInitialize() = 0;
        virtual bool IsInitialized() = 0;

        virtual ClientData* GetClientData() = 0;
        virtual EngineData* GetEngineData() = 0;
        virtual WindowsData* GetWindowsData() = 0;
        virtual SDL2Data* GetSDL2Data() = 0;
        virtual BuildVersion* GetBuildVersion() = 0;

        virtual void GetVersion(char* buffer, int size) = 0;
    };

#if NITRO_API_STATIC
    NitroApiInterface* GetNitroApi();
#endif

#define NITROAPI_INTERFACE_VERSION "NITRO_API_001"
}