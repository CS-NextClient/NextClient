#include "engine.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "console/console.h"
#include "graphics/render/RenderDevice.h"
#include "graphics/render/ShaderCache.h"

namespace
{
    constexpr uint32_t kMagic = 0x43534c4e;   // "NLSC"
    constexpr uint32_t kVersion = 2;

    // A corrupt or truncated header must not turn into an allocation the size of whatever
    // the file happens to hold. All three limits are far above any real program blob; the
    // total exists because the per-entry limits compose to more than a 32-bit process has.
    constexpr uint32_t kMaxBlob = 8u * 1024u * 1024u;
    constexpr uint32_t kMaxEntries = 256;
    constexpr uint32_t kMaxTotalBytes = 64u * 1024u * 1024u;

    constexpr uint64_t kFnvOffset = 1469598103934665603ull;
    constexpr uint64_t kFnvPrime = 1099511628211ull;

    struct CacheEntry
    {
        uint64_t hash;
        uint32_t format;
        std::vector<uint8_t> blob;

        // Not persisted: whether this run asked for the entry, which is what a save
        // carries forward.
        bool used = false;
    };

    std::vector<CacheEntry> g_Entries;
    bool g_Loaded;
    bool g_Dirty;

    uint64_t BlobHash(const std::vector<uint8_t>& blob)
    {
        return render::Cache_Hash(
            std::string_view(reinterpret_cast<const char*>(blob.data()), blob.size()),
            kFnvOffset
        );
    }

    // Which driver, on which GPU, produced the blobs. Nothing in a program binary is
    // portable, and a driver update is the ordinary way for one to stop being accepted,
    // so the whole file is keyed on this rather than each entry being probed.
    uint64_t DriverHash()
    {
        uint64_t hash = kFnvOffset;

        hash = render::Cache_Hash(render::Device_GlString(GL_VENDOR, ""), hash);
        hash = render::Cache_Hash(render::Device_GlString(GL_RENDERER, ""), hash);
        hash = render::Cache_Hash(render::Device_GlString(GL_VERSION, ""), hash);
        hash = render::Cache_Hash(render::Device_GlString(GL_SHADING_LANGUAGE_VERSION, ""), hash);

        return hash;
    }

    // Empty only if the engine's function table was never handed over, which the boot
    // order rules out: OnGameInitializing copies it before its GL_SetMode subscriber
    // builds the first programs. Treated as running without a cache.
    std::filesystem::path CachePath()
    {
        if (gEngfuncs.pfnGetGameDirectory == nullptr)
        {
            return {};
        }

        const char* directory = gEngfuncs.pfnGetGameDirectory();
        if (directory == nullptr || *directory == '\0')
        {
            return {};
        }

        return std::filesystem::path(directory) / "cache" / "shaders.bin";
    }

    template <class T>
    bool ReadPod(std::istream& file, T& out)
    {
        file.read(reinterpret_cast<char*>(&out), sizeof(T));
        return file.good();
    }

    template <class T>
    void WritePod(std::ostream& file, const T& value)
    {
        file.write(reinterpret_cast<const char*>(&value), sizeof(T));
    }

    void Load()
    {
        g_Loaded = true;

        std::filesystem::path path = CachePath();
        if (path.empty())
        {
            return;
        }

        std::ifstream file(path, std::ios::binary);

        if (!file.is_open())
        {
            return;
        }

        uint32_t magic = 0;
        uint32_t version = 0;
        uint64_t driver = 0;
        uint32_t count = 0;

        if (!ReadPod(file, magic) || !ReadPod(file, version) || !ReadPod(file, driver) || !ReadPod(file, count))
        {
            return;
        }

        if (magic != kMagic || version != kVersion || count > kMaxEntries)
        {
            return;
        }

        if (driver != DriverHash())
        {
            // Not an error and not worth a warning: a driver update invalidates every blob
            // in the file, and the entries are rebuilt over this run.
            Con_DPrintf(ConLogType::Info, "render: shader cache is from another driver, rebuilding\n");
            return;
        }

        std::vector<CacheEntry> entries;
        entries.reserve(count);

        uint32_t total = 0;

        for (uint32_t i = 0; i < count; i++)
        {
            CacheEntry entry;
            uint32_t size = 0;
            uint64_t blob_hash = 0;

            if (!ReadPod(file, entry.hash) || !ReadPod(file, entry.format) || !ReadPod(file, size) ||
                !ReadPod(file, blob_hash))
            {
                return;
            }

            if (size == 0 || size > kMaxBlob || size > kMaxTotalBytes - total)
            {
                return;
            }

            total += size;

            entry.blob.resize(size);
            file.read(reinterpret_cast<char*>(entry.blob.data()), size);

            if (!file.good())
            {
                return;
            }

            // Interleaved writers can leave a file whose framing still parses; the blob
            // hash keeps such bytes away from the driver's binary parser.
            if (BlobHash(entry.blob) != blob_hash)
            {
                return;
            }

            entries.push_back(std::move(entry));
        }

        Con_DPrintf(
            ConLogType::Info,
            "render: shader cache %s holds %u programs\n",
            path.string().c_str(),
            static_cast<unsigned>(entries.size())
        );

        g_Entries = std::move(entries);
    }

