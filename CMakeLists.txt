cmake_minimum_required(VERSION 3.20)

project(lightweight_his C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# POSIX functions (fileno, popen, fdopen, readlink, etc.) require feature test
# macros when C extensions are disabled. Define _POSIX_C_SOURCE on non-Windows.
if(NOT WIN32)
    add_compile_definitions(_POSIX_C_SOURCE=200809L)
endif()

include(CTest)

add_library(his_common
    src/common/IdGenerator.c
    src/common/InputHelper.c
    src/common/LinkedList.c
    src/common/Result.c
    src/common/StringUtils.c
    src/common/UpdateChecker.c
    third_party/sha256/sha256.c
    src/domain/Admission.c
    src/domain/Bed.c
    src/domain/DispenseRecord.c
    src/domain/ExaminationRecord.c
    src/domain/InpatientOrder.c
    src/domain/Registration.c
    src/domain/VisitRecord.c
    src/domain/Ward.c
    src/repository/AdmissionRepository.c
    src/repository/BedRepository.c
    src/repository/DepartmentRepository.c
    src/repository/DispenseRecordRepository.c
    src/repository/DoctorRepository.c
    src/repository/ExaminationRecordRepository.c
    src/repository/InpatientOrderRepository.c
    src/repository/NursingRecordRepository.c
    src/repository/RoundRecordRepository.c
    src/repository/MedicineRepository.c
    src/repository/RepositoryUtils.c
    src/repository/PatientRepository.c
    src/repository/PrescriptionRepository.c
    src/repository/RegistrationRepository.c
    src/repository/TextFileRepository.c
    src/repository/UserRepository.c
    src/repository/VisitRecordRepository.c
    src/repository/WardRepository.c
    src/service/AuditService.c
    src/service/AuthService.c
    src/service/DepartmentService.c
    src/service/BedService.c
    src/service/DoctorService.c
    src/service/InpatientOrderService.c
    src/service/InpatientService.c
    src/service/MedicalRecordService.c
    src/service/PatientService.c
    src/service/NursingRecordService.c
    src/service/RoundRecordService.c
    src/service/PharmacyService.c
    src/service/PrescriptionService.c
    src/service/RegistrationService.c
    src/ui/DemoData.c
    src/ui/MenuActionAdmin.c
    src/ui/MenuActionDoctor.c
    src/ui/MenuActionInpatient.c
    src/ui/MenuActionPatient.c
    src/ui/MenuActionPharmacy.c
    src/ui/MenuApplication.c
    src/ui/MenuController.c
    src/ui/TuiPanel.c
    src/ui/TuiStyle.c
)

target_include_directories(his_common PUBLIC include)

# Link libm for sin(), cos(), sqrt() used in TUI animations (Linux requires explicit -lm)
if(UNIX AND NOT APPLE)
    target_link_libraries(his_common PUBLIC m)
endif()

add_executable(his
    src/main.c
)

target_link_libraries(his PRIVATE his_common)

add_executable(test_result
    tests/test_result.c
)

target_link_libraries(test_result PRIVATE his_common)

add_test(NAME test_result COMMAND test_result)

add_executable(test_common
    tests/test_common.c
)

target_link_libraries(test_common PRIVATE his_common)

add_test(NAME test_common COMMAND test_common)

add_executable(test_domain
    tests/test_domain.c
)

target_link_libraries(test_domain PRIVATE his_common)

add_test(NAME test_domain COMMAND test_domain)

add_executable(test_repository_utils
    tests/test_repository_utils.c
)

target_link_libraries(test_repository_utils PRIVATE his_common)

add_test(NAME test_repository_utils COMMAND test_repository_utils)

add_executable(test_patient_repository
    tests/test_patient_repository.c
)

target_link_libraries(test_patient_repository PRIVATE his_common)

add_test(NAME test_patient_repository COMMAND test_patient_repository)

add_executable(test_doctor_repository
    tests/test_doctor_repository.c
)

target_link_libraries(test_doctor_repository PRIVATE his_common)

add_test(NAME test_doctor_repository COMMAND test_doctor_repository)

add_executable(test_registration_repository
    tests/test_registration_repository.c
)

target_link_libraries(test_registration_repository PRIVATE his_common)

add_test(NAME test_registration_repository COMMAND test_registration_repository)

add_executable(test_doctor_service
    tests/test_department_service.c
    tests/test_doctor_service.c
)

target_link_libraries(test_doctor_service PRIVATE his_common)

add_test(NAME test_doctor_service COMMAND test_doctor_service)

add_executable(test_registration_service
    tests/test_registration_service.c
)

target_link_libraries(test_registration_service PRIVATE his_common)

add_test(NAME test_registration_service COMMAND test_registration_service)

add_executable(test_patient_service
    tests/test_patient_service.c
)

target_link_libraries(test_patient_service PRIVATE his_common)

add_test(NAME test_patient_service COMMAND test_patient_service)

add_executable(test_pharmacy_service
    tests/test_pharmacy_service.c
)

target_link_libraries(test_pharmacy_service PRIVATE his_common)

add_test(NAME test_pharmacy_service COMMAND test_pharmacy_service)

add_executable(test_inpatient_domain
    tests/test_inpatient_domain.c
)

target_link_libraries(test_inpatient_domain PRIVATE his_common)

add_test(NAME test_inpatient_domain COMMAND test_inpatient_domain)

add_executable(test_bed_service
    tests/test_bed_service.c
)

target_link_libraries(test_bed_service PRIVATE his_common)

add_test(NAME test_bed_service COMMAND test_bed_service)

add_executable(test_inpatient_service
    tests/test_inpatient_service.c
)

target_link_libraries(test_inpatient_service PRIVATE his_common)

add_test(NAME test_inpatient_service COMMAND test_inpatient_service)

add_executable(test_medical_record_service
    tests/test_medical_record_service.c
)

target_link_libraries(test_medical_record_service PRIVATE his_common)

add_test(NAME test_medical_record_service COMMAND test_medical_record_service)

add_executable(test_menu_controller
    tests/test_menu_controller.c
)

target_link_libraries(test_menu_controller PRIVATE his_common)

add_test(NAME test_menu_controller COMMAND test_menu_controller)

add_executable(test_menu_application
    tests/test_menu_application.c
)

target_link_libraries(test_menu_application PRIVATE his_common)

add_test(NAME test_menu_application COMMAND test_menu_application)

add_executable(test_demo_data_integrity
    tests/test_demo_data_integrity.c
)

target_link_libraries(test_demo_data_integrity PRIVATE his_common)

add_test(NAME test_demo_data_integrity COMMAND test_demo_data_integrity)

add_executable(test_auth_service
    tests/test_auth_service.c
)

target_link_libraries(test_auth_service PRIVATE his_common)

add_test(NAME test_auth_service COMMAND test_auth_service)

add_executable(test_tui_style
    tests/test_tui_style.c
)

target_link_libraries(test_tui_style PRIVATE his_common)

add_test(NAME test_tui_style COMMAND test_tui_style)

add_executable(test_text_file_repository
    tests/test_text_file_repository.c
)

target_link_libraries(test_text_file_repository PRIVATE his_common)

add_test(NAME test_text_file_repository COMMAND test_text_file_repository)

add_executable(test_ward
    tests/test_ward.c
)

target_link_libraries(test_ward PRIVATE his_common)

add_test(NAME test_ward COMMAND test_ward)

add_executable(test_admission
    tests/test_admission.c
)

target_link_libraries(test_admission PRIVATE his_common)

add_test(NAME test_admission COMMAND test_admission)

add_executable(test_audit_service
    tests/test_audit_service.c
)

target_link_libraries(test_audit_service PRIVATE his_common)

add_test(NAME test_audit_service COMMAND test_audit_service)

add_executable(test_prescription_service
    tests/test_prescription_service.c
)

target_link_libraries(test_prescription_service PRIVATE his_common)

add_test(NAME test_prescription_service COMMAND test_prescription_service)

add_executable(test_inpatient_order_service
    tests/test_inpatient_order_service.c
)

target_link_libraries(test_inpatient_order_service PRIVATE his_common)

add_test(NAME test_inpatient_order_service COMMAND test_inpatient_order_service)

add_executable(test_nursing_record_service
    tests/test_nursing_record_service.c
)

target_link_libraries(test_nursing_record_service PRIVATE his_common)

add_test(NAME test_nursing_record_service COMMAND test_nursing_record_service)

add_executable(test_round_record_service
    tests/test_round_record_service.c
)

target_link_libraries(test_round_record_service PRIVATE his_common)

add_test(NAME test_round_record_service COMMAND test_round_record_service)

set(HIS_TEST_TARGETS
    test_result
    test_common
    test_domain
    test_repository_utils
    test_patient_repository
    test_doctor_repository
    test_registration_repository
    test_doctor_service
    test_registration_service
    test_patient_service
    test_pharmacy_service
    test_inpatient_domain
    test_bed_service
    test_inpatient_service
    test_medical_record_service
    test_menu_controller
    test_menu_application
    test_demo_data_integrity
    test_auth_service
    test_tui_style
    test_text_file_repository
    test_ward
    test_admission
    test_audit_service
    test_prescription_service
    test_inpatient_order_service
    test_nursing_record_service
    test_round_record_service
)

foreach(test_target IN LISTS HIS_TEST_TARGETS)
    if(MSVC)
        target_compile_options(${test_target} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${test_target} PRIVATE -UNDEBUG)
    endif()
endforeach()
