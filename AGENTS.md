# NextClient

A modification of the Counter-Strike 1.6 client. It ships replacement libraries alongside the stock game and patches Valve's engine (`hw.dll` on Windows, `hw.so` on Linux) at run time rather than replacing it. A Steam copy of the game is required.

## Modules

| Path | Ships as | What it is |
|---|---|---|
| [`nextclient/launcher`](nextclient/launcher/AGENTS.md) | `cstrike.exe` | process entry point, updater, whole-process relaunch |
| [`nextclient/engine_mini`](nextclient/engine_mini/AGENTS.md) | `next_engine_mini.dll` | runtime detours and byte patches of the stock `hw.dll` |
| [`nextclient/client_mini`](nextclient/client_mini/AGENTS.md) | `cstrike/cl_dlls/client_mini.dll` | HUD, view, weapons, radar |
| [`nextclient/gameui`](nextclient/gameui/AGENTS.md) | `cstrike/cl_dlls/GameUI.dll` (Windows), `cstrike/cl_dlls/gameui.so` (Linux) | VGUI menus; CEF-backed HTML UI on Windows |
| `nextclient/filesystem_proxy` | `FileSystem_Proxy.dll` | filesystem interposer |
| `nextclient/steam_api_proxy` | `steam_api.dll` | Steam API interposer |
| [`nextclient/packages`](nextclient/packages/AGENTS.md) | static and interface libraries | shared code: `ncl_math`, `ncl_utils`, `ncl_gl`, `ncl_image`, `ncl_material`, `ncl_perf`, `ncl_studio`, `task_coro`, `data_*`, `hwid_collector`, `gameplay` |
| [`nextclient/nitro_api`](nextclient/nitro_api/AGENTS.md) | `nitro_api2.dll` | engine address and hook layer |
| [`nextclient/goldsrc_abi`](nextclient/goldsrc_abi/AGENTS.md) | headers only | clean-room declarations of the stock engine ABI, namespace `goldsrc` |
| `dep/ncl-hl1-source-sdk` | `vgui2.dll` (Windows), `vgui2.so` (Linux) | HL1 SDK fork: `tier1`, `tier2`, `vgui2`, `mathlib`, `cef`, HLTV |
| `assets/` | copied into the game directory | resources, schemes, localization, shaders |
| `tools/matgen` | `matgen` development utility | Studio UV templates, material validation, and edge baking |
| `test_support/plugins/` | test server plugins | AMX Mod X test fixtures: `nextclient_testlab`, `nextclient_materials_test` |
| `test_support/runtime/` | test runtime dependencies | DLLs copied to test output directories by CMake |

All native client modules are 32-bit (`x86-windows-static` on Windows, `x86-linux` on Linux), because the engine they load into is. Third-party dependencies come from the vendored `vcpkg/`, wired in by the `base` configure preset.

## Build

Tracked configure presets are `ninja`, `ninja-tests`, `ninja-profiler`, `ninja-profiler-tests`, `vs2022`, `vs2022-tests`, `vs2026` and `linux`. Each has a matching `-release` build preset; `ninja`, `vs2022`, `vs2026` and `linux` also have `-debug` build presets.

`CMakeUserPresets.json` is untracked and machine-local: that is where a developer adds custom presets carrying their own `NEXTCLIENT_INSTALL_DIR`. Select a preset supported by the available toolchain and configure it before building. Build directories come from the selected configure preset's `binaryDir`. The Ninja generator is Multi-Config, so every build names a configuration.

```bash
cmake --build --preset ninja-debug --target client_mini
```

- Build one target per command so its result and log can be checked separately.
- Locate CMake in the current environment; if it is unavailable on PATH, use the installed executable's
  full path. For Ninja builds with MSVC, initialize the x86 compiler environment using the installed
  Visual Studio tools (`vcvarsall.bat x86` or an x86 developer shell).
- `engine_mini` and `gameui` collect sources with `file(GLOB_RECURSE ...)` and no `CONFIGURE_DEPENDS`. A new `.cpp` is absent from the build until the configure step runs again; referencing its symbols before then causes an unresolved symbol at link time. `client_mini` lists its sources instead, so a new file has to be added to `nextclient/client_mini/CMakeLists.txt`.

## Deploy

