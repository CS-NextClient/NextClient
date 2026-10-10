# Stock engine ABI declarations

This is the clean-room header library for stock interoperability facts. Start with the
[maintenance reference](../../docs/goldsrc-abi.md) for generation, verification and frozen inputs.

## Source of truth

- `tools/library.py` generates declarations from pinned Windows facts in `tools/facts.json`,
  the Linux supplement in `tools/facts_linux.json`, and `tools/header_map.json`. Both platform
  models feed headers and layout assertions. Preserve provenance; do not reconstruct declarations
  by copying deleted SDK implementation, comments or formatting.
- Stock declarations belong to `goldsrc` and its stock namespaces; NextClient extensions belong
  to their owning module. Do not put a custom field into a stock layout to simplify a consumer.
- Preserve full size and alignment even for records exposing only a prefix. Opaque tails preserve
  array stride, allocation size and engine writes beyond the fields a consumer reads. Check field
  offsets, packing and calling conventions on the 32-bit target.
- Generated portions and maintained additions have different ownership. Consult the maintenance reference and
  generator before editing, and verify regeneration preserves maintained additions. Update the
  model and golden layout expectations together when changing generated declarations.
- Historical paths such as `dep/NclNitroApi` remain in extraction inputs intentionally. The pinned
  facts describe the pre-migration tree; fresh extraction against the migrated tree is not an
  equivalent input. Do not modernize individual frozen paths without a migration of the inputs.

## Generation and checks

Run these from the repository root:

```text
python nextclient/goldsrc_abi/tools/library.py check
python -m unittest discover -s nextclient/goldsrc_abi/tools -t nextclient/goldsrc_abi/tools -p "test_*.py"
```

`library.py headers --out <directory>` and `library.py tests --out <directory>` generate headers
and `golden_layout.cpp`. Use a comparison directory inside the repository so clang-format finds
the same configuration. Inspect the diff before replacing tracked output. The `goldsrc_abi_golden`
object target compiles the layout assertions; it is not a runtime test executable.

For provenance checks, request CMake's `codemodel-v2` before configuring and run the gate after
building, when generated resource sources exist. The exact pipeline and allowlist are in
[the build workflow](../../.github/workflows/build.yml). Do not expand the allowlist just to make
a newly introduced dependency pass.

After source deletion or build-graph changes, configure again before claiming the tree builds.
Dependency comparisons must use objects in the current target graph, not stale entries from Ninja's
log. A new verification script needs a failing control case; an empty selection or a table of fixed
success values is not evidence that the checked property holds.
