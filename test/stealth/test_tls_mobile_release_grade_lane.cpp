// SPDX-FileCopyrightText: Copyright 2026 telemt community
// SPDX-License-Identifier: MIT
// telemt: https://github.com/telemt
// telemt: https://t.me/telemtrs
//
// Regression for PR #21 review finding 2 (F2): the verified browser-capture iOS
// Chromium lane (Chrome147_IOSChromium) was pinned to weight 0 in the effective
// profile weights, so iOS had only the advisory utls IOS14 lane and Android only
// its advisory utls okhttp lane. The effective weights now carve a slice of the
// iOS share for the verified iOS Chromium lane, making it reachable once
// transport_confidence permits its cross-layer claim.
//
// Honest residual (documented, not a bug to "fix" by fabricating evidence): at
// the default Unknown transport_confidence iOS still selects the advisory IOS14
// lane — only a TlsOnly-claim profile may be used without confidence evidence, so
// the advisory default is the conservative correct choice. And Android still has
// no verified browser-capture profile, so its only lane is advisory; closing that
// requires a real Android capture (a corpus/provenance task), and curating any
// mobile profile as release_gating is a separate team decision.

#include "td/mtproto/stealth/StealthRuntimeParams.h"
#include "td/mtproto/stealth/TlsHelloProfileRegistry.h"

#include "td/utils/common.h"
#include "td/utils/tests.h"

namespace {

using td::mtproto::stealth::BrowserProfile;
using td::mtproto::stealth::default_runtime_stealth_params;
using td::mtproto::stealth::DesktopOs;
using td::mtproto::stealth::DeviceClass;
using td::mtproto::stealth::MobileOs;
using td::mtproto::stealth::pick_runtime_profile;
using td::mtproto::stealth::reset_runtime_stealth_params_for_tests;
using td::mtproto::stealth::RuntimePlatformHints;
using td::mtproto::stealth::set_runtime_stealth_params_for_tests;
using td::mtproto::stealth::TransportConfidence;

constexpr td::int32 kUnixTime = 1712345678;

class Guard final {
 public:
  Guard() {
    reset_runtime_stealth_params_for_tests();
  }
  ~Guard() {
    reset_runtime_stealth_params_for_tests();
  }
};

RuntimePlatformHints ios_platform() {
  return RuntimePlatformHints{DeviceClass::Mobile, MobileOs::IOS, DesktopOs::Unknown};
}

RuntimePlatformHints android_platform() {
  return RuntimePlatformHints{DeviceClass::Mobile, MobileOs::Android, DesktopOs::Unknown};
}

// The verified iOS Chromium lane now carries a non-zero effective weight (a slice
// of the iOS share), instead of the previous hardcoded 0.
TEST(MobileReleaseGradeLane, IosChromiumLaneHasNonZeroEffectiveWeight) {
  Guard guard;
  auto weights = default_runtime_stealth_params().profile_weights;
  ASSERT_TRUE(weights.chrome147_ios_chromium > 0);
  ASSERT_TRUE(weights.ios14 > 0);
  // The carve-out comes out of the iOS share: ios14 + chrome147_ios_chromium
  // equals the configured iOS weight (70 by default).
  ASSERT_EQ(70, weights.ios14 + weights.chrome147_ios_chromium);
}

// With transport_confidence established, iOS can reach the verified Chromium lane
// while the advisory IOS14 lane stays dominant.
TEST(MobileReleaseGradeLane, IosReachesVerifiedChromiumLaneAtEstablishedConfidence) {
  Guard guard;
  auto params = default_runtime_stealth_params();
  params.transport_confidence = TransportConfidence::Partial;
  params.platform_hints = ios_platform();
  ASSERT_TRUE(set_runtime_stealth_params_for_tests(params).is_ok());

  bool saw_chromium = false;
  bool saw_ios14 = false;
  for (int i = 0; i < 256 && !(saw_chromium && saw_ios14); i++) {
    auto profile = pick_runtime_profile("ios-rel-" + td::to_string(i) + ".example", kUnixTime + i, ios_platform());
    if (profile == BrowserProfile::Chrome147_IOSChromium) {
      saw_chromium = true;
    } else if (profile == BrowserProfile::IOS14) {
      saw_ios14 = true;
    }
  }
  ASSERT_TRUE(saw_chromium);
  ASSERT_TRUE(saw_ios14);
}

// At the default Unknown confidence iOS selects only the advisory IOS14 lane: a
// cross-layer-claim profile may not be used without confidence evidence.
TEST(MobileReleaseGradeLane, IosDefaultsToAdvisoryLaneAtUnknownConfidence) {
  Guard guard;
  auto params = default_runtime_stealth_params();
  params.transport_confidence = TransportConfidence::Unknown;
  params.platform_hints = ios_platform();
  ASSERT_TRUE(set_runtime_stealth_params_for_tests(params).is_ok());

  for (int i = 0; i < 128; i++) {
    auto profile = pick_runtime_profile("ios-unk-" + td::to_string(i) + ".example", kUnixTime + i, ios_platform());
    ASSERT_TRUE(profile == BrowserProfile::IOS14);
  }
}

// Documented limitation: Android has no verified browser-capture profile, so its
// only lane is the advisory okhttp profile at any confidence.
TEST(MobileReleaseGradeLane, AndroidHasOnlyAdvisoryLane) {
  Guard guard;
  auto params = default_runtime_stealth_params();
  params.transport_confidence = TransportConfidence::Partial;
  params.platform_hints = android_platform();
  ASSERT_TRUE(set_runtime_stealth_params_for_tests(params).is_ok());

  for (int i = 0; i < 128; i++) {
    auto profile = pick_runtime_profile("android-" + td::to_string(i) + ".example", kUnixTime + i, android_platform());
    ASSERT_TRUE(profile == BrowserProfile::Android11_OkHttp_Advisory);
  }
}

}  // namespace
