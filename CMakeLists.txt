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
