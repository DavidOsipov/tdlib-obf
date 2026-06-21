<!--
SPDX-FileCopyrightText: Copyright 2026 telemt community
SPDX-License-Identifier: MIT
telemt: https://github.com/telemt
telemt: https://t.me/telemtrs
-->

# PR23 Upstream Backport Audit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Audit the staged PR23 backport changes against the repo's architecture, security, C++23, and TDD principles, and only promote a fix if a real defect is proven with red-first tests.

**Architecture:** The audit runs with a preflight policy check followed by four batches of ten checks. Batch 1 establishes provenance and reviews the seam/build wiring. Batch 2 validates the ManagedBotCreated and call-notification paths. Batch 3 covers dialog repair, reply/username behavior, and the new runtime/fuzz tests. Batch 4 checks dialog-action equality, video repair semantics, and closes with a focused regression pass. If any batch exposes a real bug, the workflow pauses, writes a red test in a separate file, makes the smallest fix, and reruns the same targeted filter until green.

**Tech Stack:** `git`, `rg`, SocratiCode `codebase_search`/`codebase_impact`, context-mode `ctx_batch_execute`/`ctx_execute_file`, CMake, `ctest`, `./build/test/run_all_tests`, C++23.

**Status:** PLAN ONLY. No production code changes yet.

---

### Preflight: Policy and audit framing

**Files:**
- Inspect: `AGENTS.md`
- Inspect: `.github/instructions/architecture.instructions.md`
- Inspect: `.github/instructions/c++_rules.instructions.md`
- Inspect: `.github/instructions/Security_Requirements.instructions.md`
- Inspect: `.github/instructions/TDD_approach.instructions.md`

- [ ] **Step 1: Load repository policy**

Re-read `AGENTS.md` and confirm the backport audit must stay aligned with the repo's own workflow, test, and security rules.

- [ ] **Step 2: Load architecture rules**

Re-read `.github/instructions/architecture.instructions.md` and confirm the audit will judge the backport against layered design and minimal-surface rules.

- [ ] **Step 3: Load C++ rules**

Re-read `.github/instructions/c++_rules.instructions.md` and confirm the audit will check ownership, const-correctness, and repository-specific C++ conventions.

- [ ] **Step 4: Load security and TDD rules**

Re-read `.github/instructions/Security_Requirements.instructions.md` and `.github/instructions/TDD_approach.instructions.md` and confirm the audit will stay deny-by-default, test-first, and red-first if a defect is real.

---

### Task 1: Batch 1 - Baseline, seam, and build-wiring audit

**Files:**
- Inspect: `td/telegram/BackportTestSeams.h`
- Inspect: `td/telegram/Dependencies.h`
- Inspect: `td/telegram/CallActor.cpp`
- Inspect: `td/telegram/MessageContent.cpp`
- Inspect: `td/telegram/MessagesManager.cpp`
- Inspect: `td/telegram/VideosManager.cpp`
- Inspect: `test/CMakeLists.txt`

- [ ] **Step 1: Confirm the exact staged surface**

Run:
```bash
git status --short --branch
git diff --cached --stat
git diff --cached --name-only
```
Expected: the staged set matches the PR23 backport surface and contains no surprise files.

- [ ] **Step 2: Inspect the new seam file**

Read `td/telegram/BackportTestSeams.h` and confirm every helper is pure, minimal, and exists only to support tests.

- [ ] **Step 3: Inspect the new dependency accessor**

Read `td/telegram/Dependencies.h` and confirm `get_user_ids()` is read-only and does not weaken dependency resolution or allow mutation through a test seam.

- [ ] **Step 4: Inspect the call notification refactor**

Read `td/telegram/CallActor.cpp` and confirm the `PendingCallNotificationAction` refactor preserves the old outgoing/incoming and pending/ready branching exactly.

- [ ] **Step 5: Inspect the message-repair helpers**

Read `td/telegram/MessageContent.cpp`, `td/telegram/MessagesManager.cpp`, and `td/telegram/VideosManager.cpp` for unintended semantic drift introduced by the new helper indirection.

- [ ] **Step 6: Inspect test registration**

Read `test/CMakeLists.txt` and confirm each new test file is registered exactly once under the expected suite name.

- [ ] **Step 7: Check the diff boundary**

Confirm the plan stays scoped to the staged PR23 backport surface and does not drift into unrelated repository cleanup.

- [ ] **Step 8: Check helper exposure**

Confirm the new test seam does not expose any mutable state that production code can now reach directly.

- [ ] **Step 9: Check for unintended API surface**

Confirm `Dependencies::get_user_ids()` is only a read-only inspection hook and not a new general-purpose accessor.

- [ ] **Step 10: Stop on ambiguity**

If any item in this batch is not provably safe, freeze the batch and write a red contract test in a separate file before changing code.

---

