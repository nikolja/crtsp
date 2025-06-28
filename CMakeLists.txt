cmake_minimum_required(VERSION 3.16.3)
cmake_policy(SET CMP0091 NEW)

project(rtsp VERSION 0.0.1 LANGUAGES C CXX)
# cmake C++ standard
if (CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD ${CMAKE_CXX_STANDARD})
else()
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS ON)

message("")
message("========================================================================")
message(" Copyright (c) 2025-present, rtsp                          MIT licensed ")
message("========================================================================")
message(" 2025 preview 1                                                  v0.0.1 ")
message("========================================================================")
message("")
message(STATUS "CMAKE_ROOT:                     ${CMAKE_ROOT}")
message(STATUS "CMAKE_VERSION:                  ${CMAKE_VERSION}")
message(STATUS "CMAKE_GENERATOR:                ${CMAKE_GENERATOR}")
message(STATUS "CMAKE_SYSTEM:                   ${CMAKE_SYSTEM}")
message(STATUS "CMAKE_SYSTEM_NAME:              ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_INFO_FILE:         ${CMAKE_SYSTEM_INFO_FILE}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR:         ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_HOST_SYSTEM:              ${CMAKE_HOST_SYSTEM}")
message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR:    ${CMAKE_HOST_SYSTEM_PROCESSOR}")
if(CMAKE_HOST_WIN32)
    message(STATUS "CMAKE_HOST_WIN32:               ${CMAKE_HOST_WIN32}")
elseif(CMAKE_HOST_LINUX)
    message(STATUS "CMAKE_HOST_LINUX:               ${CMAKE_HOST_LINUX}")
elseif(CMAKE_HOST_APPLE)
    message(STATUS "CMAKE_HOST_APPLE:               ${CMAKE_HOST_APPLE}")
endif()
message(STATUS "CMAKE_BUILD_TYPE:               ${CMAKE_BUILD_TYPE}")
if(MSVC)
    message(STATUS "STATIC_WINDOWS_RUNTIME:         ${STATIC_WINDOWS_RUNTIME}")
    message(STATUS "CMAKE_MSVC_RUNTIME_LIBRARY:     ${CMAKE_MSVC_RUNTIME_LIBRARY}")
