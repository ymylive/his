# Desktop Frontend Completion Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the native desktop frontend so it exposes the current HIS business capabilities with stable interaction, role-correct navigation, and production-usable desktop workflows.

**Architecture:** Keep `his_desktop` as a single-window native workbench built on `raylib + raygui`. Centralize desktop interaction state in `DesktopApp`/`DesktopPages`, use `DesktopAdapters` as the only bridge to existing `MenuApplication`/service flows, and add focused helper/state modules only where they reduce page complexity or repeated parsing.

**Tech Stack:** C11, CMake, MinGW, raylib, raygui, existing MenuApplication/service layer, CTest

---

## File Structure

### Core desktop files to extend

- Modify: `include/ui/DesktopApp.h`
- Modify: `src/ui/DesktopApp.c`
- Modify: `include/ui/DesktopAdapters.h`
- Modify: `src/ui/DesktopAdapters.c`
- Modify: `include/ui/DesktopPages.h`
- Modify: `src/ui/DesktopPages.c`
- Modify: `include/ui/DesktopTheme.h`
- Modify: `src/ui/DesktopTheme.c`
- Modify: `src/desktop/main.c`

### Supporting business entrypoints already available

- Inspect: `include/ui/MenuApplication.h`
- Inspect: `src/ui/MenuApplication.c`
- Inspect: `include/service/AuthService.h`
- Inspect: `include/service/MedicalRecordService.h`
- Inspect: `include/service/InpatientService.h`
- Inspect: `include/service/PharmacyService.h`

### Test files to expand

- Modify: `tests/test_desktop_state.c`
- Modify: `tests/test_desktop_adapters.c`
- Create: `tests/test_desktop_workflows.c`

### Docs and delivery

- Modify: `README.md`
- Inspect: `docs/superpowers/specs/2026-03-21-desktop-control-panel-design.md`
- Inspect: `docs/superpowers/plans/2026-03-21-desktop-control-panel.md`
- Inspect: `docs/superpowers/plans/2026-03-21-desktop-business-pages.md`

## Chunk 1: Interaction Foundation

### Task 1: Stabilize desktop interaction primitives

**Files:**
- Modify: `src/ui/DesktopPages.c`
- Modify: `include/ui/DesktopApp.h`
- Modify: `tests/test_desktop_state.c`

- [ ] **Step 1: Write failing state tests for interaction model**

Cover:
- login fields can switch active input focus
- page state stores active input ids per page
- role selector no longer depends on fragile dropdown focus state

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_state && ctest --test-dir build --output-on-failure -R test_desktop_state`
Expected: FAIL on the new focus/state assertions

- [ ] **Step 3: Implement single-focus input model and safer selectors**

Implementation target:
- one active editable field per page
- explicit focus release on outside click
- role switch uses deterministic controls instead of unstable transient combo behavior
- no page enters permanent gui exclusive mode

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_desktop_state`
Expected: PASS

## Chunk 2: Shared Desktop Components

### Task 2: Build reusable desktop UI helpers

**Files:**
- Modify: `src/ui/DesktopPages.c`
- Modify: `include/ui/DesktopPages.h`
- Modify: `include/ui/DesktopTheme.h`
- Modify: `src/ui/DesktopTheme.c`

- [ ] **Step 1: Add helpers for modern desktop composition**

Implement reusable helpers for:
- titled sections
- output/result panels
- empty states
- form grids
- compact action rows
- metric cards
- scroll-safe text/list regions

- [ ] **Step 2: Apply modern visual baseline**

Implementation target:
- consistent spacing rhythm
- sharper card hierarchy
- cleaner side navigation
- legible form density at `1600x960`
- Chinese font rendering with stable fallback

- [ ] **Step 3: Verify build and smoke**

Run: `cmake --build build --target his_desktop && .\\build\\his_desktop.exe --smoke`
Expected: PASS

## Chunk 3: Admin And Clerk Coverage

### Task 3: Complete admin/clerk desktop workflows

**Files:**
- Modify: `src/ui/DesktopPages.c`
- Modify: `src/ui/DesktopAdapters.c`
- Modify: `include/ui/DesktopAdapters.h`
- Modify: `tests/test_desktop_adapters.c`
- Test: `tests/test_desktop_workflows.c`

- [ ] **Step 1: Write failing workflow tests**

