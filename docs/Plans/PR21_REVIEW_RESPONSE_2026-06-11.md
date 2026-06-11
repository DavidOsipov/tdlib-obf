# PR #21 Stealth Corpus Similarity Review — Response (2026-06-11)

Response to `PR21_STEALTH_CORPUS_SIMILARITY_REVIEW_2026-06-11.md`. Every finding is
addressed below with the concrete change, where it landed, and how it is verified.

Two branches:

- **`stealth-corpus-real-dump-similarity`** (PR #21) — the corpus-similarity test
  work, now hardened. Findings 2 and 3.
- **`stealth-runtime-hardening`** (new, off `master`) — the five assigned runtime
  stealth risks (Finding 1, F1–F5), kept separate from the test-only PR per the
  review's own "split" recommendation.

Build note: tdlib-obf does not build on macOS (zlib≥1.3.2 gate, missing
`htole*`, `std::atomic<std::shared_ptr>` unsupported by Apple libc++). The Python
generator + analysis suites are verified locally; **all C++ is verified on Linux
CI only** (see Finding 4 for commands). Findings flagged "CI-pending" below are
not unverified-by-omission — they are intentionally deferred to CI because the
toolchain cannot run on the author's machine.

## Finding 1 (High) — assigned runtime stealth weaknesses

Implemented on `stealth-runtime-hardening` (not folded into PR #21):

| Risk | Fix | Commit |
|------|-----|--------|
| F1 fail-open activation | `create_transport` returns a `FailClosedStealthTransport` instead of a plain `ObfuscatedTransport` when emulate_tls stealth activation fails: `write()` drops data, `can_write()` is false, `read_next()` errors — the unmasked legacy fingerprint is never put on the wire | `80048c84` |
| F2 mobile release lane | effective weights carve a 1/7 slice of the iOS share for the verified `Chrome147_IOSChromium` lane (was pinned to 0); reachable once `transport_confidence` permits its cross-layer claim | `7f5a093a` |
| F3 profile TOCTOU | `apply_profile_record_size_limit` also clamps to `platform_record_size_floor()`, so a config-time vs hello-time profile divergence cannot exceed the record_size_limit the wire declared | `65bb23e5` |
| F4 per-install entropy | `stable_selection_hash` mixes in an opt-in per-install salt (`set_per_install_selection_salt`); default 0 preserves the legacy deterministic vector | `d2062f83` |
| F5 firefox weight aliasing | `Firefox149_MacOS26_3` gets its own `firefox149_macos26_3` weight slot instead of aliasing `firefox148`; effective default weights unchanged | `d2062f83` |

Adversarial regression tests added: `test_stream_transport_activation_fail_closed`
(updated to assert fail-closed), `test_tls_mobile_release_grade_lane`,
`test_stealth_config_tls_init_profile_temporal_divergence` (floor-binding test +
rewritten firefox-slot tests), `test_tls_profile_selection_per_install_entropy`,
`test_tls_profile_firefox_weight_independence`.

Honest residuals (documented, not papered over): at the default Unknown
`transport_confidence` iOS still selects advisory IOS14 (a cross-layer-claim
profile must not be used without evidence); Android has no verified browser
capture, so its only lane is advisory. Closing these needs a real Android capture
and a `release_gating` curation decision — provenance work for the team, not
something to fix by mislabelling advisory evidence as release-grade.

## Finding 2 (High) — exact-field gate skipped catalog-backed critical fields

Branch `stealth-corpus-real-dump-similarity`, commit `300a3c5e`.

- `build_family_lane_baselines.py` emits per-field observed-value catalogs
  (`observed_cipher_suite_sequences`, `observed_extension_sets`,
  `observed_supported_versions_sequences`) into `SetMembershipCatalog`; header
  regenerated, byte-deterministic, matches the generator self-test.
- `FamilyLaneMatcher::matches_release_critical_field()` dispatches on
  `EvidenceFieldStatus`: Exact → non-empty exact equality; Catalog → membership in
  the observed catalog; Policy → fail closed (no named matcher yet);
  Unavailable/Mixed → fail closed.
- `test_tls_generator_fixture_exact_fields_gate` runs that dispatch for cipher
  suites, extension set, and supported versions, and adds mutant/negative tests
  proving a wrong value fails for both Exact and Catalog status.

## Finding 3 (Medium-high) — broad percent wire-length envelope

Branch `stealth-corpus-real-dump-similarity`, commit `300a3c5e`.

- `FamilyLaneMatcher::within_wire_length_byte_model()` bounds the generated length
  to within `max_byte_delta` of an observed sample, in bytes.
- `test_tls_generator_wire_length_fixture_gate` derives the budget from the
  generator mechanism: 255 B padding-target entropy (`rng.bounded(256u)`) + a
  fixture-derived 16 B SNI-length delta, replacing the arbitrary 15%.
  `within_wire_length_envelope` is retained only for the nightly self-calibrated
  Monte Carlo diagnostic.

## Finding 4 (Medium) — C++ gates unverified locally

Unchanged: tdlib-obf cannot build on macOS, so the C++ gates and runtime tests are
validated on Linux CI. After pushing both branches, the gate is:

```bash
cmake --build build --target run_all_tests --parallel 10
# Findings 2 & 3 (stealth-corpus-real-dump-similarity):
./build/test/run_all_tests --filter 'TlsGeneratorFixtureExactFieldsGate|TlsGeneratorWireLengthFixtureGate|TlsReleaseSimilarityUnavailableFailClosed|TlsGeneratorExtensionCountSimilarity|TlsGeneratorShuffleSimilarity'
# Finding 1 / F1–F5 (stealth-runtime-hardening):
./build/test/run_all_tests --filter 'StreamTransportActivationFailClosed|MobileReleaseGradeLane|StealthConfigTlsInitProfileTemporalDivergence|PerInstallSelectionEntropy|FirefoxWeightIndependence|StealthConfigProfileRecordLimitConsistency|StealthRuntimeDefaultsContract|TlsRuntimeProfilePolicyFailClosed'
```

Locally verified: the Python generator self-test (byte-deterministic, matches the
committed header) and the three analysis suites
(`test_family_lane_oracle_generation`, `test_corpus_iteration_tier_naming_contract`,
`test_similarity_release_gate_contract`).
