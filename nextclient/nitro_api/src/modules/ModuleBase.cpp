#include "ModuleBase.h"

#include <utility>

namespace nitroapi
{
    ModuleBase::ModuleBase(std::shared_ptr<HookStorage> hook_storage, std::shared_ptr<AddressProviderBase> addr_provider) :
        hook_storage_(std::move(hook_storage)),
        addr_provider_(std::move(addr_provider))
    { }

    void ModuleBase::OnLibraryLoaded(nitro_utils::SysModule hModule)
    {
        for (auto& func : on_library_loaded_)
            func(hModule);

        on_library_loaded_.clear();
    }

    void ModuleBase::OnLibraryUnload()
    {
        for (auto& func : on_library_unloaded_)
            func();

        on_library_unloaded_.clear();
    }
}