cmake_minimum_required(VERSION 3.20)

project(lightweight_his C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(CTest)

add_library(his_common
    src/common/IdGenerator.c
    src/common/InputHelper.c
    src/common/LinkedList.c
    src/common/Result.c
    src/domain/DispenseRecord.c
    src/domain/ExaminationRecord.c
    src/domain/Registration.c
    src/domain/VisitRecord.c
    src/repository/DepartmentRepository.c
    src/repository/DispenseRecordRepository.c
    src/repository/DoctorRepository.c
    src/repository/ExaminationRecordRepository.c
    src/repository/MedicineRepository.c
    src/repository/RepositoryUtils.c
    src/repository/PatientRepository.c
    src/repository/RegistrationRepository.c
    src/repository/TextFileRepository.c
    src/repository/VisitRecordRepository.c
)

target_include_directories(his_common PUBLIC include)

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