    void EnsureLoaded()
    {
        if (!g_Loaded)
        {
            Load();
        }
    }
}

uint64_t render::Cache_Hash(std::string_view text, uint64_t seed)
{
    uint64_t hash = seed;

    for (char c : text)
    {
        hash ^= static_cast<uint8_t>(c);
        hash *= kFnvPrime;
    }

    return hash;
}

uint64_t render::Cache_HashSeed()
{
    return kFnvOffset;
}

const std::vector<uint8_t>* render::Cache_Lookup(uint64_t hash, GLenum& format)
{
    EnsureLoaded();

    for (CacheEntry& entry : g_Entries)
    {
        if (entry.hash == hash)
        {
            entry.used = true;
            format = entry.format;
            return &entry.blob;
        }
    }

    return nullptr;
}

void render::Cache_Store(uint64_t hash, GLenum format, std::vector<uint8_t> blob)
{
    if (blob.empty() || blob.size() > kMaxBlob)
    {
        return;
    }

    EnsureLoaded();

    for (CacheEntry& entry : g_Entries)
    {
        if (entry.hash == hash)
        {
            entry.format = format;
            entry.blob = std::move(blob);
            entry.used = true;
            g_Dirty = true;
            return;
        }
    }

    if (g_Entries.size() >= kMaxEntries)
    {
        return;
    }

    g_Entries.push_back(CacheEntry {hash, format, std::move(blob), true});
    g_Dirty = true;
}

void render::Cache_Save()
{
    if (!g_Dirty)
    {
        return;
    }

    std::filesystem::path path = CachePath();
    if (path.empty())
    {
        return;
    }

    // A hash nothing asked for this run belongs to a source that no longer ships;
    // carried forward it would grow the file to the entry cap and then freeze it.
    std::erase_if(g_Entries, [](const CacheEntry& entry) { return !entry.used; });

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);

    // Written beside and renamed over, so a crash mid-write cannot leave a half file
    // under the name Load trusts.
    std::filesystem::path temp = path;
    temp += ".tmp";

    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            // Dirty stays set: a locked directory is transient, and the retries are
            // bounded by how rarely anything asks for a save.
            Con_DPrintf(ConLogType::Warning, "render: cannot write %s\n", temp.string().c_str());
            return;
        }

        WritePod(file, kMagic);
        WritePod(file, kVersion);
        WritePod(file, DriverHash());
        WritePod(file, static_cast<uint32_t>(g_Entries.size()));

        for (const CacheEntry& entry : g_Entries)
        {
            WritePod(file, entry.hash);
            WritePod(file, entry.format);
            WritePod(file, static_cast<uint32_t>(entry.blob.size()));
            WritePod(file, BlobHash(entry.blob));
            file.write(reinterpret_cast<const char*>(entry.blob.data()), static_cast<std::streamsize>(entry.blob.size()));
        }
    }

    std::filesystem::rename(temp, path, error);

    if (error)
    {
        Con_DPrintf(ConLogType::Warning, "render: cannot write %s\n", path.string().c_str());
        std::filesystem::remove(temp, error);
        return;
    }

    g_Dirty = false;
}

void render::Cache_Clear()
{
    g_Entries.clear();
    g_Loaded = true;
    g_Dirty = false;

    std::error_code error;
    std::filesystem::remove(CachePath(), error);
}
