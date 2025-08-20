#[[
Project Name: MoonMesh
Description: MoonMesh is an intriguing experiment exploring the potential future value of MoonMesh
Author: mm  
Version: 0.0.0
Created: 2025-07-01
Copyright: Copyright (c) 2025 MoonMesh Chain. All rights reserved.
CMake Minimum Version: 3.15
#]]

cmake_minimum_required(VERSION 3.15)

project(mm VERSION 0.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build.")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")

add_compile_options(-w)
add_definitions(-Wno-builtin-macro-redefined)
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(NDEBUG)
    message(STATUS "Build Release")
else()
    message(STATUS "Build Debug")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CMAKE_COMMAND} -E time")

set(CXX_FLAGS -Wall -g)
add_compile_options(${CXX_FLAGS})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--warn-unresolved-symbols")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(ROOT_DIR ${CMAKE_SOURCE_DIR})
set(DEPS_DIR ${ROOT_DIR}/deps_build)

if(EXISTS ${ROOT_DIR}/make_depend.sh)
    execute_process(COMMAND bash ${ROOT_DIR}/make_depend.sh ${CMAKE_CURRENT_BINARY_DIR})
else()
    message(WARNING "make_depend.sh not found, skipping dependency build")
endif()

# Import ca_core static library from root directory
add_library(ca_core STATIC IMPORTED)
set_property(TARGET ca_core PROPERTY IMPORTED_LOCATION ${ROOT_DIR}/libca_core.a)

# Import tx_core static library from root directory
add_library(tx_core STATIC IMPORTED)
set_property(TARGET tx_core PROPERTY IMPORTED_LOCATION ${ROOT_DIR}/libtx_core.a)

file(GLOB SOURCES 
    "*.cpp"
    "api/*.cpp"
    "api/interface/*.cpp"
    "include/*.cpp" 
    "utils/*.cpp"
    "utils/json/*.cpp"
    "utils/*.c"
    "common/*.cpp"
    "ca/*.cpp"
    "ca/evm/*.cpp"
    "db/*.cpp"
    "net/*.cpp"
    "main/*.cpp"
    "proto/*.cc"
    "mpt/*.cpp"
    "contract/*.cpp"
)

# Remove the files that are now in static libraries
list(REMOVE_ITEM SOURCES 
    "${CMAKE_SOURCE_DIR}/ca/global.cpp"
    "${CMAKE_SOURCE_DIR}/ca/transaction.cpp"
    "${CMAKE_SOURCE_DIR}/ca/algorithm.cpp"
    "${CMAKE_SOURCE_DIR}/ca/contract.cpp"
    "${CMAKE_SOURCE_DIR}/ca/ca.cpp"
    "${CMAKE_SOURCE_DIR}/ca/txhelper.cpp"
)

file(GLOB_RECURSE MAIN_FILE entry.cpp)
list(REMOVE_ITEM SOURCES ${MAIN_FILE})

set(ENV{BOOST_ROOT} ${DEPS_DIR}/boost/stage)
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS regex system thread REQUIRED)
if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost library not found. Please install boost development libraries.")
endif()

add_executable(${PROJECT_NAME} ${MAIN_FILE} ${SOURCES})

include(utils.cmake)
redefine_file_macro(${PROJECT_NAME})

set(EXECUTABLE_OUTPUT_PATH bin)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${ROOT_DIR}/
    ${ROOT_DIR}/ca
    ${ROOT_DIR}/ca/evm
    ${ROOT_DIR}/db
    ${ROOT_DIR}/include
    ${ROOT_DIR}/mpt
    ${DEPS_DIR}/rocksdb/include
    ${DEPS_DIR}/protobuf/src
    ${ROOT_DIR}/proto
    ${DEPS_DIR}/spdlog/include
    ${DEPS_DIR}/openssl/include
    ${DEPS_DIR}/evmone/evmc/include/
    ${DEPS_DIR}/evmone/include/
    #${ROOT_DIR}/wasmtime-cpp/include
    ${DEPS_DIR}/silkpre/lib/
    ${DEPS_DIR}/silkpre/
    ${DEPS_DIR}/evmone/lib/
    ${DEPS_DIR}/boost
    ${ROOT_DIR}/contract
    ${ROOT_DIR}/deps/threadpool
    ${ROOT_DIR}/deps/utils
    ${ROOT_DIR}/deps/json/include/
    ${ROOT_DIR}/deps/qrcode/cpp/
)

find_library(BZ2_LIBRARY bz2)
find_library(ZSTD_LIBRARY zstd)
find_library(Z_LIBRARY z)
find_library(DL_LIBRARY dl)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ca_core
    tx_core
    pthread
    ${Boost_LIBRARIES}
     rocksdb
    protobuf
    spdlog
    openssl
    #opensslcrypto
    evmone
    silkpre
)

if(BZ2_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${BZ2_LIBRARY})
    message(STATUS "Found bz2 library: ${BZ2_LIBRARY}")
endif()

if(ZSTD_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${ZSTD_LIBRARY})
    message(STATUS "Found zstd library: ${ZSTD_LIBRARY}")
endif()

if(Z_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${Z_LIBRARY})
    message(STATUS "Found z library: ${Z_LIBRARY}")
endif()

if(DL_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${DL_LIBRARY})
    message(STATUS "Found dl library: ${DL_LIBRARY}")
endif()

