﻿if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.10")
    include_guard(GLOBAL)
endif()

# default configure, can be load multiple times
#####################################################################
if(NOT DEFINED __COMPILER_OPTION_LOADED)
    include(CheckCXXSourceCompiles)
    set(__COMPILER_OPTION_LOADED 1)
    option(COMPILER_OPTION_MSVC_ZC_CPP "Add /Zc:__cplusplus for MSVC (let __cplusplus be equal to _MSVC_LANG) when it support." ON)
    option(COMPILER_OPTION_CLANG_ENABLE_LIBCXX "Try to use libc++ when using clang." ON)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Enable IndependentCode")

    if (CMAKE_CONFIGURATION_TYPES)
        message(STATUS "Available Build Type: ${CMAKE_CONFIGURATION_TYPES}")
    else()
        message(STATUS "Available Build Type: Unknown")
    endif()

    if(NOT CMAKE_BUILD_TYPE)
        #set(CMAKE_BUILD_TYPE "Debug")
        set(CMAKE_BUILD_TYPE "RelWithDebInfo")
    endif()

    # utility functions and macros
    macro(add_compiler_flags_to_var)
        unset(add_compiler_flags_to_var_VARNAME)
        foreach(def ${ARGV})
            if ( NOT add_compiler_flags_to_var_VARNAME )
                set(add_compiler_flags_to_var_VARNAME ${def})
            else()
                if (${add_compiler_flags_to_var_VARNAME})
                    set(${add_compiler_flags_to_var_VARNAME} "${${add_compiler_flags_to_var_VARNAME}} ${def}")
                else ()
                    set(${add_compiler_flags_to_var_VARNAME} ${def})
                endif ()
            endif()
        endforeach()
        unset(add_compiler_flags_to_var_VARNAME)
    endmacro(add_compiler_flags_to_var)

    macro(add_compiler_define)
        foreach(def ${ARGV})
            if ( NOT MSVC )
                add_compile_options(-D${def})
            else()
                add_compile_options("/D ${def}")
            endif()
        endforeach()
    endmacro(add_compiler_define)

    macro(add_linker_flags_for_runtime)
        foreach(def ${ARGV})
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${def}")
            set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${def}")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${def}")
        endforeach()
    endmacro(add_linker_flags_for_runtime)

    macro(add_linker_flags_for_all)
        foreach(def ${ARGV})
            add_linker_flags_for_runtime(${def})
            set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} ${def}")
        endforeach()
    endmacro(add_linker_flags_for_all)

    macro(try_set_compiler_lang_standard VARNAME STDVERSION)
        if (NOT ${VARNAME})
            set(${VARNAME} ${STDVERSION})
        endif ()
    endmacro(try_set_compiler_lang_standard)

    # Auto compiler options, support gcc,MSVC,Clang,AppleClang
    if( ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        # add_compile_options(-Wall -Werror)
        list(APPEND COMPILER_STRICT_EXTRA_CFLAGS -Wextra)
        list(APPEND COMPILER_STRICT_CFLAGS -Wall -Werror)

        include(CheckCCompilerFlag)
        CHECK_C_COMPILER_FLAG(-rdynamic LD_FLAGS_RDYNAMIC_AVAILABLE)
        if(LD_FLAGS_RDYNAMIC_AVAILABLE)
            message(STATUS "Check Flag: -rdynamic -- yes")
            add_linker_flags_for_runtime(-rdynamic)
        else()
            message(STATUS "Check Flag: -rdynamic -- no")
        endif()

        # gcc 4.9 or upper add colorful output
        if ( CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "4.9.0")
            add_compile_options(-fdiagnostics-color=auto)
        endif()
        # disable -Wno-unused-local-typedefs (which is often used in type_traits)
        if ( CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "4.8.0")
            # GCC < 4.8 doesn't support the address sanitizer
            # -fsanitize=address require -lasan be placed before -lstdc++, every target shoud add this
            add_compile_options(-Wno-unused-local-typedefs)
            message(STATUS "GCC Version ${CMAKE_CXX_COMPILER_VERSION} Found, -Wno-unused-local-typedefs added.")
        endif()

        if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "5.0.0")
            try_set_compiler_lang_standard(CMAKE_C_STANDARD 11)
            if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.12.0")
                try_set_compiler_lang_standard(CMAKE_CXX_STANDARD 20)
            elseif (CMAKE_VERSION VERSION_GREATER_EQUAL "3.8.0")
                try_set_compiler_lang_standard(CMAKE_CXX_STANDARD 17)
            else()
                try_set_compiler_lang_standard(CMAKE_CXX_STANDARD 14)
            endif()
            message(STATUS "GCC Version ${CMAKE_CXX_COMPILER_VERSION} , using -std=c${CMAKE_C_STANDARD}/c++${CMAKE_CXX_STANDARD}.")
        elseif ( CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "4.7.0")
            try_set_compiler_lang_standard(CMAKE_C_STANDARD 11)
            try_set_compiler_lang_standard(CMAKE_CXX_STANDARD 14)
            message(STATUS "GCC Version ${CMAKE_CXX_COMPILER_VERSION} , using -std=c${CMAKE_C_STANDARD}/c++${CMAKE_CXX_STANDARD}.")
        elseif( CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "4.4.0")
            add_compiler_flags_to_var(CMAKE_CXX_FLAGS -std=c++0x)
            message(STATUS "GCC Version ${CMAKE_CXX_COMPILER_VERSION} , using -std=c++0x.")
        endif()

        if(MINGW)
            # list(APPEND COMPILER_OPTION_EXTERN_CXX_LIBS stdc++)
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
        endif()

    elseif( ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
        # add_compile_options(-Wall -Werror)
        list(APPEND COMPILER_STRICT_EXTRA_CFLAGS -Wextra)
        list(APPEND COMPILER_STRICT_CFLAGS -Wall -Werror)
        if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "3.4")
            try_set_compiler_lang_standard(CMAKE_C_STANDARD 11)
            if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.12.0")
                try_set_compiler_lang_standard(CMAKE_CXX_STANDARD 20)
            elseif (CMAKE_VERSION VERSION_GREATER_EQUAL "3.8.0")
                try_set_compiler_lang_standard(CMAKE_CXX_STANDARD 17)
            else()
                try_set_compiler_lang_standard(CMAKE_CXX_STANDARD 14)
            endif()
        elseif ( CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "3.3")
            try_set_compiler_lang_standard(CMAKE_C_STANDARD 11)
            try_set_compiler_lang_standard(CMAKE_CXX_STANDARD 11)
        endif()
        message(STATUS "Clang Version ${CMAKE_CXX_COMPILER_VERSION} , using -std=c${CMAKE_C_STANDARD}/c++${CMAKE_CXX_STANDARD}.")
        # Test libc++ and libc++abi
        set(COMPILER_CLANG_TEST_BAKCUP_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
        set(COMPILER_CLANG_TEST_BAKCUP_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
        set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -stdlib=libc++")
        list(APPEND CMAKE_REQUIRED_LIBRARIES c++ c++abi)
        check_cxx_source_compiles("
        #include <iostream>
        int main() {
            std::cout<< __cplusplus<< std::endl;
            return 0;
        }
        " COMPILER_CLANG_TEST_LIBCXX)
        set(CMAKE_REQUIRED_FLAGS ${COMPILER_CLANG_TEST_BAKCUP_CMAKE_REQUIRED_FLAGS})
        set(CMAKE_REQUIRED_LIBRARIES ${COMPILER_CLANG_TEST_BAKCUP_CMAKE_REQUIRED_LIBRARIES})
        unset(COMPILER_CLANG_TEST_BAKCUP_CMAKE_REQUIRED_FLAGS)
        unset(COMPILER_CLANG_TEST_BAKCUP_CMAKE_REQUIRED_LIBRARIES)
        if(COMPILER_OPTION_CLANG_ENABLE_LIBCXX AND COMPILER_CLANG_TEST_LIBCXX)
            add_compiler_flags_to_var(CMAKE_CXX_FLAGS -stdlib=libc++)
            message(STATUS "Clang use stdlib=libc++")
            list(APPEND COMPILER_OPTION_EXTERN_CXX_LIBS c++ c++abi)
        else()
            if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "3.9")
                message(STATUS "Clang use stdlib=default(libstdc++)")
            else ()
                add_compile_options(-D__STRICT_ANSI__)
                message(STATUS "Clang use stdlib=default(libstdc++) and add -D__STRICT_ANSI__")
            endif()
            if(MINGW)
                # list(APPEND COMPILER_OPTION_EXTERN_CXX_LIBS stdc++)
                set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
                set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
            endif()
        endif()

    elseif( ${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang")
        # add_compile_options(-Wall -Werror)
        list(APPEND COMPILER_STRICT_EXTRA_CFLAGS -Wextra -Wno-implicit-fallthrough)
        list(APPEND COMPILER_STRICT_CFLAGS -Wall -Werror)
        if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "6.0")
            try_set_compiler_lang_standard(CMAKE_C_STANDARD 11)
            if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.12.0")
                try_set_compiler_lang_standard(CMAKE_CXX_STANDARD 20)
            elseif (CMAKE_VERSION VERSION_GREATER_EQUAL "3.8.0")
                try_set_compiler_lang_standard(CMAKE_CXX_STANDARD 17)
            else()
                try_set_compiler_lang_standard(CMAKE_CXX_STANDARD 14)
            endif()
        elseif ( CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "5.0")
            try_set_compiler_lang_standard(CMAKE_C_STANDARD 11)
            try_set_compiler_lang_standard(CMAKE_CXX_STANDARD 11)
        endif()
        message(STATUS "AppleClang Version ${CMAKE_CXX_COMPILER_VERSION} , using -std=c${CMAKE_C_STANDARD}/c++${CMAKE_CXX_STANDARD}.")
        # Test libc++ and libc++abi
        set(COMPILER_CLANG_TEST_BAKCUP_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
        set(COMPILER_CLANG_TEST_BAKCUP_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
        set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -stdlib=libc++")
        list(APPEND CMAKE_REQUIRED_LIBRARIES c++ c++abi)
        check_cxx_source_compiles("
        #include <iostream>
        int main() {
            std::cout<< __cplusplus<< std::endl;
            return 0;
        }
        " COMPILER_CLANG_TEST_LIBCXX)
        set(CMAKE_REQUIRED_FLAGS ${COMPILER_CLANG_TEST_BAKCUP_CMAKE_REQUIRED_FLAGS})
        set(CMAKE_REQUIRED_LIBRARIES ${COMPILER_CLANG_TEST_BAKCUP_CMAKE_REQUIRED_LIBRARIES})
        unset(COMPILER_CLANG_TEST_BAKCUP_CMAKE_REQUIRED_FLAGS)
        unset(COMPILER_CLANG_TEST_BAKCUP_CMAKE_REQUIRED_LIBRARIES)
        if(COMPILER_OPTION_CLANG_ENABLE_LIBCXX AND COMPILER_CLANG_TEST_LIBCXX)
            add_compiler_flags_to_var(CMAKE_CXX_FLAGS -stdlib=libc++)
            message(STATUS "AppleClang use stdlib=libc++")
            list(APPEND COMPILER_OPTION_EXTERN_CXX_LIBS c++ c++abi)
        else()
            message(STATUS "AppleClang use stdlib=default(libstdc++)")
        endif()
    elseif(MSVC)
        list(APPEND COMPILER_STRICT_CFLAGS /W4 /wd4100 /wd4125 /wd4566 /wd4127 /wd4512 /WX)
        add_linker_flags_for_runtime(/ignore:4217)
        
        add_compiler_flags_to_var(CMAKE_CXX_FLAGS /MP /EHsc)
        add_compiler_flags_to_var(CMAKE_C_FLAGS /MP /EHsc)
        try_set_compiler_lang_standard(CMAKE_C_STANDARD 11)
        if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.12.0")
            try_set_compiler_lang_standard(CMAKE_CXX_STANDARD 20)
        elseif (CMAKE_VERSION VERSION_GREATER_EQUAL "3.8.0")
            try_set_compiler_lang_standard(CMAKE_CXX_STANDARD 17)
        else()
            try_set_compiler_lang_standard(CMAKE_CXX_STANDARD 14)
        endif()
        # https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warnings-by-compiler-version
        # https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=vs-2019#microsoft-specific-predefined-macros
        # if (MSVC_VERSION GREATER_EQUAL 1910)
        #     add_compiler_flags_to_var(CMAKE_CXX_FLAGS /std:c++17)
        #     message(STATUS "MSVC ${MSVC_VERSION} found. using /std:c++17")
        # endif()
        # set __cplusplus to standard value, @see https://docs.microsoft.com/zh-cn/cpp/build/reference/zc-cplusplus
        if (MSVC_VERSION GREATER_EQUAL 1914 AND COMPILER_OPTION_MSVC_ZC_CPP)
            add_compiler_flags_to_var(CMAKE_CXX_FLAGS /Zc:__cplusplus)
        endif()
    endif()

    # check c++20 coroutine
    set(COMPILER_OPTIONS_BAKCUP_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
    if ( NOT MSVC )
        if (NOT EMSCRIPTEN)
            add_compiler_flags_to_var(CMAKE_CXX_FLAGS_DEBUG -ggdb)
            add_compiler_flags_to_var(CMAKE_CXX_FLAGS_RELWITHDEBINFO -ggdb)
        endif()
        # add_compiler_flags_to_var(CMAKE_CXX_FLAGS_DEBUG -ggdb)
        # add_compiler_flags_to_var(CMAKE_CXX_FLAGS_RELEASE)
        # add_compiler_flags_to_var(CMAKE_CXX_FLAGS_RELWITHDEBINFO -ggdb)
        # add_compiler_flags_to_var(CMAKE_CXX_FLAGS_MINSIZEREL)

        # Try add coroutine
        set(CMAKE_REQUIRED_FLAGS "${COMPILER_OPTIONS_BAKCUP_CMAKE_REQUIRED_FLAGS} -fcoroutines")
        check_cxx_source_compiles("
        #include <coroutine>
        int main() {
            return std::suspend_always().await_ready()? 0: 1;
        }
        " COMPILER_OPTIONS_TEST_STD_COROUTINE)
        if (COMPILER_OPTIONS_TEST_STD_COROUTINE)
            add_compiler_flags_to_var(CMAKE_CXX_FLAGS -fcoroutines)
        else()
            set(CMAKE_REQUIRED_FLAGS "${COMPILER_OPTIONS_BAKCUP_CMAKE_REQUIRED_FLAGS} -fcoroutines-ts")
            check_cxx_source_compiles("
            #include <experimental/coroutine>
            int main() {
                return std::experimental::suspend_always().await_ready()? 0: 1;
            }
            " COMPILER_OPTIONS_TEST_STD_COROUTINE_TS)
            if (COMPILER_OPTIONS_TEST_STD_COROUTINE_TS)
                add_compiler_flags_to_var(CMAKE_CXX_FLAGS -fcoroutines-ts)
            endif()
        endif()
    else()
        if(NOT CMAKE_MSVC_RUNTIME)
            set(CMAKE_MSVC_RUNTIME "MD")
        endif()
        add_compiler_flags_to_var(CMAKE_CXX_FLAGS_DEBUG /Od /${CMAKE_MSVC_RUNTIME}d)
        add_compiler_flags_to_var(CMAKE_CXX_FLAGS_RELEASE /O2 /${CMAKE_MSVC_RUNTIME} /D NDEBUG)
        add_compiler_flags_to_var(CMAKE_CXX_FLAGS_RELWITHDEBINFO /O2 /${CMAKE_MSVC_RUNTIME} /D NDEBUG)
        add_compiler_flags_to_var(CMAKE_CXX_FLAGS_MINSIZEREL /Ox /${CMAKE_MSVC_RUNTIME} /D NDEBUG)

        # Try add coroutine
        set(CMAKE_REQUIRED_FLAGS "${COMPILER_OPTIONS_BAKCUP_CMAKE_REQUIRED_FLAGS} /await")
        check_cxx_source_compiles("#include <coroutine>
        int main() {
            return std::suspend_always().await_ready()? 0: 1;
        }" COMPILER_OPTIONS_TEST_STD_COROUTINE)
        if (NOT COMPILER_OPTIONS_TEST_STD_COROUTINE)
            check_cxx_source_compiles("
            #include <experimental/coroutine>
            int main() {
                return std::experimental::suspend_always().await_ready()? 0: 1;
            }
            " COMPILER_OPTIONS_TEST_STD_COROUTINE_TS)
        endif()
        if (COMPILER_OPTIONS_TEST_STD_COROUTINE OR COMPILER_OPTIONS_TEST_STD_COROUTINE_TS)
            add_compiler_flags_to_var(CMAKE_CXX_FLAGS /await)
        endif()
    endif()
    set(CMAKE_REQUIRED_FLAGS ${COMPILER_OPTIONS_BAKCUP_CMAKE_REQUIRED_FLAGS})
    unset(COMPILER_OPTIONS_BAKCUP_CMAKE_REQUIRED_FLAGS)
    # Check if exception enabled
    check_cxx_source_compiles("int main () { try { throw 123; } catch (...) {} return 0; }" COMPILER_OPTIONS_TEST_EXCEPTION)
    if (COMPILER_OPTIONS_TEST_EXCEPTION)
        check_cxx_source_compiles("
        #include <exception>
        void handle_eptr(std::exception_ptr eptr) {
            try {
                if (eptr) {
                    std::rethrow_exception(eptr);
                }
            } catch(...) {}
        }
    
        int main() {
            std::exception_ptr eptr;
            try {
                throw 1;
            } catch(...) {
                eptr = std::current_exception(); // capture
            }
            handle_eptr(eptr);
        }
        " COMPILER_OPTIONS_TEST_STD_EXCEPTION_PTR)
    else ()
        unset(COMPILER_OPTIONS_TEST_STD_EXCEPTION_PTR CACHE)
    endif ()
    # Check if rtti enabled
    check_cxx_source_compiles("#include <typeinfo>
    #include <cstdio>
    int main () { puts(typeid(int).name()); return 0; }" COMPILER_OPTIONS_TEST_RTTI)
endif()
