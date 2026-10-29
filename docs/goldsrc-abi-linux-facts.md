# Linux x86 GoldSrc ABI evidence

[Russian version](goldsrc-abi-linux-facts.ru.md)

The Linux supplement [facts_linux.json](../nextclient/goldsrc_abi/tools/facts_linux.json) records independent measurements for the stock 32-bit Linux ABI associated with engine build 8684. The [maintenance reference](goldsrc-abi.md) describes declaration ownership. Frozen Windows facts and source-coordinate identities remain in [facts.json](../nextclient/goldsrc_abi/tools/facts.json).

## Evidence sources

The extraction inputs are the original NextClient revision `a64b71ecbbe10d7f0d601c3e210e98ef2ace2550`, NitroApi revision `669639de5e998cf35c32bcaa121e2c57e284930b`, and SDK revision `e3130f351307968101ae569f609ce2c2d4b8eb3e`. Their original paths, including `dep/NclNitroApi/dep/ncl-hl1-source-sdk`, identify frozen records. Extraction uses copies recovered from those Git objects, without modifying the working SDK or reading replacement declarations as an expected layout.

Two stock binaries supply independent ELF32-i386 DWARF information:

| Binary | SHA256 |
|---|---|
| `hw.so` | `1e775773292407106ac17c98bc19f0444e8f1525d46e592de5cc53afad2b89e1` |
| `cstrike/cl_dlls/client.so` | `40283f3e0c4b6bd21f9281304957ab93c04c60ce5f3d2794cdaf58b3c11ab798` |

Static analysis identifies engine build 8684. `build_number` at virtual address `0x000f4e80` reads the `date` global at `0x002d53b0`, whose stored pointer refers to `Aug  3 2020` at `0x0026b810`. The disassembled year origin 1901, multiplier 365.25, truncation, day index, leap-year adjustment, and subtraction `0x88b3` produce 8684. The client binary comes from the same installed Steam copy; its identity is pinned by its own hash. No game was launched during extraction.

GNU Binutils 2.42 extracts DWARF. GCC 15.2.0 measures original complete declarations with `-m32 -std=c++23`; Clang 22.1.3 independently checks those measurements with `--target=i686-pc-linux-gnu -m32 -std=c++23` and the existing Linux sysroot. Explicit target checks cover four-byte pointers and four-byte `wchar_t`. The supplement contains actual command arrays, compiler options, probe identifiers, and results.

Raw inputs, source-containing probes, DWARF dumps, and diagnostics are kept under `build/abi-evidence/linux/`. Declaration authors consume the supplement and this reference. Extraction-only portability inputs supply existing Linux networking headers and select SDK scalar aliases through `GNUC` and `NO_HLSDK_ARCHTYPES`. They do not modify original record declarations. An extraction copy of `com_model.h` omits a duplicate `cache_user_s` declaration; the same pinned definition is supplied by `quakedef.h`, and no requested record is altered. All original input bytes are checked against the pinned archives.

## Coverage and source reconciliation

The required closure contains 117 top-level materialized records and five nested records. Both complete and prefix forms receive full Linux size, alignment, base placement, and field measurements. Equal Windows and Linux layouts also require an explicit Linux entry. A prefix's opaque storage reserves the complete stock record, including its unexposed members.

| Record | Linux size | Linux alignment |
|---|---:|---:|
| `hud_player_info_t` | 28 | 4 |
| `CHud` | 27508 | 4 |
| `CHudMessage` | 16744 | 4 |
| `server_t` | 287756 | 4 |
| `server_static_t` | 132 | 4 |

Stock DWARF matches 121 required record identities. The retained NitroApi `EngineMsg` record is established from its pinned original input and checked with both compilers. DWARF supplies full opaque sizes; complete original probes and independent compiler checks establish alignment and validate offsets. DWARF2 does not provide an explicit alignment attribute for these stock records.

Owning-binary selection resolves internal variants. `player_info_t` is the engine record used by `client_state_t::players`; its measured `hw.so` size is 592. `playermove_t` is the full engine callback interface with size 325068. The client's private variants have sizes 568 and 325056, respectively. They are recorded as distinct internal variants and do not supply these public engine layouts.

