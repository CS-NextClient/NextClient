#pragma once
// =============================================================================
// hwid_collector.h  --  zSteam HWID System (client-side) v2
// =============================================================================
// Generates a unique SHA-256 hardware identifier (64 hex chars).
//
// Sources (in order of priority):
//   seed1 — Motherboard UUID via WMI Win32_ComputerSystemProduct
//   seed2 — Boot disk serial number:
//      Win8+  → WMI MSFT_Disk (Root\Microsoft\Windows\Storage), BootFromDisk=TRUE
//      Vista+ → WMI Win32_DiskDrive + IOCTL_STORAGE_GET_DEVICE_NUMBER
//      Fallback → Direct IOCTL_STORAGE_QUERY_PROPERTY (no WMI)
//   If all fail → Persistent seed in registry HKCU\Software\NextClient\HwidSeed
//
// Dependencies: Crypt32.lib, Advapi32.lib, wbemuuid.lib, ole32.lib, oleaut32.lib
// Compiler: MSVC 2022, x86
// =============================================================================

#include <string>
#include <vector>

namespace hwid
{
    // Collects and returns the HWID (SHA-256 hex, 64 chars).
    // Idempotent — uses memory cache after the first call.
    std::string Collect();

    // Returns true if the HWID has already been successfully collected.
    bool IsReady();

    // Clears the session cache — call this when disconnecting from the server.
    void Reset();
} // namespace hwid
