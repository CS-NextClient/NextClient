# NextClient

A modification of the Counter-Strike 1.6 client. It ships replacement DLLs alongside the stock game and
patches Valve's engine (`hw.dll`) at run time rather than replacing it. A Steam copy of the game is
required.

## Modules

| Path | Ships as | What it is |
|---|---|---|
| `nextclient/launcher` | `cstrike.exe` | process entry point, updater, whole-process relaunch |
| `nextclient/engine_mini` | `next_engine_mini.dll` | runtime detours and byte patches of the stock `hw.dll` |
| `nextclient/client_mini` | `cstrike/cl_dlls/client_mini.dll` | HUD, view, weapons, radar |
| `nextclient/gameui` | `cstrike/cl_dlls/GameUI.dll` | VGUI menus and the CEF-backed HTML UI |
| `nextclient/filesystem_proxy` | `FileSystem_Proxy.dll` | filesystem interposer |
| `nextclient/steam_api_proxy` | `steam_api.dll` | Steam API interposer |
| `nextclient/packages` | static libs | shared code: `ncl_math`, `ncl_utils`, `ncl_gl`, `task_coro`, `data_*`, `hwid_collector` |
| `nextclient/nitro_api` | `nitro_api2.dll` | engine address and hook layer |
| `nextclient/goldsrc_abi` | headers only | clean-room declarations of the stock engine ABI, namespace `goldsrc` |
| `dep/ncl-hl1-source-sdk` | `vgui2.dll` | HL1 SDK fork: `tier1`, `tier2`, `vgui2`, `mathlib`, `cef`, HLTV |
| `assets/` | copied into the game directory | resources, schemes, localisation, shaders |
| `tools/` | not shipped | offline test harnesses (`radar_tests`, `studio_tests`) |

Everything is 32-bit (`x86-windows-static`), because the engine it loads into is. Third-party
dependencies come from the vendored `vcpkg/`, wired in by the `base` configure preset.

## Build

Tracked configure presets are `ninja`, `vs2022`, `vs2022-tests` and `vs2026`; the build presets that go
with them are `ninja-debug`, `ninja-release` and the `vs*` pairs. `CMakeUserPresets.json` is untracked
and machine-local: that is where a developer adds `ninja-local`-style presets carrying their own
`NEXTCLIENT_INSTALL_DIR` and, if they want them, Ninja test builds. The Ninja generator is
Multi-Config, so every build names a configuration.

```bash
cmake --build --preset ninja-debug --target client_mini
```

- One target per `--build`. Passing two fails before anything is compiled.
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

Test targets are `client_mini-tests`, `engine_mini-tests` and `updater-test`. They exist only where
`NEXTCLIENT_BUILD_TESTS` is on, which among the tracked presets means `vs2022-tests`; a Ninja test
build comes from a local preset or `-D NEXTCLIENT_BUILD_TESTS=ON`. They cover what runs without the
engine, so the engine-free half of a module is the only half that can be tested at all; keeping logic
on that side is what makes it testable.

Do not launch the game to check a change. The loop is a build plus the unit tests; the developer runs
the game and reports back. When something has to be observed at run time, add a console command that
dumps the state rather than driving the game.

## Conventions

A directory with rules of its own keeps an `AGENTS.md` next to the code and adds to what is here
rather than restating it: see
[nextclient/client_mini/src/hud/radar/AGENTS.md](nextclient/client_mini/src/hud/radar/AGENTS.md).

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
workaround, a non-local invariant, the engine or driver behaviour that forced the shape of the code.
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

- Line endings are mixed across the tree. Preserve whatever a file already uses; never normalise a whole
  file, or the diff stops showing the change.
- `assets/cstrike/resource/nextclient_*.txt` are UTF-16 LE and the English one carries a double BOM.
  Edit them through PowerShell `Get-Content`/`Set-Content -Encoding Unicode`; a plain text edit corrupts
  them.
- Engine facts (addresses, globals, function signatures) belong in NitroApi, which resolves them per
  build, not in a hardcoded copy next to the code that needs them.
- Anything shipped to the player also has to exist under `assets/`: a new cvar, scheme entry or
  localisation string is only half done in C++.
