# NextClient

A modification of the Counter-Strike 1.6 client. It ships replacement libraries alongside the stock game and
patches Valve's engine (`hw.dll` on Windows, `hw.so` on Linux) at run time rather than replacing it. A Steam copy of the game is
required.

## Modules

| Path | Ships as | What it is |
|---|---|---|
| [`nextclient/launcher`](nextclient/launcher/AGENTS.md) | `cstrike.exe` | process entry point, updater, whole-process relaunch |
| [`nextclient/engine_mini`](nextclient/engine_mini/AGENTS.md) | `next_engine_mini.dll` | runtime detours and byte patches of the stock `hw.dll` |
| [`nextclient/client_mini`](nextclient/client_mini/AGENTS.md) | `cstrike/cl_dlls/client_mini.dll` | HUD, view, weapons, radar |
| [`nextclient/gameui`](nextclient/gameui/AGENTS.md) | `cstrike/cl_dlls/GameUI.dll` (Windows), `cstrike/cl_dlls/gameui.so` (Linux) | VGUI menus; CEF-backed HTML UI on Windows |
| `nextclient/filesystem_proxy` | `FileSystem_Proxy.dll` | filesystem interposer |
| `nextclient/steam_api_proxy` | `steam_api.dll` | Steam API interposer |
| [`nextclient/packages`](nextclient/packages/AGENTS.md) | static libs | shared code: `ncl_math`, `ncl_utils`, `ncl_gl`, `task_coro`, `data_*`, `hwid_collector`, `gameplay` |
| [`nextclient/nitro_api`](nextclient/nitro_api/AGENTS.md) | `nitro_api2.dll` | engine address and hook layer |
| [`nextclient/goldsrc_abi`](nextclient/goldsrc_abi/AGENTS.md) | headers only | clean-room declarations of the stock engine ABI, namespace `goldsrc` |
| `dep/ncl-hl1-source-sdk` | `vgui2.dll` (Windows), `vgui2.so` (Linux) | HL1 SDK fork: `tier1`, `tier2`, `vgui2`, `mathlib`, `cef`, HLTV |
| `assets/` | copied into the game directory | resources, schemes, localization, shaders |

Everything is 32-bit (`x86-windows-static` on Windows, `x86-linux` on Linux), because the engine it loads into is. Third-party
dependencies come from the vendored `vcpkg/`, wired in by the `base` configure preset.

## Build

Tracked configure presets are `ninja`, `vs2022`, `vs2022-tests`, `vs2026` and `linux`; the build presets that go
with them are `ninja-debug`, `ninja-release` and the `vs*` and `linux-*` pairs. `CMakeUserPresets.json` is untracked
and machine-local: that is where a developer adds `ninja-local`-style presets carrying their own
`NEXTCLIENT_INSTALL_DIR` and, if they want them, Ninja test builds. The Ninja generator is
Multi-Config, so every build names a configuration.

```bash
cmake --build --preset ninja-debug --target client_mini
```

- Build one target per command so its result and log can be checked separately.
- MSVC must be in the environment (`vcvarsall.bat x86`), and the Visual Studio `cmake.exe` is not on
  PATH; use its full path or a shell that has one.
- `engine_mini` and `gameui` collect sources with `file(GLOB_RECURSE ...)` and no `CONFIGURE_DEPENDS`:
  a new `.cpp` there compiles into nothing until the configure step runs again, then fails at link with
  an unresolved symbol. `client_mini` lists its sources instead, so a new file has to be added to
  `nextclient/client_mini/CMakeLists.txt`.

## Deploy

A `--target X` build writes only into `out/bin/<config>/`. The game the developer runs is a separate
copy at `NEXTCLIENT_INSTALL_DIR`, so a green build does not change what the game loads. The `BUILD_ALL`
target builds every module and installs into that directory; otherwise copy the built DLL across by
hand and check the timestamps. This is the most common reason a change appears to do nothing.

## Tests and verification

```bash
cmake --build --preset vs2022-tests-release --target client_mini-tests
```

Test targets are `client_mini-tests`, `engine_mini-tests`, `gameui-tests`, `data_types-tests`,
`view-tests`, `crosshair-tests`, `radar-gl-tests` and `updater-test`. The updater tests additionally require Windows
and `NEXT_LAUNCHER_ENABLE_UPDATER=ON`. They exist only where
`NEXTCLIENT_BUILD_TESTS` is on, which among the tracked presets means `vs2022-tests`; a Ninja test
build comes from a local preset or `-D NEXTCLIENT_BUILD_TESTS=ON`. They cover what runs without the
engine, so the engine-free half of a module is the only half that can be tested at all; keeping logic
on that side is what makes it testable.

Do not launch the game to check a change. The loop is a build plus the unit tests; the developer runs
the game and reports back. When something has to be observed at run time, add a console command that
dumps the state rather than driving the game.

## Code review

Read the applicable directory rules and establish the comparison base before reviewing a branch.
Trace affected callers, owners and consumers outside the diff. Historical sessions explain why a
constraint exists, but an unmerged feature or a superseded implementation is not a fact about the
current checkout.

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

