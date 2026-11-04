# Working with NitroApi

[Russian version](nitro-api.ru.md) | [Project architecture](architecture.md) | [Frame lifecycle](performance-architecture.md#frame-lifecycle)

NitroApi gives NextClient modules access to stock game functions and data. For example, the monitor uses NitroApi to identify the start and end of each frame. Information about supported engine versions is kept here, so individual modules do not have to handle their differences independently.

## Before making a change

Identify which module needs the new feature and which engine data it requires. Check whether NitroApi already provides the necessary access. Changes must account for supported engine versions on Windows and Linux.

Read the [module rules](../nextclient/nitro_api/AGENTS.md). The [ABI guide](goldsrc-abi.md) describes how to prepare headers for engine compatibility.

## Verification

Build NitroApi and the modules that use the changed interface from the same source revision and with matching build settings. Run the relevant tests in `nitro_api-tests`. When changing a shared interface, follow the [library verification procedure](testing/installation-and-factory.md) to check that the built library loads and provides that interface.

Builds and tests without the game verify only part of compatibility. The developer checks interaction with the stock engine in the installed game and records that result separately.