endif()
message(STATUS "CMAKE_CONFIGURATION_TYPES:      ${CMAKE_CONFIGURATION_TYPES}")
message(STATUS "CMAKE_COMPILER_IS_GNUCXX:       ${CMAKE_COMPILER_IS_GNUCXX}")
message(STATUS "CMAKE_FIND_LIBRARY_SUFFIXES:    ${CMAKE_FIND_LIBRARY_SUFFIXES}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS:         ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "CMAKE_MODULE_LINKER_FLAGS:      ${CMAKE_MODULE_LINKER_FLAGS}")
message(STATUS "CMAKE_SHARED_LINKER_FLAGS:      ${CMAKE_SHARED_LINKER_FLAGS}")
message(STATUS "CMAKE_STATIC_LINKER_FLAGS:      ${CMAKE_STATIC_LINKER_FLAGS}")
message(STATUS "CMAKE_C_COMPILER:               ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_C_COMPILER_ID:            ${CMAKE_C_COMPILER_ID}")
message(STATUS "CMAKE_C_COMPILER_VERSION:       ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "CMAKE_C_FLAGS:                  ${CMAKE_C_FLAGS}")
message(STATUS "CMAKE_C_FLAGS_DEBUG:            ${CMAKE_C_FLAGS_DEBUG}")
message(STATUS "CMAKE_C_FLAGS_RELEASE:          ${CMAKE_C_FLAGS_RELEASE}")
message(STATUS "CMAKE_C_FLAGS_MINSIZEREL:       ${CMAKE_C_FLAGS_MINSIZEREL}")
message(STATUS "CMAKE_C_FLAGS_RELWITHDEBINFO:   ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
message(STATUS "CMAKE_CXX_COMPILER:             ${CMAKE_CXX_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER_ID:          ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_CXX_COMPILER_VERSION:     ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CMAKE_CXX_FLAGS:                ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG:          ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE:        ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "CMAKE_CXX_FLAGS_MINSIZEREL:     ${CMAKE_CXX_FLAGS_MINSIZEREL}")
message(STATUS "CMAKE_CXX_FLAGS_RELWITHDEBINFO: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
if(CMAKE_TOOLCHAIN_FILE)
    message(STATUS "CMAKE_TOOLCHAIN_FILE:           ${CMAKE_TOOLCHAIN_FILE}")
endif()
message(STATUS "CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES: ${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES}")
if(CMAKE_ASM_NASM_COMPILER)
    message(STATUS "CMAKE_ASM_NASM_COMPILER:        ${CMAKE_ASM_NASM_COMPILER}")
endif()
message("")

# detect the platform RPi or Jetson
if(CMAKE_TOOLCHAIN_FILE)
    set(FILE_MODEL "${CMAKE_SYSROOT}/proc/device-tree/model")
else()
    set(FILE_MODEL "/proc/device-tree/model")
endif()
if(EXISTS "/usr/lib/aarch64-linux-gnu/tegra")
    set(IS_NANO ON)
    message(STATUS "Detected platform: jetson-nano")
elseif(EXISTS ${FILE_MODEL})
    execute_process(
        COMMAND cat ${FILE_MODEL}
        OUTPUT_VARIABLE RPI_MODEL
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "Detected platform: ${RPI_MODEL}")
    string(FIND "${RPI_MODEL}" "Raspberry Pi" RPI4_FOUND)
    string(FIND "${RPI_MODEL}" "Raspberry Pi 5" RPI5_FOUND)
    string(FIND "${RPI_MODEL}" "Raspberry Pi Zero" RPIZERO_FOUND)
    if (RPI5_FOUND GREATER -1)
        set(IS_RPI5 ON)
        message(STATUS "Raspberry Pi 5 detected")
    elseif(RPIZERO_FOUND GREATER -1)
        set(IS_RPIZERO ON)
        message(STATUS "Raspberry Pi Zero detected")
    elseif(RPI4_FOUND GREATER -1)
        set(IS_RPI4 ON)
        message(STATUS "Raspberry Pi 4 detected")
    else()
        message(STATUS "Unable to detect platform")
    endif()
else()
    message(WARNING "Unable to detect platform: ${FILE_MODEL} does not exist")
endif()

include(CMakeDependentOption)
# BUILD_SHARED_LIBS
option(BUILD_SHARED_LIBS "build shared lib" OFF)
# USING_FETCHCONT_DIR_AS_SRC
option(USING_FETCHCONT_DIR_AS_SRC "build using FETCHCONTENT_BASE_DIR as source of extern libs" ON)
# BUILD_HTTPLIB
option(BUILD_HTTPLIB "build with httplib" ON)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

#
# setup_version_variables
#

function (setup_version_variables lib_name lib_url lib_tag)
    if (EXISTS ${FETCHCONTENT_BASE_DIR})
        set(base_dir ${FETCHCONTENT_BASE_DIR})
    else()
        set(base_dir ${CMAKE_BINARY_DIR}/_deps)
    endif()
    set(${lib_name}_url ${lib_url} PARENT_SCOPE)
    set(${lib_name}_url ${lib_url} CACHE INTERNAL "${lib_name}_url")
    set(${lib_name}_tag ${lib_tag} PARENT_SCOPE)
    set(${lib_name}_tag ${lib_tag} CACHE INTERNAL "${lib_name}_tag")
    set(lib_dir ${base_dir}/${lib_name}-src)
    set(${lib_name}_dir ${lib_dir} PARENT_SCOPE)
    set(${lib_name}_dir ${lib_dir} CACHE INTERNAL "${lib_name}_dir")
endfunction()

#
# subdir_or_fetch_external_project
#

function (subdir_or_fetch_external_project target url branch)
    string(TOUPPER ${target} target_upper)

    message(STATUS ">> ")
    message(STATUS ">> ${target_upper} ...")
    message(STATUS ">> ")

    set(dir ${CMAKE_CURRENT_SOURCE_DIR}/extern/${target})

    if(EXISTS ${dir})
        message(STATUS "The source directory ${dir} exists")
        message(STATUS "Call add_subdirectory of an existing folder")
        if(EXISTS ${dir}/CMakeLists.txt)
            add_subdirectory(${dir})
        else()
            message(STATUS "The source directory ${dir} does not contain a CMakeLists.txt file")
        endif()
        set(${target}_SOURCE_DIR ${dir})
        set(${target}_BINARY_DIR ${dir})
    else()
        message(STATUS "The source directory ${dir} does not exist")
        if(USING_FETCHCONT_DIR_AS_SRC AND EXISTS ${${target}_dir})
            set(FETCHCONTENT_SOURCE_DIR_${target_upper} ${${target}_dir})
            message(STATUS "FETCHCONTENT_SOURCE_DIR_${target_upper}: ${FETCHCONTENT_SOURCE_DIR_${target_upper}}")
        else()
            unset(FETCHCONTENT_SOURCE_DIR_${target_upper} CACHE)
            message(STATUS "Fetching ${target} from git repository to ${FETCHCONTENT_BASE_DIR} ...")
        endif()
        FetchContent_Declare(
            ${target}
            GIT_REPOSITORY ${url}
            GIT_TAG ${branch}
            GIT_PROGRESS TRUE
            CMAKE_ARGS ${ARGN}
        )
        FetchContent_MakeAvailable(${target})
    endif()

    message(STATUS "${target}_POPULATED: ${${target}_POPULATED}")
    message(STATUS "${target}_SOURCE_DIR: ${${target}_SOURCE_DIR}")
    message(STATUS "${target}_BINARY_DIR: ${${target}_BINARY_DIR}")
    set(${target}_SOURCE_DIR ${${target}_SOURCE_DIR} CACHE INTERNAL "${target}_SOURCE_DIR")
    set(${target}_BINARY_DIR ${${target}_BINARY_DIR} CACHE INTERNAL "${target}_BINARY_DIR")
endfunction()

#
# cmake_gather_flags
# gathers all lists of flags for printing or manipulation
#

include(CMakeParseArguments)

macro(cmake_gather_flags with_linker result)
    set(${result} "")
    # add the main flags without a config
    list(APPEND ${result} CMAKE_C_FLAGS)
    list(APPEND ${result} CMAKE_CXX_FLAGS)
    if(${with_linker})
        list(APPEND ${result} CMAKE_EXE_LINKER_FLAGS)
        list(APPEND ${result} CMAKE_MODULE_LINKER_FLAGS)
        list(APPEND ${result} CMAKE_SHARED_LINKER_FLAGS)
        list(APPEND ${result} CMAKE_STATIC_LINKER_FLAGS)
    endif()
    
    if("${CMAKE_CONFIGURATION_TYPES}" STREQUAL "" AND NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
        # handle single config generators - like makefiles/ninja - when CMAKE_BUILD_TYPE is set
        string(TOUPPER ${CMAKE_BUILD_TYPE} config)
        list(APPEND ${result} CMAKE_C_FLAGS_${config})
        list(APPEND ${result} CMAKE_CXX_FLAGS_${config})
        if(${with_linker})
            list(APPEND ${result} CMAKE_EXE_LINKER_FLAGS_${config})
            list(APPEND ${result} CMAKE_MODULE_LINKER_FLAGS_${config})
            list(APPEND ${result} CMAKE_SHARED_LINKER_FLAGS_${config})
            list(APPEND ${result} CMAKE_STATIC_LINKER_FLAGS_${config})
        endif()
    else()
        # handle multi config generators (like msvc, xcode)
        foreach(config ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER ${config} config)
            list(APPEND ${result} CMAKE_C_FLAGS_${config})
            list(APPEND ${result} CMAKE_CXX_FLAGS_${config})
            if(${with_linker})
                list(APPEND ${result} CMAKE_EXE_LINKER_FLAGS_${config})
                list(APPEND ${result} CMAKE_MODULE_LINKER_FLAGS_${config})
                list(APPEND ${result} CMAKE_SHARED_LINKER_FLAGS_${config})
                list(APPEND ${result} CMAKE_STATIC_LINKER_FLAGS_${config})
            endif()
        endforeach()
    endif()
endmacro()

#
# cmake_set_runtime
# sets the runtime (static/dynamic) for msvc/gcc
#

macro(cmake_set_runtime)
    cmake_parse_arguments(ARG "STATIC;DYNAMIC" "" "" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "unrecognized arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" STREQUAL "")
        message(AUTHOR_WARNING "cmake_set_runtime() does not support clang yet!")
    endif()
    
    cmake_gather_flags(0 flags_configs)
    
    # add/replace the flags
    # note that if the user has messed with the flags directly this function might fail
    # for example if with MSVC and the user has removed the flags - here we just switch/replace them
    if("${ARG_STATIC}")
        foreach(flags ${flags_configs})
            if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
                if(NOT ${flags} MATCHES "-static-libstdc\\+\\+")
                    set(${flags} "${${flags}} -static-libstdc++")
                endif()
                if(NOT ${flags} MATCHES "-static-libgcc")
                    set(${flags} "${${flags}} -static-libgcc")
                endif()
            elseif(MSVC)
                if(${flags} MATCHES "/MD")
                    string(REGEX REPLACE "/MD" "/MT" ${flags} "${${flags}}")
                endif()
            endif()
        endforeach()
    elseif("${ARG_DYNAMIC}")
        foreach(flags ${flags_configs})
            if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
                if(${flags} MATCHES "-static-libstdc\\+\\+")
                    string(REGEX REPLACE "-static-libstdc\\+\\+" "" ${flags} "${${flags}}")
                endif()
                if(${flags} MATCHES "-static-libgcc")
                    string(REGEX REPLACE "-static-libgcc" "" ${flags} "${${flags}}")
                endif()
            elseif(MSVC)
                if(${flags} MATCHES "/MT")
                    string(REGEX REPLACE "/MT" "/MD" ${flags} "${${flags}}")
                endif()
            endif()
        endforeach()
    endif()
endmacro()

macro(cmake_copy_cxx_flags_to_other_conf)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}${CMAKE_C_FLAGS}")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}${CMAKE_C_FLAGS}")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}${CMAKE_CXX_FLAGS}")
    endif()
endmacro()

if(MSVC AND BUILD_RUNTIME_STATIC)
    set(CMAKE_LINK_SEARCH_START_STATIC ON)
    set(CMAKE_LINK_SEARCH_END_STATIC ON)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    STRING (REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
elseif(CMAKE_COMPILER_IS_GNUCXX AND BUILD_RUNTIME_STATIC)
    cmake_set_runtime(STATIC)
    cmake_copy_cxx_flags_to_other_conf()
endif()

# 
# threads
#

find_package(Threads REQUIRED GLOBAL)

#
# gst
#

find_package(PkgConfig QUIET)
# edit the system environment variables -> control panel -> system -> advanced -> environment variables
# user variables
# Path: 
# C:\gstreamer\1.0\msvc_x86_64\bin
# C:\gstreamer\1.0\msvc_x86_64\lib\gstreamer-1.0
# C:\gstreamer\1.0\msvc_x86_64\lib
# system variables
# GSTREAMER_1_0_ROOT_MSVC_X86_64: C:\gstreamer\1.0\msvc_x86_64
# GSTREAMER_1_0_ROOT_X86_64: C:\gstreamer\1.0\msvc_x86_64
# GSTREAMER_DIR: C:\gstreamer\1.0\msvc_x86_64
# Path:
# C:\gstreamer\1.0\msvc_x86_64\bin
if(PkgConfig_FOUND)
    pkg_check_modules(GSTREAMER QUIET gstreamer-1.0 gstreamer-app-1.0 gstreamer-plugins-base-1.0 gstreamer-pbutils-1.0 gstreamer-webrtc-1.0 gstreamer-sdp-1.0 gstreamer-rtsp-server-1.0 gstreamer-rtsp-1.0)
    if(CMAKE_COMPILER_IS_GNUCXX)
        set(GSTREAMER_LINK_LIBRARIES ${GSTREAMER_LINK_LIBRARIES} -lgstriff-1.0)
    else()
        set(GSTREAMER_LINK_LIBRARIES ${GSTREAMER_LINK_LIBRARIES} ${GSTREAMER_LIBRARY_DIRS}/gstriff-1.0.lib)
    endif()
    # gstapp-1.0
    # gstbase-1.0 gstpbutils-1.0 gstriff-1.0 gstvideo-1.0 gstaudio-1.0
    # glib-2.0 gobject-2.0
endif()
if(GSTREAMER_FOUND)
    if(NOT CMAKE_HOST_WIN32)
        string(REPLACE ".a" ".so" GSTREAMER_LINK_LIBRARIES "${GSTREAMER_LINK_LIBRARIES}")
    else()
        #string(REPLACE ".dll" ".lib" GSTREAMER_LINK_LIBRARIES "${GSTREAMER_LINK_LIBRARIES}")
    endif()
    message(STATUS ">> ")
    message(STATUS ">> gstreamer-1.0 with gstreamer-rtsp-server-1.0 ...")
    message(STATUS ">> ")
    message(STATUS "GSTREAMER_FOUND: ${GSTREAMER_FOUND}")
    message(STATUS "The requested package GSTREAMER exists")
    message(STATUS "GSTREAMER_VERSION: ${GSTREAMER_VERSION}")
    message(STATUS "GSTREAMER_LINK_LIBRARIES: ${GSTREAMER_LINK_LIBRARIES}")
    message(STATUS "GSTREAMER_LIBRARY_DIRS: ${GSTREAMER_LIBRARY_DIRS}")
    message(STATUS "GSTREAMER_INCLUDE_DIRS: ${GSTREAMER_INCLUDE_DIRS}")
    set(GSTREAMER_LINK_LIBRARIES ${GSTREAMER_LINK_LIBRARIES} CACHE INTERNAL "GSTREAMER_LINK_LIBRARIES")
    set(GSTREAMER_INCLUDE_DIRS ${GSTREAMER_INCLUDE_DIRS} CACHE INTERNAL "GSTREAMER_INCLUDE_DIRS")
else()
    unset(GSTREAMER_FOUND CACHE)
    unset(GSTREAMER_LINK_LIBRARIES CACHE)
    message(STATUS "The requested package GSTREAMER does not exists")
endif()

#
# spdlog
#

setup_version_variables(spdlog https://github.com/gabime/spdlog.git v1.x)
subdir_or_fetch_external_project(
    spdlog
    ${spdlog_url}
    ${spdlog_tag}
)
set_property(TARGET spdlog PROPERTY POSITION_INDEPENDENT_CODE ON)

#
# cxxopts
#

setup_version_variables(cxxopts https://github.com/jarro2783/cxxopts master)
subdir_or_fetch_external_project(
    cxxopts
    ${cxxopts_url}
    ${cxxopts_tag}
)

#
# nlohmann_json
#

setup_version_variables(json https://github.com/nlohmann/json master)
subdir_or_fetch_external_project(
    json
    ${json_url}
    ${json_tag}
)

#
# httplib
#

setup_version_variables(httplib https://github.com/yhirose/cpp-httplib master)
if(BUILD_HTTPLIB)
    set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF)
    set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF)
    set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF)
    set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF)
    subdir_or_fetch_external_project(
        httplib
        ${httplib_url}
        ${httplib_tag}
        "-DHTTPLIB_USE_OPENSSL_IF_AVAILABLE=OFF -DHTTPLIB_USE_ZLIB_IF_AVAILABLE=OFF -DHTTPLIB_USE_BROTLI_IF_AVAILABLE=OFF -DHTTPLIB_USE_ZSTD_IF_AVAILABLE=OFF"
    )
    set(HTTPLIB_USING ON)
    set(HTTPLIB_LIB httplib)
    set(HTTPLIB_USING ${HTTPLIB_USING} CACHE INTERNAL "HTTPLIB_USING")
    set(HTTPLIB_LIB ${HTTPLIB_LIB} CACHE INTERNAL "HTTPLIB_LIB")
    set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE ${HTTPLIB_USE_OPENSSL_IF_AVAILABLE} CACHE INTERNAL "HTTPLIB_USE_OPENSSL_IF_AVAILABLE")
    set(HTTPLIB_USE_ZLIB_IF_AVAILABLE ${HTTPLIB_USE_ZLIB_IF_AVAILABLE} CACHE INTERNAL "HTTPLIB_USE_ZLIB_IF_AVAILABLE")
    set(HTTPLIB_USE_BROTLI_IF_AVAILABLE ${HTTPLIB_USE_BROTLI_IF_AVAILABLE} CACHE INTERNAL "HTTPLIB_USE_BROTLI_IF_AVAILABLE")
    set(HTTPLIB_USE_ZSTD_IF_AVAILABLE ${HTTPLIB_USE_ZSTD_IF_AVAILABLE} CACHE INTERNAL "HTTPLIB_USE_ZSTD_IF_AVAILABLE")
    set_target_properties(httplib PROPERTIES FOLDER deps)
else()
    unset(HTTPLIB_USING CACHE)
    unset(HTTPLIB_LIB CACHE)
endif()


#
# rtsp
#

add_executable(rtsp app/rtsp.cpp)
target_include_directories(rtsp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${spdlog_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${cxxopts_SOURCE_DIR}/include>
    ${GSTREAMER_INCLUDE_DIRS}
)

if(IS_NANO)
    target_compile_definitions(rtsp PUBLIC IS_NANO)
elseif(IS_RPI5)
    target_compile_definitions(rtsp PUBLIC IS_RPI5)
elseif(IS_RPIZERO)
    target_compile_definitions(rtsp PUBLIC IS_RPIZERO)
elseif(IS_RPI4)
    target_compile_definitions(rtsp PUBLIC IS_RPI4)
endif()

if(HTTPLIB_USING)
    target_compile_definitions(rtsp PUBLIC WITH_HTTPLIB)
endif()

target_link_libraries(rtsp PRIVATE 
    Threads::Threads
    #spdlog::spdlog
    nlohmann_json
    ${GSTREAMER_LINK_LIBRARIES}
    ${HTTPLIB_LIB}
)

if(MSVC)
    target_compile_options(rtsp PRIVATE /bigobj /Zc:__cplusplus /Zi)
    #target_compile_options(rtsp PRIVATE /bigobj /Zc:__cplusplus /Zi /W4 /WX) #/EHsc /c
    #target_link_options(rtsp PRIVATE /INCREMENTAL:NO)
    #target_link_options(rtsp PRIVATE /DEBUG:FULL)
elseif(CMAKE_COMPILER_IS_GNUCXX)
    #target_compile_options(rtsp PRIVATE -Wno-psabi)
endif()

message("========================================================================")