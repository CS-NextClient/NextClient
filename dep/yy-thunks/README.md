# YY-Thunks (Windows 7 compatibility)

Prebuilt thunk object that backfills Win8+ Windows API functions which the modern MSVC runtime statically imports,
and which are absent on Windows 7.

The object provides `__imp_*` definitions for those functions; the linker resolves
the imports to the thunk instead of the system import library. At runtime each thunk
calls the real OS function when available (Win8+) and falls back to a Win7-compatible
implementation otherwise.

## Provenance

- Project: https://github.com/Chuyu-Team/YY-Thunks
- Version: **v1.2.1** (2025-05-07)
- Asset: `YY-Thunks-Objs.zip`
  - SHA256: `36B6CF1F8F9AC0D05863140D20D8CA70A796775E520FED2842667B517A867F52`
- Vendored file: `x86/YY_Thunks_for_Win7.obj` (taken from `objs/x86/` in the asset)
  - SHA256: `AD9B37CC55B9A43757D7E63628A91A701CA2D74028043E9450C96165992D2302`
