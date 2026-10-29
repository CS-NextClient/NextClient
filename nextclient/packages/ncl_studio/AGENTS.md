# Studio model geometry

This package converts and validates studio model data without engine or GL dependencies. The shader renderer's ownership and batching are described in [its architecture reference](../../../docs/renderer-architecture.md).

- Treat offsets, counts, command lists and bone indices as untrusted file data. Bound reads
  by the supplied buffer size, which can end before the header's declared file length when
  the engine keeps embedded texture pixels outside its model cache allocation.
- Bound aggregate body-part, submodel and mesh visits before traversal, including empty geometry and the preliminary estimate. Charge repeated shell-normal initialization by the declared source vertex count. Shared table references remain valid within the source-sized conversion budget; emitted vertices and indices retain their separate format-derived bounds.
- Preserve strip/fan winding and the ranges that map body parts, submodels and meshes into
  the shared vertex/index arrays. Body variant selection belongs to the consumer.
- Keep UV coordinates in texel space. The active skin family selects the texture dimensions
  at draw time; conversion cannot normalize against one fixed skin.
- Keep the glow shell's normal and its bone consistent for every emitted copy of one source
  vertex. Extruding hard-edge copies by different face normals tears the shell apart.
- The converter currently emits vertices per command-list entry. A deduplication change must
  preserve UVs, lighting normals, bone indices, shell normals and all consumer range contracts.
- Keep GPU allocation, material resolution, engine cache identity and draw ordering in the
  consumer. The converter's CPU arrays can be released after upload; consumers still need
  its mesh, submodel and body-part ranges.
- `ReadStudioMetadata` owns bounds, bounded sequence names and timing, body bases, and bounded submodel names. `ReadStudioTextures` owns texture and skin tables. Both accept the supplied cached-block extent without requiring embedded pixels or full geometry validation. Share table-bound arithmetic through the private `table_bounds.h`; preserve widened skin-dimension products. Body-choice limits belong to consumers.
- `ReadStudioReferencePose` composes default bone values through the parent hierarchy in model space. Bone angles are radians about X, Y, Z, composed as `Rz * Ry * Rx`. Points include translation; normals use only the rotation of their independent normal bone.

Use the affected studio conversion/validation cases in `client_mini-tests` for behavioral changes, then build the affected consumers under the parent package and engine rules.
