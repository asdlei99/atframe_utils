﻿#.rst:
# ProjectBuildTools
# ----------------
#
# build tools
#

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.10")
    include_guard(GLOBAL)
endif()

set (PROJECT_BUILD_TOOLS_CMAKE_INHERIT_VARS 
    CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_MINSIZEREL
    CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL
    CMAKE_ASM_FLAGS CMAKE_EXE_LINKER_FLAGS CMAKE_MODULE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS CMAKE_STATIC_LINKER_FLAGS
    CMAKE_TOOLCHAIN_FILE CMAKE_AR CMAKE_RANLIB
    CMAKE_C_COMPILER CMAKE_C_COMPILER_LAUNCHER CMAKE_C_COMPILER_AR CMAKE_C_COMPILER_RANLIB CMAKE_C_LINK_LIBRARY_SUFFIX
    CMAKE_CXX_COMPILER CMAKE_CXX_COMPILER_LAUNCHER CMAKE_CXX_COMPILER_AR CMAKE_CXX_COMPILER_RANLIB CMAKE_CXX_LINK_LIBRARY_SUFFIX
    CMAKE_ASM_COMPILER CMAKE_ASM_COMPILER_LAUNCHER CMAKE_ASM_COMPILER_AR CMAKE_ASM_COMPILER_RANLIB CMAKE_ASM_LINK_LIBRARY_SUFFIX
    CMAKE_SYSTEM_NAME PROJECT_ATFRAME_TARGET_CPU_ABI 
    CMAKE_SYSROOT CMAKE_SYSROOT_COMPILE # CMAKE_SYSTEM_LIBRARY_PATH # CMAKE_SYSTEM_LIBRARY_PATH ninja里解出的参数不对，原因未知
    CMAKE_OSX_SYSROOT CMAKE_OSX_ARCHITECTURES 
    ANDROID_TOOLCHAIN ANDROID_ABI ANDROID_STL ANDROID_PIE ANDROID_PLATFORM ANDROID_CPP_FEATURES
    ANDROID_ALLOW_UNDEFINED_SYMBOLS ANDROID_ARM_MODE ANDROID_ARM_NEON ANDROID_DISABLE_NO_EXECUTE ANDROID_DISABLE_RELRO
    ANDROID_DISABLE_FORMAT_STRING_CHECKS ANDROID_CCACHE
)

macro(project_build_tools_append_cmake_inherit_options OUTVAR)
    list (APPEND ${OUTVAR} "-G" "${CMAKE_GENERATOR}")

    foreach (VAR_NAME IN LISTS PROJECT_BUILD_TOOLS_CMAKE_INHERIT_VARS)
        if (DEFINED ${VAR_NAME})
            set(VAR_VALUE "${${VAR_NAME}}")
            # message("DEBUG============ ${VAR_NAME}=${VAR_VALUE}")
            if (VAR_VALUE)
                list (APPEND ${OUTVAR} "-D${VAR_NAME}=${VAR_VALUE}")
            endif ()
            unset(VAR_VALUE)
        endif ()
    endforeach ()

    if (CMAKE_GENERATOR_PLATFORM)
        list (APPEND ${OUTVAR} "-A" "${CMAKE_GENERATOR_PLATFORM}")
    endif ()
endmacro ()

