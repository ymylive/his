cmake_minimum_required(VERSION 3.20)

project(lightweight_his C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(CTest)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/raylib_vendor EXCLUDE_FROM_ALL)

add_library(his_common
    src/common/IdGenerator.c
    src/common/InputHelper.c
    src/common/LinkedList.c
    src/common/Result.c
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
    src/repository/MedicineRepository.c
    src/repository/RepositoryUtils.c
    src/repository/PatientRepository.c
    src/repository/RegistrationRepository.c
    src/repository/TextFileRepository.c
    src/repository/UserRepository.c
    src/repository/VisitRecordRepository.c
    src/repository/WardRepository.c
    src/service/AuthService.c
    src/service/DepartmentService.c
    src/service/BedService.c
    src/service/DoctorService.c
    src/service/InpatientService.c
    src/service/MedicalRecordService.c
    src/service/PatientService.c
    src/service/PharmacyService.c
    src/service/RegistrationService.c
    src/ui/MenuApplication.c
    src/ui/MenuController.c
)

target_include_directories(his_common PUBLIC include)

add_executable(his
    src/main.c
)

target_link_libraries(his PRIVATE his_common)

add_library(desktop_core
    src/ui/DesktopApp.c
    src/ui/DesktopTheme.c
    src/ui/DesktopPages.c
    src/ui/DesktopAdapters.c
)

target_include_directories(desktop_core
    PUBLIC
        include
        third_party
)

target_link_libraries(desktop_core
    PUBLIC
        his_common
        raylib
)

add_executable(his_desktop
    src/desktop/main.c
)

target_include_directories(his_desktop PRIVATE third_party)
target_link_libraries(his_desktop PRIVATE desktop_core)

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

add_executable(test_desktop_state
    tests/test_desktop_state.c
)

target_link_libraries(test_desktop_state PRIVATE desktop_core)

add_test(NAME test_desktop_state COMMAND test_desktop_state)

add_executable(test_desktop_adapters
    tests/test_desktop_adapters.c
)

target_link_libraries(test_desktop_adapters PRIVATE desktop_core)

add_test(NAME test_desktop_adapters COMMAND test_desktop_adapters)

add_executable(test_desktop_workflows
    tests/test_desktop_workflows.c
)

target_link_libraries(test_desktop_workflows PRIVATE desktop_core)

add_test(NAME test_desktop_workflows COMMAND test_desktop_workflows)

add_executable(test_auth_service
    tests/test_auth_service.c
)

target_link_libraries(test_auth_service PRIVATE his_common)

add_test(NAME test_auth_service COMMAND test_auth_service)

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
    test_desktop_state
    test_desktop_adapters
    test_desktop_workflows
    test_auth_service
)

foreach(test_target IN LISTS HIS_TEST_TARGETS)
    if(MSVC)
        target_compile_options(${test_target} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${test_target} PRIVATE -UNDEBUG)
    endif()
endforeach()
