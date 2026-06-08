// SPDX-FileCopyrightText: Copyright 2026 telemt community
// SPDX-License-Identifier: MIT
// telemt: https://github.com/telemt
// telemt: https://t.me/telemtrs
//

// Release-facing exact-field similarity gate. For each release-critical
// family/lane this suite first asserts that the reviewed evidence status for
// cipher suites, extension set, and supported versions is enforceable
// (Exact, Catalog, or Policy — never Unavailable or Mixed), then drives the
// generator over many seeds and requires every emitted ClientHello to match
// the reviewed exact invariants. Unlike the self-calibrated nightly Monte
// Carlo suites, the oracle here is fixture-derived, so a generator drift away
// from real browser dumps fails the gate instead of silently recalibrating.
//
// ECH mode per family is chosen to match the reviewed evidence: chromium and
// firefox Linux desktop dumps carry ECH (ech_presence_required=true), so they
// run with EchMode::Rfc9180Outer; apple_ios_tls dumps have no ECH, so it runs
// with EchMode::Disabled to keep the non-GREASE extension set equal to the
// reviewed 13-extension set.

#include "test/stealth/FamilyLaneMatchers.h"
#include "test/stealth/MockRng.h"
#include "test/stealth/ReviewedFamilyLaneBaselines.h"
#include "test/stealth/TlsHelloParsers.h"

#include "td/mtproto/stealth/TlsHelloBuilder.h"
#include "td/mtproto/stealth/TlsHelloProfileRegistry.h"

#include "td/utils/tests.h"

namespace {

using td::Slice;
using td::mtproto::stealth::BrowserProfile;
using td::mtproto::stealth::build_tls_client_hello_for_profile;
using td::mtproto::stealth::EchMode;
using td::mtproto::test::FamilyLaneMatcher;
using td::mtproto::test::MockRng;
using td::mtproto::test::baselines::EvidenceFieldStatus;
using td::mtproto::test::baselines::get_baseline;
using td::mtproto::test::parse_tls_client_hello;

constexpr td::int32 kUnixTime = 1712345678;
constexpr td::uint64 kSeeds = 64;

void assert_status_is_enforceable(EvidenceFieldStatus status) {
  ASSERT_TRUE(status == EvidenceFieldStatus::Exact || status == EvidenceFieldStatus::Catalog ||
              status == EvidenceFieldStatus::Policy);
}

void run_exact_gate(Slice family_id, BrowserProfile profile, EchMode ech_mode) {
  const auto *baseline = get_baseline(family_id, Slice("non_ru_egress"));
  ASSERT_TRUE(baseline != nullptr);
  assert_status_is_enforceable(baseline->non_grease_cipher_suites_status);
  assert_status_is_enforceable(baseline->non_grease_extension_set_status);
  assert_status_is_enforceable(baseline->non_grease_supported_versions_status);

  FamilyLaneMatcher matcher(*baseline);
  for (td::uint64 seed = 0; seed < kSeeds; seed++) {
    MockRng rng(seed);
    auto wire = build_tls_client_hello_for_profile("www.google.com", "0123456789secret", kUnixTime, profile, ech_mode,
                                                   rng);
    auto parsed = parse_tls_client_hello(wire);
    ASSERT_TRUE(parsed.is_ok());
    ASSERT_TRUE(matcher.matches_exact_invariants(parsed.ok_ref()));
  }
}

TEST(TlsGeneratorFixtureExactFieldsGate, Chrome133MatchesChromiumLinuxReviewedExactFields) {
  run_exact_gate(Slice("chromium_linux_desktop"), BrowserProfile::Chrome133, EchMode::Rfc9180Outer);
}

TEST(TlsGeneratorFixtureExactFieldsGate, Firefox148MatchesFirefoxLinuxReviewedExactFields) {
  run_exact_gate(Slice("firefox_linux_desktop"), BrowserProfile::Firefox148, EchMode::Rfc9180Outer);
}

TEST(TlsGeneratorFixtureExactFieldsGate, IOS14MatchesAppleIosReviewedExactFields) {
  run_exact_gate(Slice("apple_ios_tls"), BrowserProfile::IOS14, EchMode::Disabled);
}

}  // namespace
