# SPDX-FileCopyrightText: Copyright 2026 telemt community
# SPDX-License-Identifier: MIT
# telemt: https://github.com/telemt
# telemt: https://t.me/telemtrs

from __future__ import annotations

import pathlib
import re
import unittest


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]

# A release-similarity test must fail closed (assert) rather than silently
# `return;` when its real-corpus baseline is empty/unreviewed. This matches a
# bare `return;` whose trailing line-comment signals a pending/unpopulated
# baseline, robustly to rewording (the previous version only matched three
# exact comment strings, so a reworded skip-stub would have slipped through).
_PENDING_RETURN_RE = re.compile(
    r"return\s*;\s*//[^\n]*"
    r"(not yet reviewed|review still in progress|not yet populated"
    r"|baseline not (?:yet )?populated|corpus not yet|pending review)",
    re.IGNORECASE,
)


class SimilarityReleaseGateContract(unittest.TestCase):
    def test_release_similarity_tests_do_not_return_on_empty_baselines(self) -> None:
        checked_files = [
            REPO_ROOT / "test" / "stealth" / "test_tls_multi_dump_windows_chrome_stats.cpp",
            REPO_ROOT / "test" / "stealth" / "test_tls_multi_dump_ios_apple_tls_stats.cpp",
            REPO_ROOT / "test" / "stealth" / "test_tls_generator_fixture_exact_fields_gate.cpp",
            REPO_ROOT / "test" / "stealth" / "test_tls_generator_wire_length_fixture_gate.cpp",
        ]
        # Fail closed on a renamed/deleted gate target: a missing file must
        # not silently pass (the previous `if not path.exists(): continue`
        # tolerated exactly that and would mask a gate that lost its subject).
        missing = [
            str(path.relative_to(REPO_ROOT))
            for path in checked_files
            if not path.exists()
        ]
        self.assertEqual(
            [], missing,
            msg=f"release-similarity gate targets no longer exist: {missing}",
        )
        offenders: list[str] = []
        for path in checked_files:
            text = path.read_text(encoding="utf-8")
            if _PENDING_RETURN_RE.search(text):
                offenders.append(str(path.relative_to(REPO_ROOT)))
        self.assertEqual(
            [], offenders,
            msg=(
                "release-similarity tests must assert (fail closed) instead "
                f"of returning on an empty/unreviewed baseline: {offenders}"
            ),
        )

    def test_docs_separate_similarity_gates_from_seed_stress(self) -> None:
        pipeline = (REPO_ROOT / "docs" / "Documentation" / "FINGERPRINT_GENERATION_PIPELINE.md").read_text(
            encoding="utf-8"
        )
        lessons = (REPO_ROOT / "docs" / "Documentation" / "Lessons_Learnt.md").read_text(encoding="utf-8")
        combined = pipeline + "\n" + lessons

        self.assertIn("real-corpus similarity gate", combined)
        self.assertIn("seed-stress diagnostic", combined)
        self.assertIn("self-calibrated generator tests are not real-browser similarity evidence", combined)

    def test_multi_dump_baseline_sources_are_wired_into_run_all_tests(self) -> None:
        cmake_text = (REPO_ROOT / "test" / "CMakeLists.txt").read_text(encoding="utf-8")
        required_sources = [
            "test_tls_multi_dump_android_firefox_baseline.cpp",
            "test_tls_multi_dump_macos_chromium_baseline.cpp",
        ]

        missing = [source for source in required_sources if source not in cmake_text]
        self.assertEqual([], missing)
