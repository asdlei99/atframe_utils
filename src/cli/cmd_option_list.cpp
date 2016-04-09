﻿/*
* cmd_option_list.cpp
*
*  Created on: 2011-12-29
*      Author: OWenT
*
* 应用程序命令处理
*
*/

#include "cli/cmd_option_list.h"

namespace util {
    namespace cli {
        const char *binder::cmd_option_bind_base::ROOT_NODE_CMD = "@Root";

        cmd_option_list::cmd_option_list() : ext_param_(NULL) {}

        cmd_option_list::cmd_option_list(int argv, const char *argc[]) : ext_param_(NULL) {
            for (int i = 0; i < argv; ++i)
                keys_.push_back(value_type(new cmd_option_value(argc[i])));
        }

        cmd_option_list::cmd_option_list(const std::vector<std::string> &cmds) : ext_param_(NULL) {
            std::vector<std::string>::size_type uSize = cmds.size();
            for (std::vector<std::string>::size_type i = 0; i < uSize; ++i) {
                keys_.push_back(value_type(new cmd_option_value(cmds[i].c_str())));
            }
        }


        void cmd_option_list::init_key_value_map() {
            typedef std::map<std::string, value_type> key_map_type;
            typedef std::vector<value_type> keys_type;
            // 已经初始化，跳过
            if (key_value_.get() != NULL) return;

            key_value_ = std::shared_ptr<key_map_type>(new key_map_type());

            keys_type::size_type uLen = (keys_.size() / 2) * 2; // 去除末尾无value的key
            for (keys_type::size_type i = 0; i < uLen; i += 2) {
                (*key_value_)[keys_[i]->to_string()] = keys_[i + 1];
            }
        }

        void cmd_option_list::add(const char *param) { keys_.push_back(value_type(new cmd_option_value(param))); }

        void cmd_option_list::clear() {
            key_value_.reset(); // 删除key-value映射
            keys_.clear();      // 删除索引下标映射
            cmd_array_.clear(); // 删除指令栈集合
            ext_param_ = NULL;  // 透传参数置空
        }

        void cmd_option_list::load_cmd_array(const cmd_array_type &cmds) { cmd_array_ = cmds; }

        void cmd_option_list::append_cmd(const char *cmd_content, std::shared_ptr<binder::cmd_option_bind_base> base_node) {
            cmd_array_.push_back(std::make_pair(cmd_content, base_node));
        }

        void cmd_option_list::pop_cmd() { cmd_array_.pop_back(); }

        const cmd_option_list::cmd_array_type &cmd_option_list::get_cmd_array() const { return cmd_array_; }

        cmd_option_list::value_type cmd_option_list::get(std::string key, const char *default_val) {
            value_type ret_ptr = get(key);
            if (ret_ptr.get() == NULL) return value_type(new cmd_option_value(default_val));
            return ret_ptr;
        }

        cmd_option_list::value_type cmd_option_list::get(std::string key) {
            init_key_value_map();

            std::map<std::string, value_type>::const_iterator itr;
            itr = key_value_->find(key);
            if (itr == key_value_->end()) return value_type();
            return itr->second;
        }

        cmd_option_list::value_type cmd_option_list::get(size_type index) const { return keys_[index]; }

        // 操作符重载，功能和上面一样
        cmd_option_list::value_type cmd_option_list::operator[](size_type index) const { return keys_[index]; }

        // 获取参数数量
        cmd_option_list::size_type cmd_option_list::get_params_number() const { return keys_.size(); }

        // 重置Key-Value映射表
        void cmd_option_list::reset_key_value_map() { key_value_.reset(); }

        void cmd_option_list::set_ext_param(void *param) { ext_param_ = param; }

        void *cmd_option_list::get_ext_param() const { return ext_param_; }
    }
}
