# Auth And Dispense Ownership Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the temporary role-access codes with real file-backed account authentication, and restore patient dispense-history access by adding patient ownership to dispense records.

**Architecture:** Add a small auth backend (`User` domain + `UserRepository` + `AuthService`) backed by `data/users.txt`, then wire login in `main`, `MenuApplication`, and `MenuController` with minimal surface changes. Extend `DispenseRecord` and pharmacy flows to persist `patient_id`, so patient dispense-history queries can filter by the bound patient session instead of failing closed. The implemented `users.txt` schema is `user_id|password_hash|role`, where role codes align with the seven console roles; inactive/disabled account state is deferred.

**Tech Stack:** C11, CMake, CTest, flat-file repositories, existing menu/application architecture

---

## Chunk 1: File-Backed Authentication

### Task 1: Add failing auth tests

**Files:**
- Create: `tests/test_auth_service.c`
- Inspect: `include/ui/MenuController.h`
- Inspect: `src/main.c`

- [ ] **Step 1: Write failing login tests**

Cover:
- valid user id/password/role succeeds
- wrong password fails
- wrong role fails
- inactive user fails

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build`
Expected: build fails because auth types/functions are not implemented yet

- [ ] **Step 3: Implement minimal auth backend**

Modify/Create:
- `include/domain/User.h`
- `src/domain/User.c` if needed
- `include/repository/UserRepository.h`
- `src/repository/UserRepository.c`
- `include/service/AuthService.h`
- `src/service/AuthService.c`

- [ ] **Step 4: Run auth-focused verification**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R test_auth_service`
Expected: PASS

## Chunk 2: Dispense Ownership

### Task 2: Add failing ownership tests for pharmacy records

**Files:**
- Modify: `tests/test_pharmacy_service.c`
- Inspect: `include/domain/DispenseRecord.h`
- Inspect: `src/repository/DispenseRecordRepository.c`
- Inspect: `src/service/PharmacyService.c`

- [ ] **Step 1: Write failing tests for patient-linked dispense records**

Cover:
- dispensing persists `patient_id`
- patient-linked query returns only matching patient records

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R test_pharmacy_service`
Expected: FAIL in the new ownership assertions

- [ ] **Step 3: Implement minimal dispense ownership**

Modify:
- `include/domain/DispenseRecord.h`
- `include/repository/DispenseRecordRepository.h`
- `src/repository/DispenseRecordRepository.c`
- `include/service/PharmacyService.h`
- `src/service/PharmacyService.c`

- [ ] **Step 4: Run pharmacy verification**

Run: `ctest --test-dir build --output-on-failure -R test_pharmacy_service`
Expected: PASS

## Chunk 3: Menu And Session Integration

### Task 3: Replace temporary login path and restore patient dispense history

**Files:**
- Modify: `include/ui/MenuApplication.h`
- Modify: `src/ui/MenuApplication.c`
- Modify: `src/ui/MenuController.c`
- Modify: `src/main.c`
- Modify: `tests/test_menu_application.c`
- Modify: `tests/test_menu_controller.c`
- Modify: `CMakeLists.txt`
- Create/Modify: `data/users.txt`

- [ ] **Step 1: Write failing integration tests**

Cover:
- real login flow for patient and non-patient roles
- patient dispense history succeeds for bound patient
- patient cannot see another patient's dispense history

- [ ] **Step 2: Run focused verification and confirm red**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R "test_menu_controller|test_menu_application"`
Expected: FAIL in the new auth / dispense-history assertions

- [ ] **Step 3: Implement minimal integration**

Target:
- `MenuApplication` owns `AuthService`
- `MenuApplication_init` accepts `users_path`
- `main` uses user id + password login instead of hardcoded role codes
- `MenuController` restores the patient dispense-history menu entry
- patient login binds the patient session via authenticated account id
- patient dispense history queries by bound `patient_id`

- [ ] **Step 4: Run focused integration verification**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R "test_menu_controller|test_menu_application|test_auth_service|test_pharmacy_service"`
Expected: PASS

## Chunk 4: Final Verification

### Task 4: Rebuild and run full regression suite

**Files:**
- Inspect only: all modified files above

- [ ] **Step 1: Run full build**

Run: `cmake --build build`
Expected: PASS

- [ ] **Step 2: Run full suite**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: PASS

- [ ] **Step 3: Record deferred follow-ups**

Defer explicitly:
- inactive/disabled account state in `users.txt`
- multi-user password management UX
- stronger password hashing than simple local deterministic hashing
- broader authorization model beyond role + patient self-service
