# Matgen regression checks

[Russian translation](matgen.ru.md)

Build `matgen`, then run the three standalone tests below with Python 3 and the executable path. Each test uses only the Python standard library, generates models in a temporary directory, and removes its outputs after completion.

```text
python tools/matgen/tests/repeat_test.py out/bin/Release/matgen.exe
python tools/matgen/tests/reference_pose_test.py out/bin/Release/matgen.exe
python tools/matgen/tests/output_collision_test.py out/bin/Release/matgen.exe
```

The repeat test covers UV and baked-edge coverage at horizontal and vertical repeat seams, barycentric interpolation, negative coordinates, whole-tile translation invariance, and thin triangles spanning signed-short UV limits, including a one-pixel source texture. A separate 2x2 texture case bakes the full signed-short range at 2048x2048 output to bound repeated-coverage work. Each `matgen` invocation within these tests has a 30-second timeout.

The reference-pose test compares baked pixels for identical model-space geometry encoded with translated bones, rotated bones, a parent hierarchy, and independent position/normal bones. Every result must match the identity-pose image exactly. It also verifies UV and edge output names for plain, unsafe, and empty texture names.

The output-collision test uses two texture records with separate meshes and skin mappings. It checks sanitized names, ASCII case, empty-name fallbacks, and skipped edge textures. Collisions must return an explicit error before replacing a preexisting output or writing another PNG. Noncolliding edge outputs must both contain geometry.
