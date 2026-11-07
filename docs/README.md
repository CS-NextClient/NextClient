# NextClient documentation

[Russian version](README.ru.md)

The [architecture overview](architecture.md) describes project modules and their purpose. The [build instructions](building.md) explain how to prepare a development environment. Other guides are grouped by task below.

## Installation

[Windows installation](installation.md) explains how to create a game copy, update an existing installation, recover an interrupted operation, and remove the game.

## Architecture

These sections describe module responsibilities and interactions.

| Guide | Question answered |
|---|---|
| [Project architecture](architecture.md) | Which modules make up the project, and what does each one do? |
| [Studio renderer](renderer-architecture.md) | How is rendering shared between the engine, NextClient, and the material editor? |
| [Performance monitor](performance-architecture.md) | What does the monitor measure, where does its data come from, and how does it handle skipped and interrupted frames? |

## Developer tools and workflows

These guides cover building, configuration, and diagnostics. The [developer panel](tools/developer-panel.md) provides a graphical interface for running utilities.

| Task | Guide |
|---|---|
| Build the client and configure an IDE | [Project setup](building.md) |
| Package the Windows release installer | [Installer build](tools/installer.md) |
| Run content and server tools through forms | [Developer tools panel](tools/developer-panel.md) |
| Prepare and operate a dedicated test server | [Test server](tools/test-server.md) and [compatibility](tools/test-server-compatibility.md) |
| Inspect frame cost and capture Tracy traces | [Monitor settings, graphs, and diagnostics](performance-monitor.md) |
| Inspect map geometry and object properties | [Map inspection](performance-limits.md) |
| Compare renderers and save images and measurements | [Renderer diagnostics](renderer-diagnostics.md) |
| Create and edit materials | [Material authoring](weapon-materials-authoring.md) |
| Export UVs, validate materials, or bake wear | [matgen](tools/matgen.md) |
| Generate material images or Studio test scenes | [Material images](tools/material-masks.md) and [Studio content](tools/studio-content.md) |
| Compare reference and candidate captures | [Image comparison](tools/image-comparison.md) |
| Apply materials from a server plugin | [Server material workflow](weapon-materials-server-guide.md) |
| Change and verify engine integration | [Using NitroApi](nitro-api.md) |
| Maintain engine declarations and regenerate headers | [GoldSrc ABI maintenance](goldsrc-abi.md) |
| Investigate antivirus detections in a release | [False positive reporting](reporting-av-false-positives.md) |

## Formats and behavior references

Use these references for exact parameters, file formats, interoperability, and observable behavior.

| Reference | Scope |
|---|---|
| [Weapon material format](weapon-materials.md) | MAT documents, program manifests, image paths, and limits |
| [Material effects](material-effects.md) | Effect parameters and their visible behavior |
| [Server material API](weapon-materials-server-api.md) | Plugin API, synchronization, and client validation |
| [Radar layers](radar-layers.md) | Layer selection, correction format, and diagnostic fields |
| [Master server HTTP protocol](master-server-http-protocol.md) | Client/backend request and response contract |

## Verification

The [testing index](testing/README.md) selects procedures and acceptance cases for radar, rendering, the material editor, monitoring, tools, and installation. A procedure defines preparation, actions, and expected results. Results of a particular run belong to its report with the source/build identity and logs.
