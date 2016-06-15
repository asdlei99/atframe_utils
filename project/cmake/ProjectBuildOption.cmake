﻿# 功能检测选项
include(WriteCompilerDetectionHeader)

# generate check header
write_compiler_detection_header(
    FILE "${PROJECT_ALL_INCLUDE_DIR}/config/compiler_features.h"
    PREFIX UTIL_CONFIG
    COMPILERS GNU Clang AppleClang MSVC
    FEATURES cxx_auto_type cxx_constexpr cxx_decltype cxx_decltype_auto cxx_defaulted_functions cxx_deleted_functions cxx_final cxx_override cxx_range_for cxx_noexcept cxx_nullptr cxx_static_assert cxx_thread_local cxx_variadic_templates cxx_lambdas
)
#file(MAKE_DIRECTORY "${PROJECT_ALL_INCLUDE_DIR}/config")
#file(RENAME "${CMAKE_BINARY_DIR}/compiler_features.h" "${PROJECT_ALL_INCLUDE_DIR}/config/compiler_features.h")

# 默认配置选项
#####################################################################

# 内存混淆int
set(ENABLE_MIXEDINT_MAGIC_MASK 0 CACHE STRING "Integer mixed magic mask")
if (NOT ENABLE_MIXEDINT_MAGIC_MASK)
    set(ENABLE_MIXEDINT_MAGIC_MASK 0)
endif()

if (${ENABLE_MIXEDINT_MAGIC_MASK} GREATER 0)
    add_compiler_define("ENABLE_MIXEDINT_MAGIC_MASK=${ENABLE_MIXEDINT_MAGIC_MASK}")
endif()

# Lua模块
find_package(Lua51)
if (LUA51_FOUND)
    include_directories(${LUA_INCLUDE_DIR})
    list(APPEND EXTENTION_LINK_LIB ${LUA_LIBRARIES})
    message(STATUS "Lua support enabled.(lua 5.1 detected)")
elseif(ENABLE_LUA_SUPPORT)
    message(STATUS "Lua support enabled.(enabled by custom)")
else()
    message(STATUS "Lua support disabled")
	add_compiler_define("LOG_WRAPPER_DISABLE_LUA_SUPPORT=1")
endif()

# 测试配置选项
set(GTEST_ROOT "" CACHE STRING "GTest root directory")
set(BOOST_ROOT "" CACHE STRING "Boost root directory")
option(PROJECT_TEST_ENABLE_BOOST_UNIT_TEST "Enable boost unit test." OFF)

option(PROJECT_ENABLE_UNITTEST "Enable unit test" OFF)
option(PROJECT_ENABLE_SAMPLE "Enable sample" OFF)