Shared-library and executable output roots are `<binaryDir>/out/bin/<config>/` when tests, profiling or UNIX are enabled, and `<sourceDir>/out/bin/<config>/` otherwise. Windows client and GameUI DLLs add `cstrike/cl_dlls/` below that root. `engine_mini-tests`, `gameui-tests` and `nitro_api-tests` use `<binaryDir>/nextclient/<module>/tests/<config>/` instead. Static libraries use their target's archive output directory.

Building an individual module target does not install it into the game at `NEXTCLIENT_INSTALL_DIR`. `BUILD_ALL` exists only when `NEXTCLIENT_BUILD_TESTS=OFF`; it builds the shipping modules and invokes the shared installation recipe when `NEXTCLIENT_INSTALL_DIR` is set. The complete material LFS preflight runs before any destination change, including Linux VGUI backup. With `NEXTCLIENT_BUILD_TESTS=ON`, `engine_mini` is a static library for tests; use a non-test configuration to build its shipping DLL.

Deploy the affected providers and their consumers from one source snapshot, platform, build preset, configuration and profiling setting. Follow the changed interface through its consumers; monitor changes can include NitroApi, engine, client, GameUI and launcher. Include the matching Tracy runtime when profiling is enabled. Record a manifest of the complete prepared module/resource set with source/build identity, absolute paths, byte lengths and SHA-256 values; verify every copied file against that manifest. Complete the affected builds, checks and [actual shipping-factory smoke](docs/testing/installation-and-factory.md) before requesting application closure for installation, then copy the prepared files and verify their hashes.

