# NitroApi

This subtree owns build-specific addresses, stock function signatures and hook registration.
The engine and client modules consume those facts through `EngineData` and `ClientData`.

## Extending engine access

- A new exposed function normally touches the public data declaration, `StaticHookId`, module
  registration and the relevant address provider. For a variable, pair its declaration with
  `RegisterVar` and provider data. Check the complete path; a compiling declaration alone does
  not resolve an address at runtime.
- Match the calling convention, return type, pointer depth and layout to the engine ABI. Reuse
  `goldsrc` types instead of recreating a nearby SDK struct. Hook adapters must obey the original
  ABI even when a higher-level wrapper would accept a similar C++ type.
- Keep existing public data-member offsets and static hook IDs stable when extending the API.
  Append data members at the current end, not at a historical insertion point from an old session.
  Check enum values when adding a hook because many IDs are implicit increments.
- Windows and Linux have separate build-8684 address providers. Verify each supported path affected
  by a change; a Windows build cannot validate a Linux signature or address.
- A chained hook's `next` controls execution of the remaining chain. Call it according to the
  intended replacement/forwarding contract and preserve the return value; duplicate forwarding
  can repeat engine work, while an omitted call can suppress unrelated subscribers.

## Headers and subtree history

- Keep stock ABI declarations in [goldsrc_abi](../goldsrc_abi/AGENTS.md) and NitroApi's own API in
  this subtree. Public headers should forward-declare pointer-only dependencies where possible.
- `include/nitroapi/modules/client/GameHud.h` deliberately forward-declares `class Vector`. Including
  `<mathlib/vector.h>` there brings dynamic initialization and tier0 link requirements into Debug
  consumers; include it only in consumers that need the definition.
- The private `dep/easyloggingpp` copy differs from the repository's other copy. Deduplicating it
  changes behavior and requires a separate, explicit change.
- For pre-subtree history, inspect both path spellings:
  `git log -- nextclient/nitro_api/<path> <path>`. Before removing old submodule metadata or storage,
  check other worktrees and refs that still use it. See the
  [maintenance reference](../../docs/goldsrc-abi.md#frozen-extraction-paths) for the frozen extraction paths.

## Verification

Build `nitro_api` and the affected consumers after a public API change. `goldsrc_abi_golden` checks
32-bit layouts during the build; it does not verify address-provider matches or live hook order.
For a change to public ABI or DLL boundaries, compare the relevant exports and layouts as well as
compilation results, and state which runtime behavior remains unverified.
