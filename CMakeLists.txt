cmake_minimum_required(VERSION 4.1)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_HOME})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_HOME}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    else()
        set(VCPKG_LOCAL_DIR "${CMAKE_SOURCE_DIR}/vcpkg")
        if(NOT EXISTS "${VCPKG_LOCAL_DIR}/scripts/buildsystems/vcpkg.cmake")
            message(STATUS "vcpkg not found, bootstrapping into ${VCPKG_LOCAL_DIR}")
            find_package(Git REQUIRED)
            execute_process(
                COMMAND ${GIT_EXECUTABLE} clone https://github.com/microsoft/vcpkg.git "${VCPKG_LOCAL_DIR}"
                RESULT_VARIABLE GIT_RESULT
            )
            if(NOT GIT_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to clone vcpkg")
            endif()
            if(WIN32)
                execute_process(
                    COMMAND "${VCPKG_LOCAL_DIR}/bootstrap-vcpkg.bat" -disableMetrics
                    WORKING_DIRECTORY "${VCPKG_LOCAL_DIR}"
                    RESULT_VARIABLE BOOTSTRAP_RESULT
                )
            else()
                execute_process(
                    COMMAND "${VCPKG_LOCAL_DIR}/bootstrap-vcpkg.sh" -disableMetrics
                    WORKING_DIRECTORY "${VCPKG_LOCAL_DIR}"
                    RESULT_VARIABLE BOOTSTRAP_RESULT
                )
            endif()
            if(NOT BOOTSTRAP_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to bootstrap vcpkg")
            endif()
        endif()
        set(CMAKE_TOOLCHAIN_FILE "${VCPKG_LOCAL_DIR}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    endif()
endif()

project(StrokeCVLib LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# find_package(spdlog)

add_library(${PROJECT_NAME} SHARED src/lib.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC include)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# target_link_libraries(${PROJECT_NAME} PRIVATE
#     spdlog::spdlog
# )

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Syncing assets folder to build directory..."
)
