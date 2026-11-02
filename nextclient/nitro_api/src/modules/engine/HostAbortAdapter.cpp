#include "HostAbortAdapter.h"

#include <cstdio>
#include <exception>
#include <new>
#include <stdexcept>
#include <string>

#include <funchook.h>
#include <xbyak/xbyak.h>

#include "FrameLifecycle.h"

namespace nitroapi
{
    namespace
    {
        void ReportAbort(FrameLifecycleService* service) noexcept
        {
            service->ReportAbort();
        }

        void RequireSuccess(int status, const char* operation) noexcept
        {
            if (status != FUNCHOOK_ERROR_SUCCESS)
            {
                std::fprintf(stderr, "Host abort adapter %s failed: %d\n", operation, status);
                std::terminate();
            }
        }
    } // namespace

    class HostAbortAdapter::Entry final : public Xbyak::CodeGenerator
    {
    private:
        funchook_t* hook_{};
        void* trampoline_{};

    public:
        Entry(FrameLifecycleService& service, void* target) :
            trampoline_(target)
        {
            static_assert(sizeof(void*) == 4);

            pushfd();
            pushad();
            mov(ebp, esp);

            and_(esp, -16);
            sub(esp, 12);
            push(reinterpret_cast<uintptr_t>(&service));
            mov(eax, reinterpret_cast<uintptr_t>(&ReportAbort));
            call(eax);

            mov(esp, ebp);
            popad();
            popfd();

            // Preserve the original return address and the complete variadic stack.
            jmp(ptr[reinterpret_cast<uintptr_t>(&trampoline_)]);
            ready();

            hook_ = funchook_create();
            if (!hook_)
            {
                throw std::bad_alloc();
            }

            int status = funchook_prepare(hook_, &trampoline_, const_cast<uint8_t*>(getCode()));
            if (status != FUNCHOOK_ERROR_SUCCESS)
            {
                RequireSuccess(funchook_destroy(hook_), "destroy after prepare");
                throw std::runtime_error("Host abort adapter prepare failed: " + std::to_string(status));
            }

            // Installation can partially modify code; failure must retain executable storage.
            RequireSuccess(funchook_install(hook_, 0), "install");
        }

        ~Entry()
        {
            RequireSuccess(funchook_uninstall(hook_, 0), "uninstall");
            RequireSuccess(funchook_destroy(hook_), "destroy");
        }
    };

    HostAbortAdapter::HostAbortAdapter(FrameLifecycleService& service) :
        service_(service)
    {}

    HostAbortAdapter::~HostAbortAdapter() = default;

    void HostAbortAdapter::Install(void* target)
    {
        if (entry_ || !target)
        {
            throw std::logic_error("Host abort adapter requires an uninstalled, resolved target");
        }

        entry_ = std::make_unique<Entry>(service_, target);
    }

    void HostAbortAdapter::Uninstall()
    {
        entry_.reset();
    }
} // namespace nitroapi
