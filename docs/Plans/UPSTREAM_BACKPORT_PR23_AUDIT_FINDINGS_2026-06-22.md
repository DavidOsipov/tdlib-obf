<!--
SPDX-FileCopyrightText: Copyright 2026 telemt community
SPDX-License-Identifier: MIT
telemt: https://github.com/telemt
telemt: https://t.me/telemtrs
-->

# PR23 Upstream Backport Audit — Findings (Batches 8–44)

**Date:** 2026-06-22
**Scope:** Batches 8–44 of `docs/Plans/UPSTREAM_BACKPORT_PR23_AUDIT_PLAN_2026-06-21.md`, audited against the
repo principles, architectural correctness, C++23 rules, OWASP ASVS L2, and test quality
(integration / adversarial / black-hat / contract / fuzz / unit). Branch `feat/upstream-backport-bulk` vs `master`.
**Method:** per-batch read-only audit of the `master...HEAD` delta (one auditor per batch), cross-checked against
`upstream/master` (tdlib/td) for the security-sensitive regions; TDD red-first only for proven weaknesses.

---

## Headline result

**Zero production-code defects.** Every audited production change in batches 8–44 is a faithful `upstream/master`
backport (verified byte-for-byte for the security-sensitive regions) or a strictly fail-safe fork divergence
(`BotAccessSettings` validate-before-normalize is *stricter* than upstream; `WebDomainException.hpp` initialises a
flag upstream leaves uninitialised; `cli.cpp` de-dups an upstream triple-init copy-paste). No weakened access
control, no relaxed fail-closed behaviour, no use-after-move / dangling, no serialization (store/parse) asymmetry,
no silent error swallowing, no SSRF / scheme bypass, no weak-key acceptance, no injection in the Python tooling.

All actionable findings are **test-suite quality gaps** (and one cosmetic build-listing item), not behavioural bugs.

---

## Fixed this session (Python — red→green proven, runnable without the C++ build)

### PY-3 · `test/analysis/test_release_cohort_identity_contract.py` — vacuous ML-KEM test → 3 real tests
The original `test_modern_ios26_baselines_have_mlkem_in_supported_groups` inspected the **merged** corpus baseline,
whose `supported_groups` invariant always degrades to `[]` (the 3 legacy iOS samples lack ML-KEM), so its
`if sg:` body never executed and it asserted nothing — despite a docstring promising a post-quantum (ML-KEM,
`0x11EC` == `4588`) presence check. Replaced with three distinct tests, **all green, proven non-vacuous**
(stripping `0x11EC` from the modern samples drives the modern-only baseline to `[29,23,24,25]` → all three go RED):
- **positive contract** — a baseline built from *modern-only* iOS26 `apple_ios_tls` samples must carry `4588` in
  `supported_groups` and list it **first** (preferred), matching the real iOS 26 ClientHello;
- **adversarial / anti-downgrade** — every modern iOS26 sample must individually carry `0x11EC`, so a single
  downgraded fixture cannot hide behind the merged invariant degrading to `[]`;
- **differential** — `4588` ∈ modern-only baseline and `4588` ∉ legacy-only baseline (ML-KEM is the cohort
  discriminator the surrounding tests assume).

### B30-01 · `test/analysis/test_similarity_release_gate_contract.py` — missing-fixture tolerance + brittle match
`test_release_similarity_tests_do_not_return_on_empty_baselines` silently `continue`d on a missing gate target
(a renamed/deleted file would pass the gate) and matched skip-stubs by three exact comment strings (a reworded
stub evaded it). Hardened (green): now **asserts each gate target exists** (fail-closed on rename/delete) and
detects the fail-open `return; // …pending/unreviewed baseline…` anti-pattern via a keyword regex. Proven
non-vacuous: the regex flags the real anti-pattern in `test_tls_multi_dump_windows_firefox_stats.cpp` and does
**not** false-flag the clean `test_tls_multi_dump_windows_chrome_stats.cpp`.

---

## Filed for follow-up (not fixed — see rationale)

