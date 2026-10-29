# Maintaining the GoldSrc ABI declarations

[Russian version](goldsrc-abi.ru.md)

`nextclient/goldsrc_abi` contains declarations for interoperability with the stock 32-bit GoldSrc engine and Counter-Strike game library. The CMake interface target `goldsrc::abi` exports its headers and the C++23 requirement. The engine facts cover build 8684. A declaration does not establish compatibility with another build.

The [module rules](../nextclient/goldsrc_abi/AGENTS.md) apply to changes in this library. [NitroApi](../nextclient/nitro_api/AGENTS.md) owns address resolution and hook registration.

## Getting started

To check the current model, use Python 3 and run this command from the repository root:

```text
python nextclient/goldsrc_abi/tools/library.py check
```

The command reports model problems without overwriting headers. To change declarations, follow [Adding or correcting a fact](#adding-or-correcting-a-fact), then [generate into a comparison directory](#regeneration) and compare the result with the tracked files.

## Sources and ownership

| Source | Responsibility |
|---|---|
| [facts.json](../nextclient/goldsrc_abi/tools/facts.json) | Pinned interoperability facts, source revisions, extraction tools, and compiler self-check results |
| [facts_linux.json](../nextclient/goldsrc_abi/tools/facts_linux.json) | Independently verified Linux x86 layouts and their provenance |
| [header_map.json](../nextclient/goldsrc_abi/tools/header_map.json) | Placement, names, completeness, replacements, and constant families |
| [library.py](../nextclient/goldsrc_abi/tools/library.py) | Model validation and generation of headers and layout assertions |
| [model.py](../nextclient/goldsrc_abi/tools/goldsrc_tools/model.py) | Dependency closure, declaration order, and generated model |
| [platforms.py](../nextclient/goldsrc_abi/tools/goldsrc_tools/platforms.py) | Supplement validation and separate Windows x86 and Linux x86 models |
| [emit.py](../nextclient/goldsrc_abi/tools/goldsrc_tools/emit.py) | Text emitted from the model |
| [golden_layout.cpp](../nextclient/goldsrc_abi/tests/golden_layout.cpp) | Compiler assertions against the recorded facts |

The generator reads the pinned facts and map. It does not recover facts by reading the current consumers. The `sources` and `tools` entries in the facts identify the extraction inputs; source paths inside fact keys are part of that input identity.

The Linux supplement's versioned schema and reproducible measurements are described in the [Linux evidence reference](goldsrc-abi-linux-facts.md). Its canonical `base_facts_sha256` binds it to the frozen Windows input. Every complete or prefix record, including nested dependencies, requires explicit Linux measurements and successful independent compiler checks. Missing measurements, conflicting identities, stale provenance, and failed self-checks prevent generation. Incomplete Linux records carry no inferred layout. Windows layouts retain their pinned values.

Tracked headers can also contain maintained additions. In particular, the `OBS_*` constants in [pmove.h](../nextclient/goldsrc_abi/include/goldsrc/pmove.h) are outside the generated model. The generator overwrites its output and does not merge these additions. Compare generated output with the tracked files and preserve each reviewed addition explicitly. Model validation alone does not check such additions.

[Maintained Studio facts](../nextclient/goldsrc_abi/tools/maintained_studio_facts.json) record source hashes, constants, sizes, alignment, and field offsets for the manually maintained Studio declarations. The generator does not consume this file. The [Studio layout assertions](../nextclient/goldsrc_abi/tests/studio_layout.cpp) check these declarations when the `goldsrc_abi_golden` target is compiled. Keep machine-specific commands and run results in ignored session or build reports.

## Declaration rules

- `goldsrc` contains stock engine facts; `goldsrc::cstrike` contains stock
  Counter-Strike facts. NextClient extensions belong to their owning module and
  must not add fields to a stock layout.
- Keep stock type, field, function-table member, and constant names. These names
  are the shared interoperability vocabulary. The map determines the declaration
  name when a stock tag has aliases.
- Qualify stock names explicitly in headers and sources. Do not add a
  `using namespace goldsrc` directive. Use `<goldsrc/fwd.h>` for pointer-only
  declarations instead of recreating stock tags or typedefs in a consumer.
- Declare only needed types and their dependencies. Keep related identifier sets,
  such as weapon IDs or protocol messages, together.
- Preserve function return types, pointer depth, calling conventions, table member
  order, packing, field offsets, and enum underlying types. Similar C++ types are
  not evidence of ABI compatibility.
- Stock structures have no member default initializers. Use `<cstdint>` types
  where the map replaces integer aliases; retain signature vocabulary such as
  `qboolean`, `vec3_t`, and `CRC32_t`.
- Constants are typed `inline constexpr` values; enums retain their stock form.
  The map controls decimal or hexadecimal representation for each constant family.

## Layout completeness

The map assigns declarations to three forms:

| Form | Use and required guarantees |
|---|---|
| Complete | Public records, function tables, and records embedded by value have their required fields described completely. |
| Incomplete | An internal type used only through pointers can remain a forward declaration. |
| Prefix with opaque tail | An internal record with field access exposes the required prefix and reserves the remaining stock bytes. |

A prefix record must retain the stock `sizeof` and `alignof`, including inheritance and padding. `unmodelled_` occupies stock storage after the exposed prefix; it is not an extension. The generated declaration uses the recorded alignment. If the last needed field is a bit-field or unnamed field, the prefix extends to a named non-bit-field boundary before the tail. A record with no exposed fields still reserves the required storage after its bases.

Full size remains necessary even when a consumer reads only the first fields: array indexing, pointer arithmetic, allocation, clearing, copying, and engine writes depend on it. Check all affected offsets and the total layout on an x86 target. A successful host-side Python check cannot establish a 32-bit C++ layout.

## Adding or correcting a fact

1. Establish the consumer's need and the owning binary. Keep build-specific
   addresses in NitroApi; keep a NextClient type in its owning module.
2. Obtain an interface specification from the project's call sites, binary
   analysis of the shipped engine, public documentation, or separately extracted
   facts. Record the source revision or binary build, tool versions, field names,
   types, offsets, signatures, values, and relevant semantics.
3. Keep extraction and implementation separate. The author of a replacement reads
   the specification or fact JSON, without opening Valve, leaked, or GPL originals
   while writing it. Do not copy implementation, comments, banners, formatting, or
   discretionary file organization. Do not derive replacements from Quake or
   Xash3D GPL sources. New behavior requires its own behavioral specification.
4. Accept extracted input only after resolving all reported problems, including
   completeness and compiler self-check failures. Preserve named exceptions and
   their reasons. Do not treat unmeasured values as confirmed facts.
5. Update the facts, map, generated declarations, and layout expectations together
   when the change belongs to the generated model. Review maintained additions
   separately and add appropriate checks when changing them.
6. Record which facts each replacement encodes and how they were obtained in the
   change's provenance record and commit message. Keep provenance notes out of
   source comments.

Declarations marked `moved_hltv_interface` belong to retained fork interfaces and are not clean-room input for replacement headers. The six interfaces are listed in [abi_scope.py](../nextclient/goldsrc_abi/tools/goldsrc_tools/abi_scope.py). Migration diagnostics can quote fragments of these files. Keep such logs out of the implementation input; do not open them in the clean-room authoring context.

## Regeneration

Run from the repository root:

```text
python nextclient/goldsrc_abi/tools/library.py check
python -m unittest discover -s nextclient/goldsrc_abi/tools -t nextclient/goldsrc_abi/tools -p "test_*.py"
python nextclient/goldsrc_abi/tools/library.py headers --out build/abi-review/include
python nextclient/goldsrc_abi/tools/library.py tests --out build/abi-review/tests
```

Use `python3` where that is the Python command. The generation commands accept `--facts`, `--linux-facts`, `--map`, and `--clang-format` overrides. The Linux input defaults to `tools/facts_linux.json`. Both platform models feed the same generation commands; common declarations appear once, with platform branches for differing declarations and assertions. The forward header rejects unsupported platforms and requires four-byte pointers. Their default output paths are the tracked include and test directories, so use an explicit comparison directory. The `headers` command also deletes `.h` files under its output's `goldsrc/` directory when they are absent from the generated model.

Keep the comparison directory inside the repository so clang-format finds the root configuration. Formatting output also depends on the clang-format version, which the generator reports. Compare file sets and contents, preserve maintained additions, then review the resulting diff before replacing tracked output.

Compile the layout assertions with a configured x86 build, for example:

```text
cmake --build --preset vs2022-release --target goldsrc_abi_golden
```

`goldsrc_abi_golden` is an object target with compiler assertions, not a runtime test executable. Build NitroApi and affected consumers in the relevant configurations. A public API change also requires checking exports, member offsets, signatures, and applicable address providers. Report runtime checks separately; the developer runs the game.

## Frozen extraction paths

`facts.py`, `usage.py`, and `codemod.py` use a pinned extraction model. Entries such as `dep/NclNitroApi` and `dep/NclNitroApi/dep/ncl-hl1-source-sdk` identify the source tree that produced those facts. The current declaration consumers and file coordinates do not form equivalent extraction input.

`abi_scope.EXTRACTION_SDK_DIR`, `OUR_SOURCE_DIRS`, `NITROAPI_DIR`, repository metadata in `facts.py`, and path classification in `migration.py` must be changed as one explicit input migration. Do not modernize individual frozen paths. `abi_scope.SDK_DIR` has a different role: it locates the live SDK fork for the provenance gate.

To repeat extraction, use a tree matching the recorded input and retain its provenance. `facts.py` requires `--out`; do not overwrite the pinned facts with an extraction from the current tree. Re-pinning requires reviewing payload changes, not only key sets, and rechecking macro names that occur in multiple sources.

## Provenance verification

The [build workflow](../.github/workflows/build.yml) specifies the complete pipeline. Request CMake File API `codemodel-v2` before configuration and run the gate after building, when generated resource sources exist:

```text
python nextclient/goldsrc_abi/tools/provenance.py --build-dir build/vs2022 --config Release --allowlist .github/provenance-allowlist.json --report build/provenance-report.json
```

The report follows textual includes and linked static or object-library dependencies for each binary target. Order-only dependencies are recorded separately. It checks source banners and dependency scope; it does not determine which functions survive linking or provide a legal conclusion about a binary.

The allowlist records the permitted closure and excluded dependencies. Do not expand it to admit a new dependency. `--update-allowlist` drops stale entries; review dependency classification changes explicitly. A configured target declared clean must remain free of newly introduced flagged sources.

Reconfigure after source deletion or build-graph changes. Compare dependency snapshots only for objects in the current graph; stale Ninja log entries are not current dependencies. A new verification script must have a checked failing control case and a nonempty selection. Save the build tree, target, configuration, and complete output with verification results.

## NitroApi integration

An exposed function requires its public data declaration, `StaticHookId`, module registration, and supported address-provider entries. An exposed variable requires its declaration, `RegisterVar`, and provider data. Preserve existing public member offsets and hook ID values; append new data members at the current end and inspect implicit enum increments.

Windows and Linux address providers require separate verification. Hook adapters must preserve the original calling convention and the chain's forwarding and return-value contract. Compilation does not validate runtime address matches.

`GameHud.h` forward-declares `class Vector`; consumers that need its definition include `<mathlib/vector.h>` themselves. Adding that include to the public header introduces dynamic initialization and tier0 link requirements into Debug consumers. NitroApi's private `easyloggingpp` copy also has its own behavior and must not be deduplicated as part of an ABI declaration change.
