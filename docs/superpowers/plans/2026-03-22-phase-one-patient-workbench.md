# Phase 1 Patient Workbench Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver the shared desktop shell upgrade and a complete patient-facing workbench thin slice on the native `C + raylib + raygui` desktop app.

**Architecture:** Keep the single-window desktop app and reuse the existing `MenuApplication` business layer. Split the current desktop MVP into shared shell/theme/widgets, a DTO-based desktop adapter layer, and a dedicated patient workbench controller/renderer so patient flows can ship without leaking later-phase role work into Phase 1.

**Tech Stack:** C11, CMake, raylib, raygui, existing MenuApplication/service/repository layer, CTest

---

## File Structure

### Core files to modify

- Modify: `CMakeLists.txt`
- Modify: `include/ui/DesktopApp.h`
- Modify: `src/ui/DesktopApp.c`
- Modify: `include/ui/DesktopTheme.h`
- Modify: `src/ui/DesktopTheme.c`
- Modify: `include/ui/DesktopAdapters.h`
- Modify: `src/ui/DesktopAdapters.c`
- Modify: `include/ui/DesktopPages.h`
- Modify: `src/ui/DesktopPages.c`
- Modify: `include/ui/MenuApplication.h`
- Modify: `src/ui/MenuApplication.c`
- Modify: `tests/test_desktop_state.c`
- Modify: `tests/test_desktop_adapters.c`
- Modify: `tests/test_menu_application.c`

### New shared desktop files

- Create: `include/ui/DesktopWidgets.h`
- Create: `src/ui/DesktopWidgets.c`

### New patient workbench files

- Create: `include/ui/DesktopPatientWorkbench.h`
- Create: `src/ui/DesktopPatientWorkbench.c`
- Create: `tests/test_desktop_workflows.c`

### Spec references

- Inspect: `docs/superpowers/specs/2026-03-22-patient-first-medical-desktop-workbench-design.md`

## Chunk 1: Shared Shell, Build Wiring, And Theme Foundation

### Task 1: Create placeholder Phase 1 files and wire CMake

**Files:**
- Modify: `CMakeLists.txt`
- Create: `include/ui/DesktopWidgets.h`
- Create: `src/ui/DesktopWidgets.c`
- Create: `include/ui/DesktopPatientWorkbench.h`
- Create: `src/ui/DesktopPatientWorkbench.c`
- Create: `tests/test_desktop_workflows.c`

- [ ] **Step 1: Create minimal placeholder files**

Create stub-compilable files for:
- `DesktopWidgets`
- `DesktopPatientWorkbench`
- `test_desktop_workflows`

Each placeholder should compile but do no real work yet.

- [ ] **Step 2: Add the new desktop source files and workflow test target**

Wire:
- `src/ui/DesktopWidgets.c` into `desktop_core`
- `src/ui/DesktopPatientWorkbench.c` into `desktop_core`
- `tests/test_desktop_workflows.c` into a new `test_desktop_workflows` executable
- `add_test(NAME test_desktop_workflows COMMAND test_desktop_workflows)`

- [ ] **Step 3: Run build verification for target wiring**

Run: `cmake --build build --target his_desktop test_desktop_state test_desktop_adapters test_menu_application`
Expected: PASS

### Task 2: Add Phase 1 desktop state and navigation coverage

**Files:**
- Modify: `include/ui/DesktopApp.h`
- Modify: `src/ui/DesktopApp.c`
- Modify: `tests/test_desktop_state.c`

- [ ] **Step 1: Write failing desktop-state tests for patient Phase 1 pages**

Add assertions for:
- patient default home page changes from dashboard to patient home
- new patient pages exist in enum/label mapping
- `DesktopAppState` tracks `clean / dirty / stale` freshness for page reloads
- logout clears patient workbench page state and freshness state

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_state && ctest --test-dir build --output-on-failure -R test_desktop_state`
Expected: FAIL on missing page enum, labels, and freshness-state assertions

- [ ] **Step 3: Implement minimal desktop page/freshness state**

Implementation target:
- add patient Phase 1 page enums: home, registrations, history, admissions, medications, profile
- add page freshness enum or flags owned by `DesktopAppState`
- set patient login landing page to patient home
- reset freshness and patient page state on login/logout

- [ ] **Step 4: Re-run focused verification**

Run: `cmake --build build --target test_desktop_state && ctest --test-dir build --output-on-failure -R test_desktop_state`
Expected: PASS

### Task 3: Build shared widget and bright-medical theme baseline

**Files:**
- Modify: `include/ui/DesktopTheme.h`
- Modify: `src/ui/DesktopTheme.c`
- Modify: `tests/test_desktop_state.c`

- [ ] **Step 1: Write failing shell/theme assertions**

Extend `test_desktop_state.c` to check:
- new theme token defaults are non-zero and distinct for primary/success/warning/error
- widget helpers are linkable through `desktop_core`

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_state && ctest --test-dir build --output-on-failure -R test_desktop_state`
Expected: FAIL because widget symbols or new theme fields are missing

- [ ] **Step 3: Implement shared shell/theme primitives**

