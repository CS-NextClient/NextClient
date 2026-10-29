# Shared packages

These libraries serve several DLLs. Keep their contracts independent of a current consumer; comments describe the helper's own behavior rather than the HUD, radar or dialog that uses it. Coroutine lifetime rules are in [task_coro/AGENTS.md](task_coro/AGENTS.md). Studio model conversion has [additional rules](ncl_studio/AGENTS.md).

## Boundaries

- Look for an existing STL, EASTL, tier1 or package operation before adding another helper. Put a
  reusable operation in the package that owns its data; keep engine state and feature policy in
  the consuming module.
- Use `ncl_math::Vector2`, `Vector3`, `Color` and `ColorF` for owned calculations. A raw `float*`
  belongs at a real engine or GL boundary, not in an internal API that immediately wraps it again.
  Check layout, alignment and lifetime when using `Vector3::Ref`; use `.data()` for outward calls
  that require contiguous components.
- Prefer the existing normalization and near-zero helpers. Choose tolerances for the actual
  quantity being tested; one epsilon must not serve unrelated distances, angles and normalized
  vectors. Keep zero-length behavior explicit.
- Keep `ncl_math` free of engine and rendering dependencies. A helper does not become generic
  merely by moving a feature-specific constant into this directory.

## Protocol and GL helpers

- `MessageReader` reads non-owned data. Keep the input alive and inspect overflow when
  decoding a message; a sentinel return value alone can also be a valid protocol value. Preserve
  stock byte order, string termination and truncation behavior when changing the reader or writer.
- `ncl_gl` helpers require a live GL context and an initialized function table. Keep ownership of
  returned programs and textures explicit in the caller. Uniform names and stored locations must
  keep the same order; an optimized-out uniform may legitimately have location `-1`.
- `ncl_gl::Program` owns its named uniform and block lookup caches, including inactive results. A successful in-place relink must call `ResolveLocations` and invalidate consumer configuration. Numeric GL names alone do not identify a program across reloads or context changes.

## Verification

Build affected consumers, not just the static archive. Public headers can expose new dependencies or compile errors only when another DLL includes them. Protocol changes have `data_types-tests`; math changes are also exercised by the engine-free client tests. Shared gameplay logic has `view-tests` and `crosshair-tests`. Select checks for the changed contract instead of assuming every package has a standalone test target.

## Performance accounting

[`ncl_perf`](ncl_perf/AGENTS.md) owns CPU accounting, shared sample types, capacities and profiling helpers. The engine's frame, output, snapshot, GPU and presentation policy stays in the [performance subsystem](../engine_mini/src/performance/AGENTS.md).
