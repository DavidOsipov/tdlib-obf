<!--
SPDX-FileCopyrightText: Copyright 2026 telemt community
SPDX-License-Identifier: MIT
telemt: https://github.com/telemt
telemt: https://t.me/telemtrs
-->

# Building tdlib-obf for Android with NDK r27

This note documents how the `tdlib-obf` fork is built for Android using the
stable, widely available **Android NDK r27** toolchain, and how the
`fix/ndk27-tdlib-obf-build` branch unblocks that build.

## TL;DR — the fix

The only thing that blocked the NDK r27 Android build was a **dependency gate**,
not the toolchain and not the TDLib version:

```text
CMake Error: zlib 1.3.0.1 is too old. Minimum supported version is 1.3.2.
```

NDK r27 (`27.1.12297006`) ships **zlib 1.3.0.1** in its sysroot
(`.../sysroot/usr/lib/<abi>/<api>/libz.so`). A previous hardening change raised
the project-wide zlib floor to `1.3.2` (to avoid CVE-2026-22184 in zlib's
`contrib/untgz` helper). That floor is correct for desktop builds, but
`contrib/untgz` is **not compiled into Android builds**, and tdlib-obf only uses
long-stable zlib APIs (`deflate`/`inflate`, `gz*`, `crc32`), none of which were
introduced after 1.3.0.1.

This branch makes the floor **Android-aware**: NDK-sysroot zlib below `1.3.2` is
accepted **only** when cross-compiling for Android (`ANDROID` /
`CMAKE_SYSTEM_NAME STREQUAL "Android"`). Desktop builds (Linux/macOS/Windows)
remain strict at `1.3.2`. The accepted zlib is printed at configure time, so the
build never silently downgrades.

No NDK downgrade to r29/r30, no mandatory OpenSSL 3.5.x, no mandatory Clang 22,
and no global weakening of dependency requirements were needed.

## Requirements

| Component       | Required                                                        |
|-----------------|-----------------------------------------------------------------|
| Android SDK     | `platforms;android-34`, `build-tools;34.0.0`, `cmake;3.22.1`     |
| Android NDK     | **`27.1.12297006`** (a.k.a. `r27b`) — `r27.x`, not `r29`/`r30`   |
| CMake (project) | minimum `3.10`; Android scripts use the SDK-pinned `3.22.1`      |
| OpenSSL         | **`3.0.13`** (minimum supported). `3.5.6+` is *recommended only* |
| zlib            | NDK-sysroot **`1.3.0.1`** (accepted for Android; see above)      |
| C++ compiler    | NDK r27 Clang (~18). Clang `22.1.3+` is *recommended only*       |
| Host tools      | bash, JDK, Python 3, perl, gperf, make                          |

TDLib baseline used by this fork: **1.8.63** (see `CMakeLists.txt:15`). The NDK
r27 build does **not** depend on the TDLib version; see "TDLib baseline" below.

## zlib strategy

* **Source:** NDK r27 sysroot zlib (not vendored).
* **Version:** `1.3.0.1`.
* **Include path (example, arm64-v8a):**
  `<NDK>/toolchains/llvm/prebuilt/<host>/sysroot/usr/include/zlib.h`
* **Library path (example, arm64-v8a, API 21):**
  `<NDK>/toolchains/llvm/prebuilt/<host>/sysroot/usr/lib/aarch64-linux-android/21/libz.so`

The selection is logged via the configure-time summary (see below). To instead
vendor a newer zlib, set `-DZLIB_ROOT=<prefix>` (or `ZLIB_INCLUDE_DIR` +
`ZLIB_LIBRARY`) for the per-ABI configure; the Android-aware allowance only
triggers when the resolved zlib is older than `1.3.2`.

## OpenSSL

* Built per-ABI by `example/android/build-openssl.sh` (default `openssl-3.0.13`).
* Resolved deterministically via `-DOPENSSL_ROOT_DIR="$OPENSSL_INSTALL_DIR/$ABI"`
  in `build-tdlib.sh`, so the host OpenSSL is never linked into Android builds.
* `3.0.13` satisfies the minimum gate; `3.5.6` is only a recommendation
  (`WARNING`, never fatal).

## Configure-time build summary

For Android cross-compiles the configure step now prints a summary so a
downstream consumer can confirm exactly what the artifact was built against:

```text
-- ==== tdlib-obf Android build summary ====
--   Android ABI:            arm64-v8a
--   Android API level:      android-21 (native 21)
--   Android NDK:            <path>/android-ndk-r27b
--   Android NDK revision:   27.1.12297006
--   C++ compiler:           Clang 18.x.x
--   zlib version:           1.3.0.1
--   zlib include dir:       .../sysroot/usr/include
--   zlib library:           .../sysroot/usr/lib/aarch64-linux-android/21/libz.so
--   OpenSSL version:        3.0.13
--   tdlib-obf git revision: <hash>
-- =========================================
```

## Exact build commands

From `example/android`:

```bash
# 1. Verify host tooling
./check-environment.sh

# 2. Fetch the Android SDK + NDK r27b (27.1.12297006), cmake 3.22.1, android-34
./fetch-sdk.sh                 # downloads into ./SDK
#   …or reuse an existing SDK that already has ndk;27.1.12297006:
#   ./build-openssl.sh <SDK_ROOT> r27b
#   ./build-tdlib.sh   <SDK_ROOT> r27b

# 3. Build OpenSSL 3.0.13 for every ABI
./build-openssl.sh

# 4. Build tdlib-obf (Java/JNI by default) for arm64-v8a, armeabi-v7a, x86_64, x86
./build-tdlib.sh

# Artifacts: example/android/tdlib/libs/<abi>/  and  tdlib/tdlib.zip
```

`build-tdlib.sh` defaults already target NDK `r27b` → `27.1.12297006` and
`ANDROID_PLATFORM=android-16`; the per-ABI cross-compile resolves zlib/OpenSSL
from the NDK sysroot / built OpenSSL.

## Cleaning stale builds

Stale per-ABI build directories can hold an old CMake cache that still remembers
the rejected zlib gate. Remove them before re-running:

```bash
cd example/android
rm -rf build-native-* build-*-Java build-*-JSON*  tdlib
# single ABI only:
rm -rf build-arm64-v8a-Java
```

## TDLib baseline

The fork is on TDLib **1.8.63** and is actively kept current via per-commit
upstream backports (it is not stuck on an old baseline). Bumping the baseline to
`1.8.65` is **orthogonal** to the NDK r27 build fix — the zlib gate is version
independent — and is a large, separate effort that must reapply the fork's
obfuscation/custom-networking patches commit by commit. It is intentionally
**not** bundled into this build-fix branch, to keep this change reviewable and to
avoid destabilising the custom patches. Track the baseline bump separately.

## Known failure → resolution

| Symptom                                                      | Cause                                              | Fix on this branch                                              |
|-------------------------------------------------------------|----------------------------------------------------|----------------------------------------------------------------|
| `zlib 1.3.0.1 is too old. Minimum supported version 1.3.2.` | Desktop zlib floor applied to NDK r27 sysroot zlib | Android-aware allowance in `CMakeLists.txt` (platform-guarded)  |
| OpenSSL `3.0.13` "recommended" noise                        | `3.5.6` recommendation                             | Already a `WARNING`; `3.0.13` passes the minimum gate           |
| Clang version warning                                       | Clang `22.1.3+` recommendation                     | `TD_STRICT_COMPILER_VERSIONS=OFF` by default → warning only     |