Implementation target:
- extend theme tokens to match the approved palette exactly:
  - background `#F4FAF9`
  - panel `#FFFFFF`
  - panel_alt `#EAF6F5`
  - nav/background accent family `#0F9FA8`, `#3BB8C4`
  - text primary `#16343A`
  - text secondary `#5F7D83`
  - border `#CFE5E4`
  - success `#22A06B`
  - warning `#D89B2B`
  - error `#D95C5C`
- add reusable helpers for section headers, metric cards, status chips, empty states, two-column list/detail shells, and message cards
- keep Chinese font fallback path intact

- [ ] **Step 4: Verify build and smoke**

Run: `cmake --build build --target his_desktop test_desktop_state && .\\build\\his_desktop.exe --smoke`
Expected: PASS

## Chunk 2: Patient Application-Layer And Desktop DTOs

### Task 4: Expose missing patient-facing application APIs

**Files:**
- Modify: `include/ui/MenuApplication.h`
- Modify: `src/ui/MenuApplication.c`
- Modify: `tests/test_menu_application.c`

- [ ] **Step 1: Write failing application tests for missing patient read/query APIs**

Add tests covering:
- public medicine detail / usage query for the patient medications page
- no duplicate work for already-public `MenuApplication_cancel_registration`

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_menu_application && ctest --test-dir build --output-on-failure -R test_menu_application`
Expected: FAIL on missing public API declarations or behavior

- [ ] **Step 3: Implement minimal public application surface**

Implementation target:
- expose only the missing public `MenuApplication_*` functions needed by patient Phase 1 medication/detail flows
- reuse existing internal logic where available
- do not leak later-phase admin capability lifting into Phase 1

- [ ] **Step 4: Re-run focused verification**

Run: `cmake --build build --target test_menu_application && ctest --test-dir build --output-on-failure -R test_menu_application`
Expected: PASS

### Task 5: Add patient DTO-based desktop adapter coverage

**Files:**
- Modify: `include/ui/DesktopAdapters.h`
- Modify: `src/ui/DesktopAdapters.c`
- Modify: `tests/test_desktop_adapters.c`

- [ ] **Step 1: Write failing adapter tests for patient DTOs and adapter result contract**

Cover:
- patient home summary DTO returns current primary state, reminders, and recent-flow rows deterministically
- patient registrations DTO sorts by latest first and prefers pending selection
- patient history DTO returns visit + exam sections with pending exam preference
- patient admissions DTO prefers active admission
- patient medications DTO returns dispense history plus medicine detail lookup
- patient profile DTO handles success and missing-profile failure path
- adapter result contract covers `error_kind` and `snapshot_policy`

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_adapters && ctest --test-dir build --output-on-failure -R test_desktop_adapters`
Expected: FAIL on missing DTO structs/functions and contract assertions

- [ ] **Step 3: Implement minimal DTO adapter layer**

Implementation target:
- define Phase 1 DTOs in `DesktopAdapters.h` for patient home summary, reminders, recent flows, registrations, history, admissions, medications, and profile
- keep `DesktopAdapters` stateless: input params + DTO output + adapter result contract
- encode `error_kind` with exact values `none`, `user_error`, `conflict`, `system_error`
- encode `snapshot_policy` with exact values `keep_last_success`, `clear_view`
- implement deterministic status precedence exactly as specified

- [ ] **Step 4: Re-run focused verification**

Run: `cmake --build build --target test_desktop_adapters && ctest --test-dir build --output-on-failure -R test_desktop_adapters`
Expected: PASS

## Chunk 3: Patient Workbench Rendering And Interaction

### Task 6: Add patient workbench controller skeleton and patient-only navigation

**Files:**
- Modify: `include/ui/DesktopPatientWorkbench.h`
- Modify: `src/ui/DesktopPatientWorkbench.c`
- Modify: `include/ui/DesktopPages.h`
- Modify: `src/ui/DesktopPages.c`
- Modify: `include/ui/DesktopApp.h`
- Modify: `src/ui/DesktopApp.c`
- Modify: `tests/test_desktop_workflows.c`

- [ ] **Step 1: Write failing workflow tests for controller skeleton**

Cover:
- patient-only navigation routes to the new patient pages
- patient controller keeps selected row ids and last DTO snapshot
- missing patient profile blocks all patient pages and shows severe error state
- shared shell still renders top bar, patient-only left nav, message bar, and work area

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: FAIL because patient workbench controller/pages do not exist yet

- [ ] **Step 3: Implement patient controller skeleton**

Implementation target:
- controller state owns selected row ids, inputs, last successful DTOs, and page-local view prefs
- patient-only navigation route is added to shared shell
- profile page and severe-error shell rendering exist behind the new controller
- `DesktopPages.c` only routes to patient controller; patient page-specific behavior lives in `DesktopPatientWorkbench.c`

- [ ] **Step 4: Re-run focused verification**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: PASS

### Task 7: Implement patient home and registration flow

**Files:**
- Modify: `src/ui/DesktopPatientWorkbench.c`
- Modify: `tests/test_desktop_workflows.c`

