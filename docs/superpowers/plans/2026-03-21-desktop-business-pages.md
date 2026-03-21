# Desktop Business Pages Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the native desktop control panel with three new business pages: doctor workflow, inpatient/bed workflow, and pharmacy operations.

**Architecture:** Keep `his_desktop` as a single-window workbench. Add page state for the three new business areas and reuse existing `MenuApplication` public APIs, with optional desktop adapter wrappers where they simplify testing and output handling. Preserve the existing desktop shell, role-aware navigation, and CJK font/theme pipeline.

**Tech Stack:** C11, CMake, raylib, raygui, existing HIS service/application layer, CTest

---

## Chunk 1: Adapter And Test Support

### Task 1: Add desktop adapter helpers for new business workflows

**Files:**
- Modify: `include/ui/DesktopAdapters.h`
- Modify: `src/ui/DesktopAdapters.c`
- Modify: `tests/test_desktop_adapters.c`

- [ ] **Step 1: Write failing adapter tests**

Cover:
- doctor pending-list/history helper output
- inpatient list/admit/discharge/query helper output
- pharmacy add/restock/dispense/query helper output

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_adapters && ctest --test-dir build --output-on-failure -R test_desktop_adapters`
Expected: FAIL in the new business-helper assertions

- [ ] **Step 3: Implement minimal wrappers**

Implementation target:
- doctor helper wrappers around existing `MenuApplication_*`
- inpatient helper wrappers around existing `MenuApplication_*`
- pharmacy helper wrappers around existing `MenuApplication_*`
- keep result transport simple: formatted buffer + `Result`

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_desktop_adapters`
Expected: PASS

## Chunk 2: Desktop State And Navigation

### Task 2: Extend desktop page/state model

**Files:**
- Modify: `include/ui/DesktopApp.h`
- Modify: `src/ui/DesktopApp.c`
- Modify: `tests/test_desktop_state.c`

- [ ] **Step 1: Write failing state tests**

Cover:
- new pages exist in enum/label mapping
- role visibility assumptions for doctor/inpatient/pharmacy views
- logout/page-reset still stable after new page state is added

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build --target test_desktop_state && ctest --test-dir build --output-on-failure -R test_desktop_state`
Expected: FAIL in the new desktop-state assertions

- [ ] **Step 3: Implement new page state**

Implementation target:
- add `DESKTOP_PAGE_DOCTOR`
- add `DESKTOP_PAGE_INPATIENT`
- add `DESKTOP_PAGE_PHARMACY`
- add page state structs for forms/results

- [ ] **Step 4: Re-run focused verification**

Run: `ctest --test-dir build --output-on-failure -R test_desktop_state`
Expected: PASS

## Chunk 3: Doctor Workflow Page

### Task 3: Add doctor business page

**Files:**
- Modify: `src/ui/DesktopPages.c`
- Inspect: `include/ui/MenuApplication.h`

- [ ] **Step 1: Implement doctor page UI**

Include:
- doctor pending registrations query
- patient history query
- visit-record submit form
- exam request/result form
- result/output panel

- [ ] **Step 2: Keep role visibility correct**

Doctor page visible to:
- doctor
- admin

- [ ] **Step 3: Verify build and smoke**

Run: `cmake --build build --target his_desktop`
Expected: PASS

## Chunk 4: Inpatient And Bed Workflow Page

### Task 4: Add inpatient/bed page

**Files:**
- Modify: `src/ui/DesktopPages.c`

- [ ] **Step 1: Implement inpatient page UI**

Include:
- ward list
- beds by ward
- admit patient
- discharge patient
- active admission query by patient
- current patient query by bed

- [ ] **Step 2: Keep role visibility correct**

Inpatient page visible to:
- inpatient registrar
- ward manager
- admin

- [ ] **Step 3: Verify build and smoke**

Run: `cmake --build build --target his_desktop`
Expected: PASS

## Chunk 5: Pharmacy Operations Page

### Task 5: Add pharmacy operations page

**Files:**
- Modify: `src/ui/DesktopPages.c`

- [ ] **Step 1: Implement pharmacy page UI**

Include:
- add medicine
- restock
- dispense
- stock query
- low-stock query

- [ ] **Step 2: Keep role visibility correct**

Pharmacy page visible to:
- pharmacy
- admin

- [ ] **Step 3: Verify build and smoke**

Run: `cmake --build build --target his_desktop`
Expected: PASS

## Chunk 6: Final Verification

### Task 6: Verify integrated desktop expansion

**Files:**
- Inspect only: `include/ui/DesktopApp.h`, `src/ui/DesktopApp.c`, `src/ui/DesktopPages.c`, `src/ui/DesktopAdapters.c`, tests

- [ ] **Step 1: Build desktop targets**

Run: `cmake --build build --target his_desktop test_desktop_state test_desktop_adapters`
Expected: PASS

- [ ] **Step 2: Run focused desktop tests**

Run: `ctest --test-dir build --output-on-failure -R "test_desktop_state|test_desktop_adapters"`
Expected: PASS

- [ ] **Step 3: Run desktop smoke**

Run: `.\\build\\his_desktop.exe --smoke`
Expected: PASS

- [ ] **Step 4: Run full regression suite**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS
