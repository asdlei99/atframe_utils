﻿/**
 * @file singleton.h
 * @brief 单件模式基类, 参考 boost::serialization::singleton,去除了Debug锁操作 <br />
 *        实例的初始化会在模块载入（如果是动态链接库则是载入动态链接库）时启动 <br />
 *        在模块卸载时会自动析构 <br />
 *
 * Note that this singleton class is thread-safe.
 *
 * @version 1.0
 * @author owent
 * @date 2012.02.13
 *
 * @history
 *   2012.07.20 为线程安全而改进实现方式
 *   2015.01.10 改为使用双检锁实现线程安全
 *   2015.11.02 增加内存屏障，保证极端情况下多线程+编译优化导致的指令乱序问题
 *   2019-12-05 移除对noncopyable的继承链，以适应用于dllexport时自带的 util::design_pattern::noncopyable 未导出的问题
 *              增加nomovable实现
 * 
 * @note 如果在Windows下dll请使用宏来生成函数
 *          UTIL_DESIGN_PATTERN_SINGLETON_VISIBLE_DECL(类名): visiable 标记(.lib)
 *          UTIL_DESIGN_PATTERN_SINGLETON_IMPORT_DECL(类名): __attribute__((__dllimport__))/__declspec(dllimport) 标记(从其他dll中导入)
 *          UTIL_DESIGN_PATTERN_SINGLETON_EXPORT_DECL(类名): __attribute__((__dllexport__))/__declspec(dllexport) 标记(从其他dll中导入)
 *       然后需要在源文件中使用UTIL_DESIGN_PATTERN_SINGLETON_EXPORT_DECL(类名)的模块中导出实例符号
 *          UTIL_SYMBOL_EXPORT 类名::singleton_data_t 类名::singleton_wrapper_t::data;
 *       如果单例对象不需要导出则也可以直接util::design_pattern::singleton<类名>或者用VISIBLE导出规则
 * @example 
 *      // singleton_class.h
 *      class singleton_class : public util::design_pattern::singleton<singleton_class> {};
 * @example 
 *      // singleton_class.h
 *      class singleton_class {
 *      #if (defined(LIB_EXPORT) && LIB_EXPORT) || (defined(EXE_EXPORT) && EXE_EXPORT)
 *         UTIL_DESIGN_PATTERN_SINGLETON_VISIBLE_DECL(singleton_class)
 *      #elif defined(DLL_EXPORT) && DLL_EXPORT
 *         UTIL_DESIGN_PATTERN_SINGLETON_EXPORT_DECL(singleton_class)
 *      #else
 *         UTIL_DESIGN_PATTERN_SINGLETON_IMPORT_DECL(singleton_class)
 *      #endif
 *      };
 *      // singleton_class.cpp
 *      #if (defined(LIB_EXPORT) && LIB_EXPORT) || (defined(EXE_EXPORT) && EXE_EXPORT)
 *          UTIL_SYMBOL_VISIBLE singleton_class::singleton_data_t singleton_class::singleton_wrapper_t::data;
 *      #elif defined(DLL_EXPORT) && DLL_EXPORT
 *          UTIL_SYMBOL_EXPORT singleton_class::singleton_data_t singleton_class::singleton_wrapper_t::data;
 *      #else
 *          UTIL_SYMBOL_IMPORT singleton_class::singleton_data_t singleton_class::singleton_wrapper_t::data;
 *      #endif
 */

#ifndef UTILS_DESIGNPATTERN_SINGLETON_H
#define UTILS_DESIGNPATTERN_SINGLETON_H

#pragma once

#include <cstddef>
#include <memory>

#include <config/compiler_features.h>
#include <config/compile_optimize.h>

#include "lock/lock_holder.h"
#include "lock/spin_lock.h"
#include "std/smart_ptr.h"

/**
 * @brief if you are under Windows, you may want to declare import/export of singleton<T>
 * @usage UTIL_DESIGN_PATTERN_SINGLETON_IMPORT(Your Class Name)
 *        UTIL_DESIGN_PATTERN_SINGLETON_EXPORT(Your Class Name)
 */
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32) || defined(__CYGWIN__)
    #define UTIL_DESIGN_PATTERN_SINGLETON_IMPORT(T)                                                         \
        UTIL_SYMBOL_IMPORT bool ::util::design_pattern::wrapper::singleton_wrapper<T>::destroyed_ = false;  \
        template class UTIL_SYMBOL_IMPORT ::util::design_pattern::singleton<T>;
        
    #define UTIL_DESIGN_PATTERN_SINGLETON_EXPORT(T)                                                         \
        UTIL_SYMBOL_EXPORT bool ::util::design_pattern::wrapper::singleton_wrapper<T>::destroyed_ = false;  \
        template class UTIL_SYMBOL_EXPORT ::util::design_pattern::singleton<T>;

#else
    #define UTIL_DESIGN_PATTERN_SINGLETON_IMPORT(T)
    #define UTIL_DESIGN_PATTERN_SINGLETON_EXPORT(T)
#endif

#if defined(UTIL_CONFIG_COMPILER_CXX_RVALUE_REFERENCES) && UTIL_CONFIG_COMPILER_CXX_RVALUE_REFERENCES
#define UTIL_DESIGN_PATTERN_SINGLETON_NOMAVLBLE(CLAZZ)              \
            CLAZZ(CLAZZ &&) UTIL_CONFIG_DELETED_FUNCTION;           \
            CLAZZ &operator=(CLAZZ &&) UTIL_CONFIG_DELETED_FUNCTION;
#else
#define UTIL_DESIGN_PATTERN_SINGLETON_NOMAVLBLE(CLAZZ)
#endif