- [ ] **Step 1: Extend failing workflow tests for home and registrations**

Add coverage for:
- patient home computes current primary status and reminder ordering
- patient quick registration uses current time path and blocks duplicate pending registration
- patient registrations page supports cancel flow
- failed self-registration preserves department/doctor selection
- doctor/department load failure disables submit and shows retry path

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: FAIL on the newly added home/registration assertions

- [ ] **Step 3: Implement patient home and registrations rendering**

Implementation target:
- home quick-registration widget and registrations page reuse the same validation/submission path
- home renders status board, reminders, recent flows, and quick registration
- registrations page renders list/detail/cancel flow
- failed self-registration preserves department/doctor selection
- doctor/department load failure disables submit until retry succeeds

- [ ] **Step 4: Re-run focused verification**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: PASS

### Task 8: Implement patient history page

**Files:**
- Modify: `src/ui/DesktopPatientWorkbench.c`
- Modify: `tests/test_desktop_workflows.c`

- [ ] **Step 1: Extend failing workflow tests for the history page**

Add coverage for:
- patient history page prefers pending exam selection

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: FAIL on the newly added history assertions

- [ ] **Step 3: Implement the history page**

Implementation target:
- history page uses shared list/detail widgets
- pending exam selection preference is enforced
- empty and error states match the Phase 1 spec

- [ ] **Step 4: Re-run focused verification**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: PASS

### Task 9: Implement patient admissions page

**Files:**
- Modify: `src/ui/DesktopPatientWorkbench.c`
- Modify: `tests/test_desktop_workflows.c`

- [ ] **Step 1: Extend failing workflow tests for the admissions page**

Add coverage for:
- patient admissions page prefers active admission
- missing admission data falls back to empty-state copy rather than blank detail panel

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: FAIL on the newly added admissions assertions

- [ ] **Step 3: Implement the admissions page**

Implementation target:
- admissions page uses shared list/detail widgets
- active-admission preference is enforced
- empty and error states match the Phase 1 spec

- [ ] **Step 4: Re-run focused verification**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: PASS

### Task 10: Implement patient medications and profile pages

**Files:**
- Modify: `src/ui/DesktopPatientWorkbench.c`
- Modify: `tests/test_desktop_workflows.c`

- [ ] **Step 1: Extend failing workflow tests for medications and profile**

Add coverage for:
- patient medications page loads dispense history and medicine detail
- profile page renders grouped patient details and related summary links
- missing profile continues to block all patient pages until reload succeeds

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: FAIL on the newly added medications/profile assertions

- [ ] **Step 3: Implement medications and profile pages**

Implementation target:
- medications and profile pages use shared list/detail widgets
- medicine detail lookups are wired through the new public application API
- missing profile remains a controller-level blocking error, not just a page-local warning

- [ ] **Step 4: Re-run focused verification**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: PASS

### Task 11: Integrate patient refresh and stale-data behavior

**Files:**
- Modify: `src/ui/DesktopApp.c`
- Modify: `src/ui/DesktopPatientWorkbench.c`
- Modify: `tests/test_desktop_workflows.c`

- [ ] **Step 1: Add failing refresh/stale workflow assertions**

Cover:
- successful registration marks home and registrations pages dirty then reloads
- failed reload preserves previous DTO and exposes stale warning
- successful cancel clears duplicate-registration block and recomputes home status

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: FAIL on freshness and stale-state assertions

- [ ] **Step 3: Implement freshness invalidation flow**

Implementation target:
- `DesktopApp` owns page freshness map
- patient controller requests reload based on freshness state
- stale warning messages are user-visible without wiping page context

- [ ] **Step 4: Re-run focused verification**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: PASS

## Chunk 4: Phase 1 Verification And Delivery

### Task 12: Run integrated Phase 1 verification

**Files:**
- Inspect only: all files above

- [ ] **Step 1: Build the desktop and patient test targets**

Run: `cmake --build build --target his_desktop test_desktop_state test_desktop_adapters test_desktop_workflows test_menu_application`
Expected: PASS

- [ ] **Step 2: Run focused Phase 1 tests**

Run: `ctest --test-dir build --output-on-failure -R "test_desktop_state|test_desktop_adapters|test_desktop_workflows|test_menu_application"`
Expected: PASS

- [ ] **Step 3: Run desktop smoke**

Run: `.\\build\\his_desktop.exe --smoke`
Expected: PASS

- [ ] **Step 4: Run full regression suite**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS

- [ ] **Step 5: Verify Phase 1 DoD against spec**

Checklist:
- patient role lands on patient home
- patient home / registrations / history / admissions / medications / profile all render
- quick registration and cancellation both work
- current status board and recent-flow list are deterministic
- `dirty / stale` behavior is visible and test-covered

- [ ] **Step 6: Run manual visual verification for Phase 1**

Run:
- `.\\build\\his_desktop.exe`
- check patient login at `1600x960`
- resize to `1366x768`

Expected:
- patient home and all patient subpages remain operable
- no obvious horizontal clipping blocks use
- Chinese text renders legibly on shared shell and patient pages