Cover:
- patient add/update/query/delete helper flows
- doctor/department add/query/list flows
- time-range records query flow
- clerk registration query/cancel flow

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_adapters test_desktop_workflows && ctest --test-dir build --output-on-failure -R "test_desktop_adapters|test_desktop_workflows"`
Expected: FAIL in the new admin/clerk workflow assertions

- [ ] **Step 3: Implement desktop surfaces**

Implementation target:
- Patient page becomes full CRUD + detail page for admin/clerk
- Registration page gains query/cancel section
- Admin tools exposed as role-aware subsections, not hidden backend capabilities
- time-range medical record query available from admin/system workflow

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R "test_desktop_adapters|test_desktop_workflows"`
Expected: PASS

## Chunk 4: Doctor Workflow Completion

### Task 4: Complete doctor page beyond raw output dump

**Files:**
- Modify: `src/ui/DesktopPages.c`
- Modify: `tests/test_desktop_workflows.c`

- [ ] **Step 1: Write failing workflow tests**

Cover:
- doctor queue refresh and result presentation
- patient history query from doctor page
- visit record submission
- exam create and exam result completion
- medicine stock query from doctor workflow

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: FAIL in the doctor workflow assertions

- [ ] **Step 3: Implement doctor UI sections**

Implementation target:
- distinct doctor panels for queue/history/visit/exam/stock
- session-derived doctor id by default
- no misleading “处方与发药” affordance unless actual dispensing is supported there
- output area replaced with clearer per-section feedback

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: PASS

## Chunk 5: Inpatient And Ward Completion

### Task 5: Complete inpatient/ward operations page

**Files:**
- Modify: `src/ui/DesktopPages.c`
- Modify: `tests/test_desktop_workflows.c`

- [ ] **Step 1: Write failing workflow tests**

Cover:
- ward list and beds-by-ward browsing
- admit/discharge flows
- active admission by patient
- current patient by bed
- transfer-bed and discharge-check desktop actions

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: FAIL in the inpatient workflow assertions

- [ ] **Step 3: Implement inpatient page sections**

Implementation target:
- split ward browser and admission actions
- add transfer-bed and discharge-check subflows
- keep registrar/ward-manager role behavior distinct where necessary

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: PASS

## Chunk 6: Pharmacy Completion

### Task 6: Complete pharmacy page into full operator workbench

**Files:**
- Modify: `src/ui/DesktopPages.c`
- Modify: `tests/test_desktop_workflows.c`

- [ ] **Step 1: Write failing workflow tests**

Cover:
- add medicine
- restock
- dispense with patient ownership
- stock query
- low-stock query
- patient dispense-history interplay from desktop workflow

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: FAIL in the pharmacy workflow assertions

- [ ] **Step 3: Implement pharmacy UI sections**

Implementation target:
- split medicine master, inventory, and dispense operations
- session-derived pharmacist id by default
- clearer success/failure and resulting stock visibility

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: PASS

## Chunk 7: Patient Self-Service Completion

### Task 7: Complete patient desktop experience

**Files:**
- Modify: `src/ui/DesktopPages.c`
- Modify: `tests/test_desktop_workflows.c`

- [ ] **Step 1: Write failing workflow tests**

Cover:
- patient self info
- patient registration history
- patient visit/exam/admission history summaries
- patient dispense history
- patient medicine usage lookup

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_workflows && ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: FAIL in the patient self-service assertions

- [ ] **Step 3: Implement patient-only UX**

Implementation target:
- auto-bound patient fields
- remove operator-only inputs from patient view
- patient history tabs/sections readable without manual id re-entry

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_desktop_workflows`
Expected: PASS

## Chunk 8: Delivery Hardening

### Task 8: Package quality and docs

**Files:**
- Modify: `README.md`
- Inspect: `src/desktop/main.c`
- Inspect: desktop package/release instructions

- [ ] **Step 1: Update docs for actual desktop behavior**

Cover:
- role auto-detection on login
- portable package usage
- page coverage matrix

- [ ] **Step 2: Build and verify desktop targets**

Run: `cmake --build build --target his_desktop test_desktop_state test_desktop_adapters test_desktop_workflows`
Expected: PASS

- [ ] **Step 3: Run desktop smoke and full regression**

Run: `.\\build\\his_desktop.exe --smoke`
Expected: PASS

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS
