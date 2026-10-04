// =============================================================================
// hwid_collector_linux.cpp -- Linux implementation of the HWID collector
// =============================================================================
// Same contract as hwid_collector_windows.cpp: SHA-256(seed1 + "|" + seed2) in
// lowercase hex, 64 chars.
//
//   seed1 - /etc/machine-id      (systemd persistent per-install ID, always
//                                 readable without root)
//   seed2 - /sys/class/dmi/id/product_uuid (motherboard UUID via DMI/sysfs -
//                                 the direct equivalent of the WMI UUID query
//                                 on Windows; usually root-only, so it's
//                                 normal for this to come back empty)
//
//   If both are empty -> a random seed persisted in a file under the user's
//   home directory (the equivalent of the Windows registry fallback).
// =============================================================================

#include <hwid_collector/hwid_collector.h>

#include <data_encoding/sha256.h>

#include <fstream>
#include <random>
#include <sstream>

namespace
{
    std::string g_hwid_cache;
    bool g_hwid_ready = false;

// SHA-256 result size constant
#ifndef NCLM_HWID_SIZE
#define NCLM_HWID_SIZE 64
#endif

    std::string Trim(const std::string& s)
    {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            return "";

        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    std::string ReadFileTrimmed(const std::string& path)
    {
        std::ifstream file(path);

        if (!file)
            return "";

        std::string line;
        std::getline(file, line);
        return Trim(line);
    }

    std::string CollectMachineId()
    {
        return ReadFileTrimmed("/etc/machine-id");
    }

    std::string CollectMotherboardUuid()
    {
        return ReadFileTrimmed("/sys/class/dmi/id/product_uuid");
    }

    // Fallback for the (rare) case where both of the above come back empty:
    // a random seed generated once and reused from a file, similar in spirit
    // to the registry key on Windows.
    std::string GetOrCreateFallbackSeed()
    {
        const char* home = getenv("HOME");
        std::string path = (home ? std::string(home) : std::string()) + "/.nextclient_hwid_seed";

        std::ifstream in(path);
        std::string existing;
        if (in && std::getline(in, existing) && !existing.empty())
            return existing;

        std::random_device rd;
        std::ostringstream oss;
        oss << std::hex << rd() << rd();
        std::string seed = oss.str();

        std::ofstream out(path);
        if (out)
            out << seed << std::endl;

        return seed;
    }
} // namespace

namespace hwid
{
    std::string Collect()
    {
        if (g_hwid_ready)
            return g_hwid_cache;

        std::string s1 = CollectMachineId(); // Hardware Seed 1
        std::string s2 = CollectMotherboardUuid(); // Hardware Seed 2

        // If both hardware identifiers fail, use the fallback seed
        if (s1.empty() && s2.empty())
            s1 = GetOrCreateFallbackSeed();

        std::string combined = s1 + "|" + s2;
        std::string hwid = sha256_hex(combined);

        // Ensure the hash result matches the expected size constant
        if (hwid.size() == NCLM_HWID_SIZE)
        {
            g_hwid_cache = hwid;
            g_hwid_ready = true;
        }

        return g_hwid_cache;
    }

    bool IsReady()
    {
        return g_hwid_ready;
    }

    void Reset()
    {
        g_hwid_cache.clear();
        g_hwid_ready = false;
    }
} // namespace hwid
