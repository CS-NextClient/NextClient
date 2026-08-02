# Reporting antivirus false positives

NextClient binaries (especially `nitro_api2.dll`, which detours engine functions at runtime via funchook and xbyak) periodically trigger heuristic detections. All official binaries are built from the public source code by GitHub Actions ([build.yml](../.github/workflows/build.yml) / [release.yml](../.github/workflows/release.yml)), and release archives carry a signed build provenance attestation; this verifiable origin is the core argument of every false positive report below. This document describes how to get such detections removed. Last verified: July 2026.

## General workflow

1. Upload the file to [VirusTotal](https://www.virustotal.com) (or find the existing report by SHA256) and note which vendors flag it and under which detection names.
2. Prefer submitting the official release artifact rather than a local build: the official artifact is built from source by the public GitHub Actions pipeline and covered by its signed provenance attestation (an argument a local build cannot offer), and some vendors whitelist by hash, while a dev build hash changes with every rebuild. Fixes to the heuristic signature itself generalize to future builds either way.
3. For each flagging vendor, submit a false positive report using the forms below. Always include: file name, SHA256, VirusTotal link, detection name, the statement that the binary is built from public source code by GitHub Actions, and a short explanation of what the module does (runtime API hooking of the game engine inside the game process - the usual heuristic trigger).
4. For vendors not listed here, look up the submission channel in the [False-Positive-Center](https://github.com/yaronelh/False-Positive-Center) aggregator.

Notes on VirusTotal tags: `detect-debug-environment` is a sandbox behavior tag (IsDebuggerPresent and similar calls from crash handlers), not a vendor verdict; there is nothing to dispute about it. An `invalid-signature` tag, however, raises heuristic scores by itself - make sure release binaries are signed correctly.

## Vendor submission channels

### Bitdefender (root source of Gen:Variant.* detections)

Consumer form: <https://www.bitdefender.com/consumer/support/answer/29358/> ("Report an incorrect detection"); business products: <https://www.bitdefender.com/business/support/en/77209-343057-submitting-sample-files-and-websites-for-analysis.html>

`Gen:Variant.*` signatures belong to the Bitdefender engine, which is licensed by many OEM products (Emsisoft, eScan, GData, VIPRE, ALYac, Arcabit, CTX). Fixing the signature at Bitdefender clears the whole group, so report here first when several of these vendors flag at once.

- "Detection name" must be entered exactly as the product displays it (OEM products may show variants such as `Gen:Variant.jaik.300143 (B)` in Emsisoft).
- Attach the flagged file itself; zip it if larger than 25 MB.
- The sensitive-files checkbox is mandatory; a screenshot of the detection or the VirusTotal page (jpg/gif/jpeg/png) is optional but speeds up processing; a captcha completes the form.
- Confirmed false alarms are typically corrected within hours.

### Microsoft (Defender, SmartScreen)

Form: <https://www.microsoft.com/en-us/wdsi/filesubmission> (sign-in with a Microsoft account required; submitter type "Software developer").

- In "What do you believe this file is?" select `Incorrectly detected as malware/malicious`, or `Incorrectly detected as PUA (potentially unwanted application)` if the disputed verdict is a PUA one.
- "Detection name" is required; enter it exactly as shown in the Protection History (e.g. `Trojan:Win32/Wacatac.C!ml`; the `!ml` suffix means an ML verdict, a frequent false positive on unsigned self-built binaries).
- "Definition version" is optional but recommended; read it from `Get-MpComputerStatus | Select-Object AntivirusSignatureVersion` on the machine where the detection appeared.
- Put the report text into "Additional information" (required, English, max 1900 characters).
- "Company Name" is a required field; enter `Independent developer`.
- For "Select the Microsoft security product used to scan the file", choose `Microsoft Defender Antivirus (Windows 11)` (or the Windows 10 variant, matching the OS where the detection appeared); pick `Microsoft Defender SmartScreen` only when the block comes from the SmartScreen reputation filter rather than the antivirus engine.

### Trend Micro (including HouseCall, TROJ_GEN.* detections)

Form: <https://www.trendmicro.com/en_us/about/legal/detection-reevaluation.html>

- Request type: "False Alarm on a File(s)".
- Attach the file as ZIP or RAR, max 12 MB, password must be the word `virus`.
- The "Details about the Problem" field must mention the archive file name; the contact e-mail goes into a separate field.

### VIPRE (Gen:Variant.* detections)

Form: <https://www.vipre.com/support/submit-false-positive/> (fallback: <https://helpdesk.vipre.com/hc/en-us/requests/new>)

VIPRE licenses the Bitdefender engine; `Gen:Variant.*` names are Bitdefender signatures, so report them to Bitdefender first (see above). Submit to VIPRE directly when VIPRE flags while Bitdefender-engine vendors (Bitdefender, GData, Emsisoft, eScan, ALYac) already show clean on VirusTotal - in that case the detection is likely an outdated definition set on VIPRE's side, and that is worth mentioning in the report.

### Antiy-AVL

No web form; e-mail only, and the channel is unreliable (community reports confirm some of their published addresses do not exist; `avlsdk_support_vt@antiy.cn`, previously listed for VirusTotal detections, bounced as non-existent in July 2026).

Send one e-mail to `avlsdk_support@antiy.cn` (the AVL SDK false positive address listed by current aggregators), CC `support@antiy.cn` (the only address confirmed on the [official contacts page](https://www.antiy.net/contacts/)) and `submit@antiy.com` (historical sample-submission address). Attach the sample as a ZIP with password `infected`, or provide just the SHA256 and VirusTotal link.

If all three bounce, there is no known working channel and the report can be skipped: Antiy heuristic detections often clear with definition updates without a request.

### MaxSecure

False positive form: <https://www.maxsecureantivirus.com/submit_aFalse_Positive.htm> (fallback e-mail: `tech@maxpcsecure.com`).

The form takes no attachment: state in the message that the file is a false positive and include the VirusTotal link plus a download link; the vendor replies by e-mail. The site's TLS certificate is frequently expired, so the browser may warn before the form loads - the channel still works. The generic contact form <https://www.maxpcsecure.com/contact.htm> also reaches support but is not false-positive specific.

### Cynet

No public false positive form for outside developers (in-console allow-listing exists only for Cynet customers). Report by e-mail to `soc@cynet.com` - the Cynet SOC / CyOps address, taken from false-positive aggregators rather than an official vendor page. Include the file name, SHA256, VirusTotal link, and detection name.

### Gridinsoft (no cloud submission)

Incorrect-detection form: <https://gridinsoft.com/incorrect-detection> (fallback e-mail: `virus@gridinsoft.com`).

Gridinsoft has no cloud submission or online whitelisting system; reports go through this form, the fallback e-mail, or the in-product "Let us know" / "False positive" reporter. Provide a contact e-mail, a link to the file, and the exact detection name; a screenshot of the detection helps.

## Report templates

Adjust the vendor-specific parts (detection name, archive password) per the sections above. Keep the text short: an FP analyst needs file identification, the statement that it is clean, built from public source code by a public CI pipeline (GitHub Actions), not packed and not obfuscated, the detection name, the sample, and the fact that independent engines report it clean. Speculative explanations of why the heuristic fired only dilute the report.

The templates below share the identification header, the build provenance paragraph, and the closing; only the module description differs. For any other NextClient binary (next_engine_mini.dll, client_mini.dll, GameUI.dll, vgui2.dll), take the closest template and adjust the module description.

### nitro_api2.dll

```
Detection name: <detection name> (<vendor/engine>)
File name: nitro_api2.dll
SHA256: <sha256>
VirusTotal report: https://www.virustotal.com/gui/file/<sha256>

The file is a component of NextClient, an open-source enhanced game client for Counter-Strike 1.6 (GoldSrc engine). Source code: https://github.com/CS-NextClient/NextClient

The binary is built from that public source code by GitHub Actions in the same repository: the workflow definitions and full build logs are public, and official release archives carry a signed build provenance attestation (https://github.com/CS-NextClient/NextClient/attestations) that links each artifact to the exact commit and workflow run that produced it. The file is not built or modified outside this public CI pipeline, and it is not packed or obfuscated.

This module implements runtime API hooking of the game engine inside the game's own process (it uses the open-source funchook and xbyak libraries to detour engine functions). This is legitimate, documented behavior required to extend the game engine, and it is the likely cause of the heuristic detection. The DLL is loaded only by the game executable and performs no network communication, persistence, or actions outside the game process.

Please reevaluate the detection.
```

### FileSystem_Proxy.dll

```
Detection name: <detection name> (<vendor/engine>)
File name: FileSystem_Proxy.dll
SHA256: <sha256>
VirusTotal report: https://www.virustotal.com/gui/file/<sha256>

The file is a component of NextClient, an open-source enhanced game client for Counter-Strike 1.6 (GoldSrc engine). Source code: https://github.com/CS-NextClient/NextClient

The binary is built from that public source code by GitHub Actions in the same repository: the workflow definitions and full build logs are public, and official release archives carry a signed build provenance attestation (https://github.com/CS-NextClient/NextClient/attestations) that links each artifact to the exact commit and workflow run that produced it. The file is not built or modified outside this public CI pipeline, and it is not packed or obfuscated.

This module is a thin pass-through wrapper around the game's standard file system module: it implements Valve's IFileSystem interface, loads the stock filesystem_stdio.dll and forwards every call to it. It adds only two in-process conveniences: a path aliasing extension used by the client, and, for the single MOTD temporary file consumed by the embedded browser (CEF), a re-encoding of the returned local path to UTF-8 so that non-Latin install paths (e.g. Cyrillic) are handled correctly. It performs no code hooking or patching, no network communication, and no file access beyond the ordinary game file I/O requested by the engine. The DLL is loaded only by the game executable.

Please reevaluate the detection.
```

### cstrike.exe

```
Detection name: <detection name> (<vendor/engine>)
File name: cstrike.exe
SHA256: <sha256>
VirusTotal report: https://www.virustotal.com/gui/file/<sha256>

The file is a component of NextClient, an open-source enhanced game client for Counter-Strike 1.6 (GoldSrc engine). Source code: https://github.com/CS-NextClient/NextClient

The binary is built from that public source code by GitHub Actions in the same repository: the workflow definitions and full build logs are public, and official release archives carry a signed build provenance attestation (https://github.com/CS-NextClient/NextClient/attestations) that links each artifact to the exact commit and workflow run that produced it. The file is not built or modified outside this public CI pipeline, and it is not packed or obfuscated.

This file is the game launcher. It loads the client modules and the stock GoldSrc engine and runs the game. The auto-updater, crash reporting (Sentry) and anonymous analytics are compile-time options that are disabled in the public GitHub Actions build - the release workflow builds the same CI job with these options off - so the official artifact downloads nothing, sends no telemetry, initializes no crash reporter, and makes no network connections of its own. It creates no autorun entries, services, or scheduled tasks, and writes settings only to the game's own registry keys under HKCU\Software\Valve\Half-Life. After an in-game restart it starts a new instance of itself and exits, which is ordinary launcher behavior and the likely cause of the heuristic detection.

Please reevaluate the detection.
```

### steam_api.dll

```
Detection name: <detection name> (<vendor/engine>)
File name: steam_api.dll
SHA256: <sha256>
VirusTotal report: https://www.virustotal.com/gui/file/<sha256>

The file is a component of NextClient, an open-source enhanced game client for Counter-Strike 1.6 (GoldSrc engine). Source code: https://github.com/CS-NextClient/NextClient

The binary is built from that public source code by GitHub Actions in the same repository: the workflow definitions and full build logs are public, and official release archives carry a signed build provenance attestation (https://github.com/CS-NextClient/NextClient/attestations) that links each artifact to the exact commit and workflow run that produced it. The file is not built or modified outside this public CI pipeline, and it is not packed or obfuscated.

This module is not a Steam emulator and does not alter, emulate, or bypass any Steam functionality. It is a thin forwarding wrapper: it exports the standard steam_api.dll functions and forwards every call unchanged to the original Steam API library, which resides next to it as steam_api_orig.dll. Its only addition is one extra export (NextSteamProxy_SetSEH) through which the game launcher registers a crash-handler callback, so that crash minidumps written through the Steam API also reach the client's own crash reporting. A third-party module named steam_api.dll is a common game-crack heuristic trigger, which is the likely cause of the detection. The DLL is loaded only inside the game process and performs no activity beyond forwarding these calls.

Please reevaluate the detection.
```
