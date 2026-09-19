#pragma once

#include "NitroApiInterface.h"
#include <vector>
#include <memory>
#include <utility>
#include <goldsrc/client_api.h>
#include <goldsrc/engine/client_state.h>
#include <goldsrc/engine/net.h>
#include <goldsrc/server_api.h>

namespace nitroapi
{
    class NitroApiHelper
    {
        NitroApiInterface* nitro_api_;
        std::vector<std::shared_ptr<Unsubscriber>> unsubscribers_;

    public:
        explicit NitroApiHelper(NitroApiInterface* nitro_api) : nitro_api_(nitro_api) { }

        virtual ~NitroApiHelper() {
            for (auto& unsubscriber : unsubscribers_)
                unsubscriber->Unsubscribe();
        }

        void DeferUnsub(std::shared_ptr<Unsubscriber> unsubscriber) {
		    unsubscribers_.push_back(std::move(unsubscriber));
	    }

        // ----- NitroApi data -----

        [[nodiscard]] NitroApiInterface* api() { return nitro_api_; };

        [[nodiscard]] EngineData* eng() { return nitro_api_->GetEngineData(); };
        [[nodiscard]] ClientData* cl() { return nitro_api_->GetClientData(); };

        // ----- Engine variables -----

        [[nodiscard]] goldsrc::client_static_t* cls()
        {
            return eng()->client_static;
        };
        [[nodiscard]] goldsrc::client_state_t* client_state()
        {
            return eng()->client_state;
        };
        [[nodiscard]] goldsrc::cl_enginefunc_t* cl_enginefunc()
        {
            return eng()->cl_enginefunc;
        };
        [[nodiscard]] goldsrc::enginefuncs_t* enginefuncs()
        {
            return eng()->enginefuncs;
        };
        [[nodiscard]] goldsrc::cldll_func_t* cldll_func()
        {
            return eng()->cldll_func;
        };
        [[nodiscard]] goldsrc::modfuncs_t* modfuncs()
        {
            return eng()->modfuncs;
        };

        [[nodiscard]] int* msg_readcount() { return eng()->msg_readcount; };
        [[nodiscard]] goldsrc::sizebuf_t* net_message()
        {
            return eng()->net_message;
        };

        // ----- Client variables -----

        [[nodiscard]] gamehud_t* gHUD() { return cl()->gHUD; };

        // ----- Engine common functions -----

        [[nodiscard]] float GetCvarFloat(const char* cvar) { return eng()->cl_enginefunc->pfnGetCvarFloat(cvar); };
        [[nodiscard]] const char* GetCvarString(const char* cvar) { return eng()->cl_enginefunc->pfnGetCvarString(cvar); };

        template<class... TArgs>
        char* Con_Printf(const char* format, TArgs&&... args) { return eng()->Con_Printf.GetFunc()(format, std::forward<TArgs>(args)...); };

        template<class... TArgs>
        char* Con_DPrintf(const char* format, TArgs&&... args) { return eng()->Con_DPrintf.GetFunc()(format, std::forward<TArgs>(args)...); };
    };
}