### Task 2: Batch 2 - Managed bot dependency and call-notification audit

**Files:**
- Inspect: `td/telegram/MessageContent.cpp`
- Inspect: `td/telegram/Dependencies.h`
- Inspect: `td/telegram/BackportTestSeams.h`
- Inspect: `td/telegram/CallActor.cpp`
- Inspect: `test/managed_bot_created_dependency_contract.cpp`
- Inspect: `test/managed_bot_created_dependency_adversarial.cpp`
- Inspect: `test/managed_bot_created_dependency_integration.cpp`
- Inspect: `test/managed_bot_created_dependency_runtime.cpp`
- Inspect: `test/call_notification_send_closure_later_contract.cpp`
- Inspect: `test/call_notification_send_closure_later_adversarial.cpp`
- Inspect: `test/call_notification_send_closure_later_runtime.cpp`

- [ ] **Step 1: Verify ManagedBotCreated dependency semantics**

Read the `MessageContent.cpp` ManagedBotCreated branch and confirm the new helper still resolves `bot_user_id` as a required dependency, not as a best-effort hint.

- [ ] **Step 2: Verify the contract test is real**

Read `test/managed_bot_created_dependency_contract.cpp` and confirm it checks the source contract itself, not just the helper name or a brittle formatting artifact.

- [ ] **Step 3: Verify adversarial and runtime coverage**

Read `test/managed_bot_created_dependency_adversarial.cpp`, `test/managed_bot_created_dependency_integration.cpp`, and `test/managed_bot_created_dependency_runtime.cpp` and confirm they cover missing bot-user resolution, malformed service-message shapes, and end-to-end dependency repair.

- [ ] **Step 4: Verify the notification action helper**

Read `td/telegram/CallActor.cpp` and the seam helpers in `td/telegram/BackportTestSeams.h`; confirm the notification action table is a pure translation of the old branching logic.

- [ ] **Step 5: Verify deferred add vs immediate remove**

Read `test/call_notification_send_closure_later_contract.cpp`, `test/call_notification_send_closure_later_adversarial.cpp`, and `test/call_notification_send_closure_later_runtime.cpp` and confirm they exercise the deferred add, immediate remove, and no-op cases separately.

- [ ] **Step 6: Check ordering and race windows**

Confirm the `send_closure_later` path does not introduce a new ordering regression or race window relative to the previous direct branch.

- [ ] **Step 7: Cross-check the blast radius before edits**

If a divergence appears, use `codebase_search` and `codebase_impact` on the affected symbol before touching the implementation.

- [ ] **Step 8: Prove the bug before fixing it**

If the change is wrong, write the smallest failing test first in a separate file instead of weakening the existing contracts.

- [ ] **Step 9: Run the targeted slice**

Run:
```bash
./build/test/run_all_tests --filter ManagedBotCreatedDependency
./build/test/run_all_tests --filter CallNotificationSendClosureLater
```
Expected: either green, or a single reproducible failure that maps to one concrete contract gap.

- [ ] **Step 10: Record the result**

Capture any confirmed issue with exact file/line references and a short severity note before moving to the next batch.

---

### Task 3: Batch 3 - Dialog repair, reply, and username audit

**Files:**
- Inspect: `td/telegram/MessagesManager.cpp`
- Inspect: `test/parse_dialog_repair_refetch_contract.cpp`
- Inspect: `test/parse_dialog_repair_refetch_adversarial.cpp`
- Inspect: `test/parse_dialog_repair_refetch_integration.cpp`
- Inspect: `test/parse_dialog_repair_refetch_runtime.cpp`
- Inspect: `test/parse_dialog_repair_refetch_stress.cpp`
- Inspect: `test/reply_and_username_contract.cpp`
- Inspect: `test/reply_and_username_adversarial.cpp`
- Inspect: `test/reply_and_username_integration.cpp`
- Inspect: `test/reply_and_username_light_fuzz.cpp`
- Inspect: `test/reply_and_username_runtime_contract.cpp`
- Inspect: `test/reply_and_username_stress.cpp`

- [ ] **Step 1: Audit the dialog repair path**

Read the `MessagesManager.cpp` parse-dialog repair branch and confirm dependency repairs are scheduled in a fail-closed order.

- [ ] **Step 2: Check the repair operation decomposition**

Confirm `make_dialog_dependency_repair_operations()` preserves every previous fetch/reload action and does not silently drop any unresolved message or dialog refresh.

- [ ] **Step 3: Verify the parse-dialog contract test**

Read `test/parse_dialog_repair_refetch_contract.cpp` and confirm it pins the real repair contract rather than only the helper names.

- [ ] **Step 4: Verify adversarial coverage**

Read `test/parse_dialog_repair_refetch_adversarial.cpp` and confirm it attacks missing dependencies, malformed database state, and retry behavior.