function(import_static_library_safe lib_name lib_paths)
    set(lib_found FALSE)
    foreach(lib_path ${lib_paths})
        if(EXISTS ${lib_path})
            add_library(${lib_name} STATIC IMPORTED)
            set_property(TARGET ${lib_name} PROPERTY IMPORTED_LOCATION ${lib_path})
            target_link_libraries(${PROJECT_NAME} PRIVATE ${lib_name})
            message(STATUS "Found ${lib_name} at: ${lib_path}")
            set(lib_found TRUE)
            break()
        endif()
    endforeach()
    
    if(NOT lib_found)
        message(FATAL_ERROR "Could not find ${lib_name} in any of the following paths: ${lib_paths}")
    endif()
endfunction()

if(EXISTS "/etc/centos-release")
    set(EVMONE_LIB_PATH "${DEPS_DIR}/evmone/build/lib/libevmone-standalone.a")
    message(STATUS "Detected CentOS, using lib path for evmone")
else()
    set(EVMONE_LIB_PATH "${DEPS_DIR}/evmone/build/lib64/libevmone-standalone.a")
    message(STATUS "Detected non-CentOS system, using lib64 path for evmone")
endif()

import_static_library_safe(rocksdb "${DEPS_DIR}/rocksdb/librocksdb.a")
import_static_library_safe(protobuf "${DEPS_DIR}/protobuf/build/libprotobuf.a")
import_static_library_safe(spdlog "${DEPS_DIR}/spdlog/libspdlog.a")
import_static_library_safe(openssl "${DEPS_DIR}/openssl/libssl.a")
import_static_library_safe(opensslcrypto "${DEPS_DIR}/openssl/libcrypto.a")
import_static_library_safe(silkpre "${DEPS_DIR}/silkpre/build/lib/libsilkpre-standalone.a")
import_static_library_safe(evmone "${EVMONE_LIB_PATH};${DEPS_DIR}/evmone/build/lib/libevmone-standalone.a;${DEPS_DIR}/evmone/build/lib64/libevmone-standalone.a")
import_static_library_safe(qrcode "${DEPS_DIR}/qrcode/cpp/libqrcodegencpp.a;")
#import_static_library_safe(wasmtime "${DEPS_DIR}/wasmtime-cpp/lib/libwasmtime.a")

if(EXISTS ${CMAKE_SOURCE_DIR}/gen_version_info.sh)
    add_custom_command(TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND bash ${CMAKE_SOURCE_DIR}/gen_version_info.sh 2 ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E echo "gen_version_info.sh executed successfully"
    )
else()
    message(WARNING "gen_version_info.sh not found, skipping version info generation")
endif()

find_package(GTest)
if(GTEST_FOUND)
    message(STATUS "GTest found, enabling test target")
    
    file(GLOB TEST_SOURCE "tests/*.cpp")
    if(TEST_SOURCE)
        add_executable(test EXCLUDE_FROM_ALL ${SOURCES} ${TEST_SOURCE})
        
        target_include_directories(test PRIVATE
            ${GTEST_INCLUDE_DIRS}
            ${ROOT_DIR}/
            ${ROOT_DIR}/ca
            ${ROOT_DIR}/ca/evm
            ${ROOT_DIR}/db
            ${ROOT_DIR}/include
            ${ROOT_DIR}/mpt
            ${DEPS_DIR}/rocksdb/include
            ${DEPS_DIR}/protobuf/src
            ${ROOT_DIR}/proto
            ${DEPS_DIR}/spdlog/include
            ${DEPS_DIR}/openssl/include
            ${DEPS_DIR}/evmone/evmc/include/
            ${DEPS_DIR}/evmone/include/
            #${ROOT_DIR}/wasmtime-cpp/include
            ${DEPS_DIR}/silkpre/lib/
            ${DEPS_DIR}/silkpre/
            ${DEPS_DIR}/evmone/lib/
            ${DEPS_DIR}/boost
            ${ROOT_DIR}/contract
        )
        
        message(STATUS "GTEST_BOTH_LIBRARIES: ${GTEST_BOTH_LIBRARIES}")
        message(STATUS "CMAKE_THREAD_LIBS_INIT: ${CMAKE_THREAD_LIBS_INIT}")

        target_link_libraries(test PRIVATE
            ca_core
            tx_core
            ${GTEST_BOTH_LIBRARIES}
            ${CMAKE_THREAD_LIBS_INIT}
            ${Boost_LIBRARIES}
            pthread
            rocksdb
            protobuf
            spdlog
            openssl
            opensslcrypto
            evmone
            silkpre
            #wasmtime
        )

        if(BZ2_LIBRARY)
            target_link_libraries(test PRIVATE ${BZ2_LIBRARY})
        endif()
        if(ZSTD_LIBRARY)
            target_link_libraries(test PRIVATE ${ZSTD_LIBRARY})
        endif()
        if(Z_LIBRARY)
            target_link_libraries(test PRIVATE ${Z_LIBRARY})
        endif()
        if(DL_LIBRARY)
            target_link_libraries(test PRIVATE ${DL_LIBRARY})
        endif()
    else()
        message(WARNING "No test source files found in tests/ directory")
    endif()
else()
    message(STATUS "GTest not found, test target disabled")
endif()

message(STATUS "=== Build Configuration ===")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Root Directory: ${ROOT_DIR}")
message(STATUS "Output Directory: ${EXECUTABLE_OUTPUT_PATH}")
message(STATUS "============================")