A finding identifies the trigger, the incorrect result and a precise code location, backed by the
actual call path or a reproducing test. Separate confirmed defects from open questions; discard a
candidate when the code proves its precondition cannot occur. Do not report a hypothetical guard
or a preference as a demonstrated bug. State the checks performed and relevant coverage limits.

## Conventions

A directory with rules of its own keeps an `AGENTS.md` next to the code and adds to what is here
rather than restating it: see
[nextclient/client_mini/src/hud/radar/AGENTS.md](nextclient/client_mini/src/hud/radar/AGENTS.md).

**Documentation.** Keep maintained documentation in English (`.md`), with Russian translations
in matching `.ru.md` files. Use `.superpowers/` for design drafts, implementation plans and
session reports; that directory is ignored by Git. Keep procedures, references and test cases
separate, and link each translation to its counterpart.

**Naming.** Methods and functions are `CamelCase`, public free functions `Module_Method`, globals
`g_CamelCase`, `constexpr` constants `kCamelCase` (no underscore, acronyms as words: `Abc`, not
`ABC`), locals `snake_case`, typedefs carry the `_t` suffix. A simple accessor is `snake_case`:
`get_x()` for a plain one, `is_x()` / `are_x()` / `should_x()` for a predicate. Setters keep the `SetX()` form;
`set_x()` exists nowhere here and should not be introduced.

**Predicates.** `Is` is a pure answer read from state already known. `Check` warns that answering
does the work, so the call is neither free nor repeatable at will (a first call that runs a one-time
GL init, creates a font, or fires engine traces). `Ensure` is the make-it-so form that reports
success rather than an answer. A third-person verb already reads as a question and keeps its name
(`Contains`, `Blocks`). A bare adjective or participle (`Available`, `Enabled`) picks none of these.
An accessor that has to load or compute first is not plain and says so on the declaration.

**Comments.** Sparse, English only, explaining what the code cannot: an ordering constraint, a
workaround, a non-local invariant, the engine or driver behavior that forced the shape of the code.
No banners, no line-by-line narration, no doc comment above a function by default, nothing that
restates the name or the signature. A comment on a function states a property of that function, never
where it is called from today nor what a caller does with the answer: callers change, and such a
claim cannot be checked from the file the comment sits in. A `call after X` precondition is allowed
only when it is that function's own correctness condition, and reads better as the dependency it
rests on. Do not describe what something is not unless the negation names the obvious alternative
that was rejected and what it breaks. One fact lives in one place: the same note repeated over
several declarations is a missing named constant. Why a change was made belongs in the commit
message, not next to the code.

**Declarations.** `#pragma once`, never include guards. Member defaults use empty braces when the
default is the zero value (`int x{}`, `T* p{}`), the equals form only for a non-zero or named default
(`float start_ = -1.0f`); locals keep `=`. Prefer explicit types over `= {}`, and `auto` only for
iterators and verbose types. C-style casts are for numeric conversions only; a pointer or layout reinterpretation goes
through `reinterpret_cast` or `memcpy`, never a C-style cast. Brace every
control-flow block. File-scope constants go in an anonymous namespace rather than inside a function.
Includes run own header, STL, external `<...>`, project headers last, with project paths written from
the module's source root and never `../`.

**Code layout.** Write new and modified code for human readability. Separate declarations
and setup, control flow, result processing, and returns with blank lines when they form
distinct logical blocks. Keep nonempty function bodies and control-flow blocks on multiple
lines. Wrap long calls, signatures, and aggregate initializers by argument or element when
their structure is difficult to scan, even if they fit the column limit. Use the root
`.clang-format` for indentation and brace placement; its column limit does not replace
these readability rules. Apply this to touched code without reformatting entire files.

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

Repeat access labels when needed to keep these groups in order. A static constant used in a
member type, such as an array bound, must precede its first use; explain that ordering exception
with a short comment. Preserve the relative order of nonstatic fields and virtual methods when
reorganizing declarations, since they determine object layout, construction order, and the virtual
interface.

**API shape.** Pick by what the thing owns. A class for state with an owner and a lifetime, where the
owner holds the instance and resets it; one header may declare several classes when a single owner
holds all of them, so group a header by owner and never by property. `Module_Function` free functions
for pure helpers, and for singleton state with a process or context lifetime that a C callback or an
engine table has to reach, where an owning object is not possible. A namespace only for a group of
constants that needs a qualifier to read. Types stay at file scope.

**Changes.** Keep them tightly scoped and format only the lines you touched, with the root
`.clang-format`: the committed tree is
not uniformly format-clean, so reformatting a whole file buries the change in noise. Prefer a
deterministic fix to a defensive guard; a check that cannot fire is noise, and a state that cannot
occur should fail loudly rather than be papered over.

## Traps

- Line endings are mixed across the tree. Preserve whatever a file already uses; never normalize a whole
  file, or the diff stops showing the change.
- `assets/cstrike/resource/nextclient_*.txt` are UTF-16 LE and the English one carries a double BOM.
  Edit them through PowerShell `Get-Content`/`Set-Content -Encoding Unicode`; a plain text edit corrupts
  them.
- Engine facts (addresses, globals, function signatures) belong in NitroApi, which resolves them per
  build, not in a hardcoded copy next to the code that needs them.
- Anything shipped to the player also has to exist under `assets/`: a new cvar, scheme entry or
  localization string is only half done in C++.
