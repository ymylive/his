# Demo Data Selector Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add resettable full demo seed data and replace manual ID entry with search-and-select interactions in both desktop and console flows.

**Architecture:** Keep core business services ID-based, add a demo-seed reset layer plus UI/menu-side selection helpers that search domain data and map selected rows back to IDs. Reuse shared helpers so desktop pages and console actions do not each reimplement filtering and selection logic.

**Tech Stack:** C11, raylib/raygui desktop UI, console menu UI, txt repositories, CTest

---

### Task 1: Add demo-seed storage and reset helpers

**Files:**
- Create: `data/demo_seed/*.txt`
- Create: `include/ui/DemoData.h`
- Create: `src/ui/DemoData.c`
- Modify: `CMakeLists.txt`
- Test: `tests/test_demo_data_integrity.c`

- [ ] Add a reusable helper API that copies all demo seed files onto runtime `data/*.txt`.
- [ ] Add a helper that validates all required demo seed files exist before reset.
- [ ] Extend integrity tests to validate demo seed completeness and reset-source presence.

### Task 2: Expand demo dataset to cover all role flows

**Files:**
- Modify: `data/demo_seed/*.txt`
- Optionally refresh: `data/*.txt`
- Test: `tests/test_demo_data_integrity.c`
- Test: `tests/test_desktop_workflows.c`

- [ ] Build a scenario-driven seed set with enough patients, doctors, departments, wards, beds, registrations, visits, exams, admissions, medicines, and dispenses to cover all demo pages and three full workflows.
- [ ] Ensure states include pending, completed, cancelled, active inpatient, discharged inpatient, low stock, and completed dispense.
- [ ] Keep all foreign keys aligned so search-and-select lists can show coherent related data.

### Task 3: Add desktop reset entry points

**Files:**
- Modify: `include/ui/DesktopApp.h`
- Modify: `src/ui/DesktopApp.c`
- Modify: `src/ui/DesktopPages.c`
- Modify: `src/ui/workbench/AdminWorkbench.c`
- Modify: `src/ui/DesktopAdapters.c`
- Modify: `include/ui/DesktopAdapters.h`
- Test: `tests/test_desktop_state.c`

- [ ] Add adapter methods for resetting demo data and clearing cached desktop page state.
- [ ] Add a reset button on the login page and on the system/admin page.
- [ ] Ensure reset refreshes dashboard/page freshness so new demo data is visible immediately.

### Task 4: Add shared desktop search-and-select components

**Files:**
- Modify: `include/ui/DesktopApp.h`
- Modify: `src/ui/WorkbenchCommon.c`
- Modify: `include/ui/Workbench.h`
- Test: `tests/test_desktop_layout_rules.c`

- [ ] Add reusable desktop picker state for search text, selected index, and rendered option labels.
- [ ] Add shared drawing helpers for searchable selection panels and compact selected-value badges.
- [ ] Keep layout responsive enough for desktop pages already in use.

### Task 5: Replace desktop ID entry in patient/clerk/doctor flows

**Files:**
- Modify: `src/ui/workbench/PatientWorkbench.c`
- Modify: `src/ui/workbench/ClerkWorkbench.c`
- Modify: `src/ui/workbench/DoctorWorkbench.c`
- Modify: `src/ui/DesktopAdapters.c`
- Test: `tests/test_patient_workbench_flow.c`
- Test: `tests/test_clerk_workbench_flow.c`
- Test: `tests/test_doctor_workbench_flow.c`

- [ ] Replace department/doctor/patient/registration/visit/examination ID text inputs with search-and-select controls.
- [ ] Preserve current downstream calls by writing selected IDs back into existing page state before submit.
- [ ] Update workflow tests to validate selection-based paths rather than raw ID entry.

### Task 6: Replace desktop ID entry in inpatient/pharmacy/admin flows

**Files:**
- Modify: `src/ui/workbench/InpatientWorkbench.c`
- Modify: `src/ui/workbench/WardWorkbench.c`
- Modify: `src/ui/workbench/PharmacyWorkbench.c`
- Modify: `src/ui/workbench/AdminWorkbench.c`
- Modify: `src/ui/DesktopAdapters.c`
- Test: `tests/test_inpatient_ward_workbench_flow.c`
- Test: `tests/test_pharmacy_admin_workbench_flow.c`

- [ ] Replace ward/bed/admission/medicine/patient/doctor/department inputs with search-and-select controls.
- [ ] Make low-stock, bed, and doctor lists selectable so follow-up actions can be triggered without typing IDs.
- [ ] Update integration tests to verify selection-driven execution.

### Task 7: Add console-side search-and-select helpers

**Files:**
- Modify: `include/ui/MenuApplication.h`
- Modify: `src/ui/MenuApplication.c`
- Modify: `src/ui/MenuController.c`
- Test: `tests/test_menu_application.c`
- Test: `tests/test_menu_controller.c`

- [ ] Add generic console helpers that list candidates, optionally filter by keyword, and accept a numeric selection.
- [ ] Use these helpers in all patient/doctor/department/ward/bed/admission/medicine/registration/visit related menu actions.
- [ ] Add a main-menu reset action for demo data.

### Task 8: Update docs and run end-to-end verification

**Files:**
- Modify: `README.md`
- Modify: `docs/BUILD_AND_RUN.md`
- Modify: `docs/AI_USAGE.md`
- Test: `tests/test_demo_data_integrity.c`
- Test: desktop and menu workflow suites

- [ ] Document reset-demo behavior and the fact that the app now supports search-and-select flows instead of manual ID entry.
- [ ] Run targeted and full verification commands across demo-data, desktop, and console workflows.
- [ ] Keep package/release expectations consistent with the new demo seed files.