- [ ] **Step 5: Verify integration and stress coverage**

Read `test/parse_dialog_repair_refetch_integration.cpp`, `test/parse_dialog_repair_refetch_runtime.cpp`, and `test/parse_dialog_repair_refetch_stress.cpp` and confirm they cover the full repair loop under realistic load.

- [ ] **Step 6: Audit reply and username semantics**

Read `test/reply_and_username_contract.cpp` and `test/reply_and_username_runtime_contract.cpp` and confirm same-chat unsent replies, local-message handling, and username checks still follow the current repo policy.

- [ ] **Step 7: Verify fuzz and stress guardrails**

Read `test/reply_and_username_light_fuzz.cpp`, `test/reply_and_username_adversarial.cpp`, `test/reply_and_username_integration.cpp`, and `test/reply_and_username_stress.cpp` and confirm they pin the required guard patterns instead of overfitting to current formatting.

- [ ] **Step 8: Check for duplicate retries or loops**

Confirm the dialog repair flow does not accidentally duplicate network requests or introduce a retry loop when multiple dependencies fail at once.

- [ ] **Step 9: Run the targeted slice**

Run:
```bash
./build/test/run_all_tests --filter 'ParseDialogRepairRefetch|ReplyAndUsername'
```
Expected: either green, or one reproducible failure that can be isolated to a single dialog-repair or reply rule.

- [ ] **Step 10: Capture the outcome**

If anything is flaky, reproduce it with a minimal seed or fixture, keep the failing test separate, and record whether the issue is a true regression or a stale test assumption.

---

### Task 4: Batch 4 - Dialog action equality, video repair, and final regression sweep

**Files:**
- Inspect: `td/telegram/DialogAction.h`
- Inspect: `td/telegram/VideosManager.h`
- Inspect: `td/telegram/VideosManager.cpp`
- Inspect: `td/telegram/MessageContent.cpp`
- Inspect: `test/dialog_action_equality_fields_contract.cpp`
- Inspect: `test/dialog_action_equality_fields_runtime.cpp`
- Inspect: `test/dialog_action_equality_fields_light_fuzz.cpp`
- Inspect: `test/video_alternative_properties_repair_contract.cpp`
- Inspect: `test/video_alternative_properties_repair_adversarial.cpp`
- Inspect: `test/video_alternative_properties_repair_integration.cpp`
- Inspect: `test/video_alternative_properties_repair_light_fuzz.cpp`
- Inspect: `test/video_alternative_properties_repair_runtime.cpp`

- [ ] **Step 1: Audit the dialog-action equality semantics**

Read `td/telegram/DialogAction.h` and the three `dialog_action_equality_fields_*` tests to verify `random_id_` and `text_` are compared wherever equality drives deduplication or update suppression.

- [ ] **Step 2: Check the fuzz target really hits the contract**

Read `test/dialog_action_equality_fields_light_fuzz.cpp` and confirm it mutates the fields that matter and fails when either field is ignored.

- [ ] **Step 3: Audit the video repair implementation**

Read `td/telegram/VideosManager.cpp`, `td/telegram/VideosManager.h`, and `td/telegram/MessageContent.cpp` and confirm the alternative-video repair plan only fills missing duration or thumbnail data.

- [ ] **Step 4: Verify the video repair contract**

Read `test/video_alternative_properties_repair_contract.cpp` and confirm it pins the expected repair behavior for missing duration and missing thumbnail cases.

- [ ] **Step 5: Verify adversarial coverage for conflicting alternatives**

Read `test/video_alternative_properties_repair_adversarial.cpp` and confirm it rejects conflicting alternative durations, bogus thumbnail promotion, and primary-data overwrite.

- [ ] **Step 6: Verify integration, fuzz, and runtime coverage**

Read `test/video_alternative_properties_repair_integration.cpp`, `test/video_alternative_properties_repair_light_fuzz.cpp`, and `test/video_alternative_properties_repair_runtime.cpp` and confirm they cover the full repair path without relying on a single happy-path fixture.

- [ ] **Step 7: Check the helper invariants**

Confirm `get_alternative_video_repair_plan()` is deterministic, explicit about its invariants, and does not mutate valid primary values.

- [ ] **Step 8: Run the targeted slice**

Run:
```bash
./build/test/run_all_tests --filter 'DialogActionEqualityFields|VideoAlternativePropertiesRepair'
```
Expected: green, or a single reproducible failure that maps to one concrete semantics bug.

- [ ] **Step 9: Escalate real defects with red-first tests**

If the slice fails for a real reason, add a new red test in a separate file first, then make the minimal code fix, then rerun the same targeted slice until green.

- [ ] **Step 10: Close the audit**

If all four batches are green, run the broader `ctest --test-dir build --output-on-failure` slice that this branch expects, then write a short audit findings note that separates verified defects from accepted design choices.