macro(project_build_tools_append_cmake_build_type_for_lib OUTVAR)
    if (CMAKE_BUILD_TYPE)
        if (MSVC)
            list (APPEND ${OUTVAR} "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
        elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
            list (APPEND ${OUTVAR} "-DCMAKE_BUILD_TYPE=RelWithDebInfo")
        else ()
            list (APPEND ${OUTVAR} "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
        endif ()
    endif ()
endmacro ()

macro(project_build_tools_append_cmake_cxx_standard_options OUTVAR)
    if (CMAKE_C_STANDARD)
        list (APPEND ${OUTVAR} "-DCMAKE_C_STANDARD=${CMAKE_C_STANDARD}")
    endif ()
    if (CMAKE_CXX_STANDARD)
        list (APPEND ${OUTVAR} "-DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}")
    endif ()
endmacro()

macro(project_build_tools_append_cmake_options_for_lib OUTVAR)
    project_build_tools_append_cmake_inherit_options(${OUTVAR})
    project_build_tools_append_cmake_build_type_for_lib(${OUTVAR})
    project_build_tools_append_cmake_cxx_standard_options(${OUTVAR})
    list (APPEND ${OUTVAR}
        "-DCMAKE_POLICY_DEFAULT_CMP0075=NEW" 
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
    )
endmacro ()

function(project_make_executable)
    if (UNIX OR MINGW OR CYGWIN OR APPLE OR CMAKE_HOST_APPLE OR CMAKE_HOST_UNIX)
        foreach(ARG IN LISTS ARGN)
            execute_process(COMMAND chmod -R +x ${ARG})
        endforeach()
    endif()
endfunction()

function (project_make_writable)
    if (CMAKE_HOST_APPLE OR APPLE OR UNIX OR MINGW OR MSYS OR CYGWIN)
        execute_process(COMMAND chmod -R +w ${ARGN})
    else ()
        foreach(arg IN LISTS ARGN)
            execute_process(COMMAND attrib -R "${arg}" /S /D /L)
        endforeach()
    endif ()
endfunction()


# 如果仅仅是设置环境变量的话可以用 ${CMAKE_COMMAND} -E env M4=/foo/bar 代替
macro (project_expand_list_for_command_line OUTPUT INPUT)
    foreach(ARG IN LISTS ${INPUT})
        string(REPLACE "\\" "\\\\" project_expand_list_for_command_line_OUT_VAR ${ARG})
        string(REPLACE "\"" "\\\"" project_expand_list_for_command_line_OUT_VAR ${project_expand_list_for_command_line_OUT_VAR})
        set (${OUTPUT} "${${OUTPUT}} \"${project_expand_list_for_command_line_OUT_VAR}\"")
        unset (project_expand_list_for_command_line_OUT_VAR)
    endforeach()
endmacro()

function (project_expand_list_for_command_line_to_file)
    unset (project_expand_list_for_command_line_to_file_OUTPUT)
    unset (project_expand_list_for_command_line_to_file_LINE)
    foreach(ARG IN LISTS ARGN)
        if (NOT project_expand_list_for_command_line_to_file_OUTPUT)
            set (project_expand_list_for_command_line_to_file_OUTPUT "${ARG}")
        else ()
            string(REPLACE "\\" "\\\\" project_expand_list_for_command_line_OUT_VAR ${ARG})
            string(REPLACE "\"" "\\\"" project_expand_list_for_command_line_OUT_VAR ${project_expand_list_for_command_line_OUT_VAR})
            if (project_expand_list_for_command_line_to_file_LINE)
                set (project_expand_list_for_command_line_to_file_LINE "${project_expand_list_for_command_line_to_file_LINE} \"${project_expand_list_for_command_line_OUT_VAR}\"")
            else ()
                set (project_expand_list_for_command_line_to_file_LINE "\"${project_expand_list_for_command_line_OUT_VAR}\"")
            endif ()
            unset (project_expand_list_for_command_line_OUT_VAR)
        endif ()
    endforeach()

    if (project_expand_list_for_command_line_to_file_OUTPUT)
        file(APPEND "${project_expand_list_for_command_line_to_file_OUTPUT}" "${project_expand_list_for_command_line_to_file_LINE}${PROJECT_THIRD_PARTY_BUILDTOOLS_BASH_EOL}")
    endif ()
    unset (project_expand_list_for_command_line_to_file_OUTPUT)
    unset (project_expand_list_for_command_line_to_file_LINE)
endfunction()

if (CMAKE_HOST_WIN32)
    set (PROJECT_THIRD_PARTY_BUILDTOOLS_EOL "\r\n")
    set (PROJECT_THIRD_PARTY_BUILDTOOLS_BASH_EOL "\r\n")
elseif (CMAKE_HOST_APPLE OR APPLE)
    set (PROJECT_THIRD_PARTY_BUILDTOOLS_EOL "\r")
    set (PROJECT_THIRD_PARTY_BUILDTOOLS_BASH_EOL "\n")
else ()
    set (PROJECT_THIRD_PARTY_BUILDTOOLS_EOL "\n")
    set (PROJECT_THIRD_PARTY_BUILDTOOLS_BASH_EOL "\n")
endif ()
