# Matgen regression checks

[Russian translation](matgen.ru.md)

Build `matgen`, then run the three standalone tests below with Python 3 and the executable path. Each test uses only the Python standard library, generates models in a temporary directory, and removes its outputs after completion.

```text
python tools/matgen/tests/repeat_test.py out/bin/Release/matgen.exe
python tools/matgen/tests/reference_pose_test.py out/bin/Release/matgen.exe
python tools/matgen/tests/output_collision_test.py out/bin/Release/matgen.exe
```

The first test checks horizontal and vertical repetition in UV templates and wear maps, including negative and extreme coordinates and a one-pixel source texture. Moving the UV layout by whole texture repetitions must not change the image. A separate case checks processing time for a 2x2 source texture and a 2048x2048 output. Each `matgen` run has a 30-second limit.

The second test checks that different skeleton structures produce the same wear map when the model geometry is identical in the reference pose. The output images must match pixel for pixel. It also verifies UV and edge output names for plain, unsafe, and empty texture names.

The output-collision test uses two texture records with separate meshes and skin mappings. It checks sanitized names, ASCII case, empty-name fallbacks, and skipped edge textures. Collisions must return an explicit error before replacing a preexisting output or writing another PNG. Noncolliding edge outputs must both contain geometry.