Use the feature procedures for [monitoring](docs/testing/performance-monitor.md), [material editor scenes](docs/testing/material-editor-scene.md) and [Studio rendering](docs/testing/studio-rendering.md). Ownership and shutdown rules remain in the [NitroApi lifecycle](nextclient/nitro_api/AGENTS.md#frame-lifecycle), [monitor](nextclient/engine_mini/src/performance/AGENTS.md), [renderer](nextclient/engine_mini/src/graphics/render/AGENTS.md) and [GameUI material authoring](nextclient/gameui/AGENTS.md#material-authoring-ownership) guides.

## Tests and verification

```bash
cmake --build --preset vs2022-tests-release --target client_mini-tests
```

Test targets require `NEXTCLIENT_BUILD_TESTS=ON`, enabled by the tracked `vs2022-tests`, `ninja-tests` and `ninja-profiler-tests` presets or a local preset. Available targets are grouped below:

- Module tests: `client_mini-tests`, `engine_mini-tests`, `gameui-tests`, `drag-value-label-tests`, `material-preview-panel-tests`, `nitro_api-tests` and `ncl-entity-message-tests`.
- Shared package tests: `data_types-tests`, `view-tests`, `crosshair-tests`, `key-binding-tests`, `settings_code-tests`, `ncl_image-tests` and `ncl_perf-tests`.
- Renderer and lifecycle tests: `renderer-lifecycle-tests`, `material-lifecycle-tests`, `performance-history-tests` and `preview-model-tests`.
- Offscreen GL tests: `radar-gl-tests`, `performance-gl-tests`, `studio-capture-gl-tests` and `scene-preview-gl-tests`.
- Conditional tests: `diagnostic-profiling-tests` and `profiling-connection-tests` additionally require `USE_PROFILER=ON`; `updater-test` requires Windows and `NEXT_LAUNCHER_ENABLE_UPDATER=ON`.

Build `verify-nitro-api-factory` for the real shared-provider check, or `nitro-api-factory-probe` for an explicit absolute provider path. Both targets are available outside test configurations and are excluded from ordinary shipping/install dependencies. With tests enabled, CTest registers `installation-preflight`, `nitro-api-shipping-factory` and `nitro-api-shipping-factory-missing-provider`; see the [installation/factory procedure](docs/testing/installation-and-factory.md) for executable commands and paths.

These tests run without the live engine. Keeping logic on that side makes it testable; offscreen GL tests additionally exercise rendering with their own context.

Do not launch the game to check a change. The loop is a build plus the unit tests; the developer runs the game and reports back. When something has to be observed at run time, add a console command that dumps the state rather than driving the game.

## Code review

Read the applicable directory rules and establish the comparison base before reviewing a branch. Trace affected callers, owners and consumers outside the diff. Historical sessions explain why a constraint exists, but an unmerged feature or a superseded implementation is not a fact about the current checkout.

Cover these areas where the change touches them:

- **Lifetime and ordering:** initialization, reset, disconnect, normal and fatal shutdown;
  subscriptions, callbacks and work that can continue after cancellation.
- **ABI and NitroApi:** layout, pointer depth, calling conventions, public member offsets, hook IDs
  and the address-provider registration path.
- **Duplication and architecture:** ownership of state, existing STL/EASTL or package helpers,
  repeated policy and an API whose name no longer describes its behavior.
- **GL resources and state:** context lifetime, framebuffer selection, state restoration and draw
  order across the stock and replacement renderers.
- **Behavior:** actual recipients and timing of engine messages, observer modes, stale state,
  failure paths and preservation of stock callback contracts.
- **Project conventions:** the root and local naming, API shape, dependencies, comments and
  formatting rules on the changed code. Do not turn the review into unrelated style cleanup.
- **Verification:** tests that exercise the failure scenario, affected consumers, a current CMake
  graph and the distinction between compilation, installation and runtime observation.

A finding identifies the trigger, the incorrect result and a precise code location, backed by the actual call path or a reproducing test. Separate confirmed defects from open questions; discard a candidate when the code proves its precondition cannot occur. Do not report a hypothetical guard or a preference as a demonstrated bug. State the checks performed and relevant coverage limits.

## Conventions

A directory with rules of its own keeps an `AGENTS.md` next to the code and adds to what is here rather than restating it: see [nextclient/client_mini/src/hud/radar/AGENTS.md](nextclient/client_mini/src/hud/radar/AGENTS.md).

**Documentation.** Keep maintained guides, references, and test procedures under `docs/`, in English (`.md`) with Russian translations in matching `.ru.md` files. Root README files remain project entry points, and directory-specific `AGENTS.md` files stay beside their code. Use `.superpowers/` for design drafts, implementation plans, and session reports; that directory is ignored by Git. Keep procedures, references, and test cases separate, and link each translation to its counterpart. In human-facing Markdown documentation, keep each prose paragraph on one source line. Do not insert hard wraps at a fixed column width. Preserve structural and intentional line breaks in lists, tables, code, and diagrams.

Write Russian documentation as natural Russian technical prose. Adapt the meaning and sentence structure rather than translating English word for word. Use short sentences with a concrete subject and action; state the condition and result explicitly. Prefer verbs to chains of abstract nouns and passive constructions. A formal tone must remain easy to read.

Use established developer terms consistently, including common loanwords such as «колбэк», «дамп», «пресет» and «рендеринг» where appropriate. Explain an unfamiliar term at first use. Translate idioms by context: `in flight` means an operation is still running or has not completed; `hot path` means frequently executed code; `teardown` means the relevant cleanup or shutdown. Keep commands, identifiers, numeric limits and technical distinctions exact. Use [Microsoft Learn](https://learn.microsoft.com/ru-ru/) and the articles by [dalerank](https://habr.com/ru/users/dalerank/articles/), [anz](https://habr.com/ru/users/anz/articles/) and [PatientZero](https://habr.com/ru/users/PatientZero/articles/) as readability references. Review each Russian text on its own, then compare it with the English source for completeness.

**Naming.** Methods and functions are `CamelCase`; public global feature functions use `Module_Method`, while functions in subsystem namespaces follow that subsystem's naming. Restored engine functions retain their engine names. Globals are `g_CamelCase`, `constexpr` constants `kCamelCase` (no underscore, acronyms as words: `Abc`, not `ABC`), locals `snake_case`, and typedefs carry the `_t` suffix. A simple accessor is `snake_case`: `get_x()` for a plain one, `is_x()` / `are_x()` / `should_x()` for a predicate. Setters keep the `SetX()` form; `set_x()` exists nowhere here and should not be introduced.

**File names.** Use `CamelCase` for a file organized around a primary class, struct or enum. Several related types may share that file when one is the main type. Use `snake_case` for files that expose a set of free functions.

**Predicates.** `Is` is a pure answer read from state already known. `Check` warns that answering does the work, so the call is neither free nor repeatable at will (a first call that runs a one-time GL init, creates a font, or fires engine traces). `Ensure` is the make-it-so form that reports success rather than an answer. A third-person verb already reads as a question and keeps its name (`Contains`, `Blocks`). A bare adjective or participle (`Available`, `Enabled`) picks none of these. An accessor that has to load or compute first is not plain and says so on the declaration.

**Comments.** Sparse, English only, explaining what the code cannot: an ordering constraint, a workaround, a non-local invariant, the engine or driver behavior that forced the shape of the code. No banners, no line-by-line narration, no doc comment above a function by default, nothing that restates the name or the signature. A comment on a function states a property of that function, never where it is called from today nor what a caller does with the answer: callers change, and such a claim cannot be checked from the file the comment sits in. A `call after X` precondition is allowed only when it is that function's own correctness condition, and reads better as the dependency it rests on. Do not describe what something is not unless the negation names the obvious alternative that was rejected and what it breaks. One fact lives in one place: the same note repeated over several declarations is a missing named constant. Why a change was made belongs in the commit message, not next to the code.

**Declarations.** `#pragma once`, never include guards. Member defaults use empty braces when the default is the zero value (`int x{}`, `T* p{}`), the equals form only for a non-zero or named default (`float start_ = -1.0f`); locals keep `=`. Prefer explicit types over `= {}`, and `auto` only for iterators and verbose types. C-style casts are for numeric conversions only; a pointer or layout reinterpretation goes through `reinterpret_cast` or `memcpy`, never a C-style cast. Brace every control-flow block. File-scope constants go in an anonymous namespace rather than inside a function. Includes run own header, STL, external `<...>`, project headers last, with project paths written from the module's source root and never `../`.

**Code layout.** Write new and modified code for human readability. Separate declarations and setup, control flow, result processing, and returns with blank lines when they form distinct logical blocks. Keep nonempty function bodies and control-flow blocks on multiple lines. Wrap long calls, signatures, and aggregate initializers by argument or element when their structure is difficult to scan, even if they fit the column limit. Use the root `.clang-format` for indentation and brace placement; its column limit does not replace these readability rules. Apply this to touched code without reformatting entire files.

**Class member order.** Omit empty groups and use this order:

1. Public nested types.
2. Private nested types.
3. Private nonstatic variables.
4. Private static variables.
5. Public variables, including static variables.
6. Public nonstatic methods, including constructors and destructors.
7. Private nonstatic methods.
8. Private static methods.
9. Public static methods.

Repeat access labels when needed to keep these groups in order. A static constant used in a member type, such as an array bound, must precede its first use; explain that ordering exception with a short comment. Preserve the relative order of nonstatic fields and virtual methods when reorganizing declarations, since they determine object layout, construction order, and the virtual interface.

**Interface versions.** Compare a factory interface with the latest published release that contains it. If its ABI changes, allocate one new version for the next release and keep that number throughout development of that release. New interfaces start at `001`. Keep provider registrations, consumers, and maintained documentation consistent.

**API shape.** Use namespaces for subsystems and shared packages, grouping their related types, functions and constants. Keep state with an owner and a lifetime in classes; the owner holds the instance and resets it. One header may declare several classes when a single owner holds them, so group a header by owner and never by property. Global functions are appropriate for restored engine functionality and small features. Keep engine entry point names and use `Module_Function` for other public global feature functions. Groups of constants may also use a namespace when a qualifier helps them read clearly. Inside a named namespace, avoid repeating that namespace's subsystem name as a function prefix. Use a nested namespace for a coherent subcategory when another ownership boundary is useful.

**Changes.** Keep them tightly scoped and format only the lines you touched, with the root `.clang-format`: the committed tree is not uniformly format-clean, so reformatting a whole file buries the change in noise. Prefer a deterministic fix to a defensive guard; a check that cannot fire is noise, and a state that cannot occur should fail loudly rather than be papered over.

## Traps

- Line endings are mixed across the tree. Preserve whatever a file already uses; never normalize a whole
  file, or the diff stops showing the change.
- `assets/cstrike/resource/nextclient_*.txt` are UTF-16 LE and the English one carries a double BOM.
  Use an editor or script that preserves UTF-16 LE, the existing BOM sequence and line endings.
  Verify these properties after saving.
- Engine facts (addresses, globals, function signatures) belong in NitroApi, which resolves them per
  build, not in a hardcoded copy next to the code that needs them.
- Player-facing resources, scheme entries and localization strings belong under `assets/`. Cvar defaults belong in their owning C++ tables; shipped configuration files serve intentional presets or user-facing workflows.