The supplement preserves established public member names through explicit stock-to-frozen correspondences. Stock `CHudMessage::m_pMessages` corresponds to `m_Messages`, a 16-element `client_message_t` array at offset 20. Stock `playermove_t::player_mins` and `player_maxs` correspond to `_player_mins` and `_player_maxs`, with equivalent `float[4][3]` storage at offsets 324852 and 324900. The `enginefuncs_t` tutor-buffer functions and parameter-check function retain their public spellings; the correspondence records exact function types and table offsets.

Three stock Linux function tables have additional terminal members established directly from DWARF and verified in separate full-table compiler probes:

| Record | Full size | Terminal member | Offset | Function pointer type |
|---|---:|---|---:|---|
| `cl_enginefunc_t` | 540 | `pfnFilteredClientCmd` | 536 | `int (*)(char *)` |
| `cl_enginefunc_dst_t` | 492 | `pfnFilteredClientCmd` | 488 | `void (*)(char **)` |
| `enginefuncs_t` | 636 | `pfnPEntityOfEntIndexAllEntities` | 632 | `edict_t *(*)(int)` |

These are nonvariadic pointers with the default Linux x86 C calling convention. Each full-table probe checks every member offset and total layout against the stock measurements. An intentionally incorrect terminal offset must fail the compiler check.

## Supplement contract

`schema_version` is 1; `target` is `i686-pc-linux-gnu`; `engine_build` is 8684. `base_facts_sha256` hashes the canonical UTF-8 serialization produced by `json.dumps(facts, sort_keys=True, separators=(",", ":"), ensure_ascii=False)`. The raw input byte hash is retained separately. Both identify the unchanged Windows extraction.

`records` uses the existing fact schema, including nested entries and original record keys. `measurements` indexes every materialized key, including nested keys, and connects it to its original probe, stock DIE where available, and independent compiler check. Newly established stock table members have binary-specific member identities. `sources`, `tools`, and `field_correspondences` record provenance and reconciled names without transferring source implementation.

`self_check.completed` may be true only when the computed materialized closure equals both `required_records` and `verified_records`, every measurement has real successful verification entries, and failures, unresolvable values, other errors, and missing records are zero. An unknown measurement cannot fall back to a Windows layout. Full command manifests and failing controls are retained with the raw evidence.

Final verification covers all 122 records, 1877 full-record field offsets, and eight base offsets. All 45 GCC measurement binaries were replayed with identical output; all 45 Clang checks passed. The recorded positive compiler assertions total 3103, including separate full-stock table checks. Numeric summaries are retained in `compiler/gcc-replay-manifest.json`, `compiler/clang_cross_manifest.json`, and `supplement-verification.json` under the evidence directory.

The retained extraction workspace provides a numerical verifier, executable from the repository root:

```text
python build/abi-evidence/linux/verify_linux_facts.py
```

It compares the finalized supplement with the independent numerical manifests and checks failing mutations of `CHud` full size, SteamID offset, and the terminal function's `char **` pointer depth. It does not read original implementation or replacement headers.

## Linux compatibility handles

NitroApi's Linux platform branch supplies the global opaque handle aliases `HINSTANCE`, `HMODULE`, `HWND`, `HDC`, and `HGLRC` used by its consumers. All five are `void *`, with size and alignment four on x86. Their source is `common/port.h:61-64` at pinned SDK revision `e3130f351307968101ae569f609ce2c2d4b8eb3e`, under the frozen input identity `dep/NclNitroApi/dep/ncl-hl1-source-sdk`. Its SHA-256 is `c656199776296b9729e4a74c6a6ea077952247d5e0876142abf04ff3a8f090db`.

The same revision's `public/engine_launcher_api.h:54` establishes an opaque `void *` instance parameter for `IEngineAPI::Run`; its SHA-256 is `aeba8233edd1b0974f499f1f1808fb394fe72f5cc3b086f0a2f645b8d04f1352`. It establishes no ownership or nullability rule. GCC 15.2 and Clang 22.1.3 independently verified the four aliases and the exact launcher member signature with 14 assertions each on `i686-pc-linux-gnu`. Both rejected an integral-handle negative control. Numeric facts, actual commands, and source hashes are retained in `build/abi-evidence/linux/port-handles-facts.json`. These aliases belong to NitroApi's platform compatibility declarations; they add no `goldsrc` record or layout to the generated models.

`HMODULE` is established by the same input at `common/port.h:47`, in the non-Windows branch. Additional GCC and Clang checks confirmed its `void *` identity, x86 size and alignment, and rejected an integral-alias control.
