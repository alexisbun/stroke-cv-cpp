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

project(HeadlessDataset LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(USER_HOME "$ENV{HOME}")

find_package(OpenGL REQUIRED)
find_package(spdlog REQUIRED)
find_package(Stb REQUIRED)

set(MEDIAPIPE_INCLUDE_DIR "/home/alexis/git/mediapipe")
set(MEDIAPIPE_LIB_DIR "/home/alexis/git/mediapipe/bazel-bin/mediapipe/tasks/c/vision/face_landmarker")
set(ONNXRUNTIME_INCLUDE_DIR "${USER_HOME}/.local/include")
set(ONNXRUNTIME_LIB_DIR "${USER_HOME}/.local/lib")

add_executable(${PROJECT_NAME} 
    src/dataset_generation.cpp
    src/face_mesh.cpp
    src/headless_gl_manager.cpp
    src/stroke_model_inference.cpp
)

# need to compile mp face landmarker in bazel, get open gl libraries for linux
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    OpenGL::EGL
    spdlog::spdlog
    ${MEDIAPIPE_LIB_DIR}/libface_landmarker.so
    ${ONNXRUNTIME_LIB_DIR}/libonnxruntime.so
)

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    ${Stb_INCLUDE_DIR}
    ${MEDIAPIPE_INCLUDE_DIR}
    ${MEDIAPIPE_INCLUDE_DIR}/mediapipe/tasks/c/vision/face_landmarker
    ${ONNXRUNTIME_INCLUDE_DIR}
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()