| bd id | Severity | Item | Why not fixed now |
|---|---|---|---|
| tdlib-obf-vvz | P2 (bug) | `test_tls_multi_dump_windows_firefox_stats.cpp:91,341` fail-open `return;` on empty `firefox_windows` baseline — the exact anti-pattern the similarity gate forbids, but not in the gate's checked list. | **Pre-existing in `master`, outside the PR23 backport delta.** Plausibly an intentional "pending real-capture review" skip. Needs a human decision (populate the baseline vs. convert to fail-closed) before changing master behaviour. |
| tdlib-obf-s7b | P2 (task) | `test_darwin_profile_hardcoding_bug.cpp:77-104` `ThreatsToProfileFixCorrectionness` has **zero assertions** (all results `(void)`-discarded); also `DarwinAlwaysSelectsChrome133` is name-inverted vs its body. | Fix is a C++ test edit; **cannot be verified red→green** — the C++ test binary does not configure in this environment (see blocker below). Did not commit an unverifiable C++ edit. |
| tdlib-obf-2ks | P3 (task) | B14-01: 5 new headers (`FormattedDate.h`, `JoinChatBotResult.h`, `WebBrowserManager.h`, `WebBrowserSettings.h`, `WebDomainException.h`) absent from the `CMakeLists.txt` header-listing block (IDE/`install(FILES)` only; build unaffected). | Cosmetic housekeeping; no test semantics. |

---

## Lower-severity test-gaps (documented, behaviour is sound)

These tests assert real behaviour but could be tightened; **none indicates a production defect**:
- **B38-001 (cross-cutting, all `*_1k` corpus tests):** the "1k"/"1024 runs" names run **64** iterations by default
  (`kSpotIterations`); the 1024 path (`kFullIterations`) runs only when `TD_NIGHTLY_CORPUS` is set. Confirm the
  nightly CI job exports that env var, otherwise the 1024-iteration path is never exercised.
- **B39** `test_tls_fingerprint_classifier_blackhat.cpp:442` — LOOCV indistinguishability gate silently `return`s if a
  baseline drops below 15 templates (add `ASSERT_TRUE(size >= 15)`). `BitFlipInWireChangesHmacVerification` asserts
  SHA-256 avalanche, not the builder's HMAC-embedding (reconstruct the builder's CR derivation to make it load-bearing).
- **B41-02** `test_tls_reader_byte_flow_adversarial.cpp` — only oversized + valid vectors; add truncated/incomplete-length
  and invalid-leading-byte cases. **B41-04** release-similarity fail-closed is asserted at the data layer, not the gate-decision layer.
- **B42-01** `test_tls_runtime_platform_weight_gate_adversarial.cpp` — asserts over-reject regression, not rejection of
  zeroed/invalid weights (that lives in `params_platform_fail_closed.cpp`). **B42-04** `serverhello_fixture_contract.cpp`
  pins path-routing + load, not generated-output field equivalence.
- **B43-01** `serverhello_pairing_adversarial.cpp` — positive pairing + ECH-off only; no mismatched-ServerHello rejection case.
- **B33-01** platform-isolation A/B tests check the static allow-list, not sampler-driven selection.
- **B34-01/02, B37-09, B42-06, B31-1/2** — several source-string/source-grep contracts (verify source text, not runtime
  behaviour) and whole-file (vs region-anchored) presence/absence checks; low false-pass risk, behaviour covered elsewhere.
- **B35-1** `session_event_bounds_fuzz.cpp` two cases assert only enum-membership (no-crash). **B37-03** profile-weight-bridge
  light_fuzz mutates one narrow axis.
- **Darwin-host note:** `real_fixture_alignment.cpp` / `case_alias_fixture_blackbox.cpp` / `transport_confidence_unknown_integration.cpp`
  are `#if !TD_DARWIN` — they compile to zero tests on macOS.

---

## Environment blocker (verification caveat)

The plan's `./build/test/run_all_tests --filter …` and `ctest --test-dir build` slices **cannot run in this
environment.** A CMake re-configure (auto-triggered) fails at `CMakeLists.txt:335`:
`Unable to determine zlib version from CMake discovery` — the fork-added zlib-version gate (a deliberate fail-closed
build guard) cannot parse a version from the local `/tmp/zlib-shim`. This is a pre-existing environment/shim
mismatch, independent of the audit and of the changes in this session (working tree touches only the two Python
test files). Consequently the C++ red→green cycle for tdlib-obf-s7b is deferred until the build configures.

---

## Coverage statement

Batches 8–44 of the plan were audited (the curated PR23 delta subset; ~440 files across business/auth, Telegram
runtime, MTProto/stealth, net, docs/TL schema, fixtures, tooling, and the regression/stealth test suites). The
5886-file raw `master...HEAD` diff is dominated by clienthello fixtures and generated artifacts outside the plan's
batch lists. Result: no proven production defect; two Python test-quality defects fixed (red→green); remaining
items are tracked in beads.
