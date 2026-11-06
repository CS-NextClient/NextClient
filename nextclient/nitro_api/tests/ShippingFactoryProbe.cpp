#include <cstdio>
#include <filesystem>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

#include <nitroapi/FrameLifecycleInterface.h>

namespace nitroapi::verification
{
#ifdef _WIN32
    using Factory_t = void*(__cdecl*)(const char*, int*);
#else
    using Factory_t = void* (*)(const char*, int*);
#endif

    class Observer final : public FrameLifecycleObserverInterface
    {
    public:
        void OnHostEntry() override {}

        void OnFrameAccepted() override {}

        void OnHostExit(bool) override {}
    };

    int CheckFactory(Factory_t factory)
    {
        if (!factory)
        {
            std::fprintf(stderr, "FAIL factory: CreateInterface is missing\n");
            return 3;
        }

        FrameLifecycleInterface* lifecycle =
            static_cast<FrameLifecycleInterface*>(factory(NITRO_FRAME_LIFECYCLE_INTERFACE_VERSION, nullptr));
        if (!lifecycle)
        {
            std::fprintf(stderr, "FAIL interface: %s is missing\n", NITRO_FRAME_LIFECYCLE_INTERFACE_VERSION);
            return 4;
        }

        Observer first;
        Observer second;
        FrameLifecycleSubscription_t first_id = lifecycle->Subscribe(&first);
        FrameLifecycleSubscription_t second_id = lifecycle->Subscribe(&second);
        lifecycle->Unsubscribe(first_id);
        lifecycle->Unsubscribe(first_id);
        lifecycle->Unsubscribe(second_id);
        lifecycle->Unsubscribe(second_id);

        if (first_id == 0 || second_id == 0 || first_id == second_id)
        {
            std::fprintf(stderr, "FAIL subscriptions: IDs must be distinct and nonzero\n");
            return 5;
        }

        std::printf(
            "PASS %s: subscription IDs %llu, %llu; repeated removal completed\n",
            NITRO_FRAME_LIFECYCLE_INTERFACE_VERSION,
            static_cast<unsigned long long>(first_id),
            static_cast<unsigned long long>(second_id)
        );
        return 0;
    }

    int CheckProvider(const std::filesystem::path& path)
    {
        if (!path.is_absolute())
        {
            std::fprintf(stderr, "FAIL arguments: provider path must be absolute\n");
            return 1;
        }

#ifdef _WIN32
        HMODULE module = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!module)
        {
            std::fprintf(stderr, "FAIL load: Windows error %lu\n", GetLastError());
            return 2;
        }

        Factory_t factory = reinterpret_cast<Factory_t>(GetProcAddress(module, "CreateInterface"));
#else
        void* module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!module)
        {
            std::fprintf(stderr, "FAIL load: %s\n", dlerror());
            return 2;
        }

        Factory_t factory = reinterpret_cast<Factory_t>(dlsym(module, "CreateInterface"));
#endif
        int result = CheckFactory(factory);

#ifdef _WIN32
        FreeLibrary(module);
#else
        dlclose(module);
#endif
        return result;
    }
} // namespace nitroapi::verification

#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif
    if (argc != 2)
    {
        std::fprintf(stderr, "FAIL arguments: expected one absolute provider path\n");
        return 1;
    }

    return nitroapi::verification::CheckProvider(std::filesystem::path(argv[1]));
}
