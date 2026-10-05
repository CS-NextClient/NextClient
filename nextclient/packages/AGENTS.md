# Shared packages

These libraries serve several DLLs. Keep their contracts independent of a current consumer;
comments describe the helper's own behavior rather than the HUD, radar or dialog that uses it.
Coroutine lifetime rules are in [task_coro/AGENTS.md](task_coro/AGENTS.md).

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

## Verification

Build affected consumers, not just the static archive. Public headers can expose new dependencies
or compile errors only when another DLL includes them. Shared gameplay logic has `view-tests` and
`crosshair-tests`. Select checks for the changed contract instead of assuming every package has a
standalone test target.
