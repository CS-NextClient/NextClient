# Testing NextClient

[Russian version](README.ru.md) | [All documentation](../README.md)

Use the tables below to choose a verification procedure. Each procedure describes preparation, actions, and expected results. A successful build, a unit-test result, a server operation, and a visual observation provide separate evidence.

## Setup and operation

| Document | Purpose |
|---|---|
| [Developer tools panel](../tools/developer-panel.md) | Run preparation and comparison tools through explicit forms. |
| [Test server reference](../tools/test-server.md) | Prepare all test plugins, configure the server, and collect reports. |
| [Server compatibility](../tools/test-server-compatibility.md) | Check supported components and platform requirements. |
| [Manual testing procedure](manual-testing.md) | Connect the client, run scenario steps, and record results. |
| [Materials test plugin](materials-test-plugin.md) | Use the material-effect test scenes and in-game menu. |
| [Studio content preparation](../tools/studio-content.md) | Generate models, materials, labels, and maps for rendering checks. |

## Procedures and acceptance cases

| Area | Procedure or cases |
|---|---|
| Radar scenarios | [Manual test cases and role/outcome matrix](manual-testing-scenarios.md) |
| Radar layers | [Verification procedure](radar-layers.md) and [acceptance cases](radar-layer-cases.md) |
| Studio rendering | [Rendering verification](studio-rendering.md) and [rigid instancing](studio-rigid-instancing.md) |
| Material editor | [Scene verification](material-editor-scene.md) |
| Performance monitor | [Runtime checks](performance-monitor.md) |

## Diagnostics and automated checks

| Document | Purpose |
|---|---|
| [Renderer diagnostics](../renderer-diagnostics.md) | Capture renderer state and inspect the resulting files. |
| [Image comparison](../tools/image-comparison.md) | Compare prepared captures and set difference thresholds. |
| [Matgen regression checks](matgen.md) | Verify material validation, UV output, and edge-wear baking. |
| [Developer tools verification](../tools/developer-panel.md#verification) | Check command construction, translation, launch scripts, and files produced by generators in the test cases. |
| [Native module installation and NitroApi checks](installation-and-factory.md) | Verify the CMake installation recipe in temporary destinations and check the built NitroApi library's public interface. |
| [Windows installer checks](installer.md) | Verify new installation, ZIP updates, cancellation, recovery, and removal on synthetic files. |
| [Repository test targets](../../AGENTS.md#tests-and-verification) | Select CMake test targets and the required configuration. |

Record the relevant source revision, build configuration, installed artifacts, selected scenario, and result. Reuse passing evidence for an unchanged source and configuration snapshot; repeat checks when the affected behavior changes or a concrete concern requires it.
