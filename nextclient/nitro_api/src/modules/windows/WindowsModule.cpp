#include <utility>
#include "WindowsModule.h"

namespace nitroapi
{
    WindowsModule::WindowsModule(std::shared_ptr<HookStorage> hook_storage,
                                 std::shared_ptr<AddressProviderBase> addr_provider) :
        ModuleBase(std::move(hook_storage), std::move(addr_provider))
    {
        windows_data_ = std::make_shared<WindowsData>();

#ifdef _WIN32
        RegisterFuncStdcall<StaticHookId::LoadLibraryA>(&windows_data_->LoadLibraryAFunc, [](nitro_utils::SysModule hModule){ return (uint32_t)LoadLibraryA; });
        RegisterFuncStdcall<StaticHookId::LoadLibraryW>(&windows_data_->LoadLibraryWFunc, [](nitro_utils::SysModule hModule){ return (uint32_t)LoadLibraryW; });
        RegisterFuncStdcall<StaticHookId::FreeLibrary>(&windows_data_->FreeLibraryFunc, [](nitro_utils::SysModule hModule) { return (uint32_t)FreeLibrary; });
        RegisterFuncStdcall<StaticHookId::SetCursor>(&windows_data_->SetCursorFunc, [](nitro_utils::SysModule hModule) { return (uint32_t)SetCursor; });
#endif
    }
}