#define UTIL_DESIGN_PATTERN_SINGLETON_DEF_FUNCS(LABEL, CLAZZ, BASE_CLAZZ)                       \
private:                                                                                        \
    template<class TCLASS>                                                                      \
    class singleton_wrapper_permission_t : public TCLASS { };                                   \
    class LABEL singleton_data_t {                                                              \
    public:                                                                                     \
        bool destroyed;                                                                         \
        std::shared_ptr<CLAZZ> instance;                                                        \
        util::lock::spin_lock lock;                                                             \
        singleton_data_t(): destroyed(false) {}                                                 \
    };                                                                                          \
    class LABEL singleton_wrapper_t {                                                           \
    public:                                                                                     \
        typedef std::shared_ptr<singleton_wrapper_permission_t<CLAZZ> > ptr_permission_t;       \
        typedef std::shared_ptr<CLAZZ> ptr_t;                                                   \
        static LABEL singleton_data_t data;                                                     \
        struct deleter {                                                                        \
            void operator()(singleton_wrapper_permission_t<CLAZZ>* p) const {                   \
                data.destroyed = true;                                                          \
                delete p;                                                                       \
            }                                                                                   \
        };                                                                                      \
        static LABEL void use(CLAZZ const &) {}                                                 \
        static LABEL ptr_t &me() {                                                              \
            if (!data.instance) {                                                               \
                util::lock::lock_holder<util::lock::spin_lock> lock_opr(data.lock);             \
                UTIL_LOCK_ATOMIC_THREAD_FENCE(::util::lock::memory_order_acquire);              \
                do {                                                                            \
                    if (data.instance) {                                                        \
                        break;                                                                  \
                    }                                                                           \
                    ptr_t new_data = std::static_pointer_cast<CLAZZ>(ptr_permission_t(new singleton_wrapper_permission_t<CLAZZ>(), deleter())); \
                    data.instance    = new_data;                                                \
                } while (false);                                                                \
                UTIL_LOCK_ATOMIC_THREAD_FENCE(::util::lock::memory_order_release);              \
                use(*data.instance);                                                            \
            }                                                                                   \
            return data.instance;                                                               \
        }                                                                                       \
    };                                                                                          \
private:                                                                                        \
    friend class singleton_wrapper_t;                                                           \
    BASE_CLAZZ(const BASE_CLAZZ &) UTIL_CONFIG_DELETED_FUNCTION;                                \
    BASE_CLAZZ &operator=(const BASE_CLAZZ &) UTIL_CONFIG_DELETED_FUNCTION;                     \
    UTIL_DESIGN_PATTERN_SINGLETON_NOMAVLBLE(BASE_CLAZZ)                                         \
public:                                                                                         \
    static LABEL CLAZZ &get_instance() { return *singleton_wrapper_t::me(); }                   \
    static LABEL const CLAZZ &get_const_instance() { return get_instance(); }                   \
    static LABEL CLAZZ *instance() { return singleton_wrapper_t::me().get(); }                  \
    static LABEL std::shared_ptr<CLAZZ> &me() { return singleton_wrapper_t::me(); }             \
    static LABEL bool is_instance_destroyed() { return singleton_wrapper_t::data.destroyed; }   \
private:


#define UTIL_DESIGN_PATTERN_SINGLETON_IMPORT_DECL(CLAZZ)                                        \
    UTIL_DESIGN_PATTERN_SINGLETON_DEF_FUNCS(UTIL_SYMBOL_IMPORT, CLAZZ, CLAZZ)

#define UTIL_DESIGN_PATTERN_SINGLETON_EXPORT_DECL(CLAZZ)                                        \
    UTIL_DESIGN_PATTERN_SINGLETON_DEF_FUNCS(UTIL_SYMBOL_EXPORT, CLAZZ, CLAZZ)

#define UTIL_DESIGN_PATTERN_SINGLETON_VISIBLE_DECL(CLAZZ)                                       \
    UTIL_DESIGN_PATTERN_SINGLETON_DEF_FUNCS(UTIL_SYMBOL_VISIBLE, CLAZZ, CLAZZ)

#define UTIL_DESIGN_PATTERN_SINGLETON_LOCAL_DECL(CLAZZ)                                         \
    UTIL_DESIGN_PATTERN_SINGLETON_DEF_FUNCS(UTIL_SYMBOL_LOCAL, CLAZZ, CLAZZ)

namespace util {
    namespace design_pattern {
        template <class T>
        class singleton {
        public:
            /**
             * @brief 自身类型声明
             */
            typedef T                          self_type;
            typedef std::shared_ptr<self_type> ptr_t;

            UTIL_SYMBOL_VISIBLE singleton() {}
            UTIL_SYMBOL_VISIBLE ~singleton() {}

            UTIL_DESIGN_PATTERN_SINGLETON_DEF_FUNCS(UTIL_SYMBOL_VISIBLE, self_type, singleton)
        };

        template <class T>
        UTIL_SYMBOL_VISIBLE typename singleton<T>::singleton_data_t singleton<T>::singleton_wrapper_t::data;

        template <class T>
        class local_singleton {
        public:
            /**
             * @brief 自身类型声明
             */
            typedef T                          self_type;
            typedef std::shared_ptr<self_type> ptr_t;

            UTIL_SYMBOL_LOCAL local_singleton() {}
            UTIL_SYMBOL_LOCAL ~local_singleton() {}

            UTIL_DESIGN_PATTERN_SINGLETON_DEF_FUNCS(UTIL_SYMBOL_LOCAL, self_type, local_singleton)
        };

        template <class T>
        UTIL_SYMBOL_LOCAL typename local_singleton<T>::singleton_data_t local_singleton<T>::singleton_wrapper_t::data;
    } // namespace design_pattern
} // namespace util
#endif
