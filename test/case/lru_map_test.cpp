﻿#include <cstring>

#include "frame/test_macros.h"

#ifdef max
#undef max
#endif

#include "mem_pool/lru_map.h"


CASE_TEST(lru_map_test, basic_container) {
    typedef util::mempool::lru_map<int, long> lru_t;
    lru_t                                     lru;
    lru.reserve(128);

    typedef std::pair<lru_t::iterator, bool> insert_pair_t;

    CASE_EXPECT_TRUE(lru.empty());
    CASE_EXPECT_EQ(0, lru.size());


    insert_pair_t res = lru.insert_key_value(1, 101);
    CASE_EXPECT_TRUE(res.second);
    CASE_EXPECT_EQ(1, (*res.first).first);
    CASE_EXPECT_EQ(101, *(*res.first).second);

    CASE_EXPECT_FALSE(lru.empty());
    CASE_EXPECT_EQ(1, lru.size());

    std::vector<std::pair<int, std::shared_ptr<long> > > vec;
    vec.push_back(std::pair<int, std::shared_ptr<long> >(2, std::make_shared<long>(102)));
    vec.push_back(std::pair<int, std::shared_ptr<long> >(3, std::make_shared<long>(103)));
    lru.insert(vec.begin(), vec.end());
    CASE_EXPECT_EQ(3, lru.size());

    lru[4] = 104;
    CASE_EXPECT_EQ(4, lru.size());

    CASE_EXPECT_EQ(1, lru.front().first);
    CASE_EXPECT_EQ(101, *lru.front().second);
    CASE_EXPECT_EQ(4, lru.back().first);
    CASE_EXPECT_EQ(104, *lru.back().second);

    // insert invalid
    res = lru.insert_key_value(1, 1001);
    CASE_EXPECT_FALSE(res.second);
    res = lru.insert_key_value(2, 1002);
    CASE_EXPECT_FALSE(res.second);
    res = lru.insert_key_value(3, 1003);
    CASE_EXPECT_FALSE(res.second);
    res = lru.insert_key_value(4, 1004);
    CASE_EXPECT_FALSE(res.second);

    // pop
    lru.pop_front();
    lru.pop_back();
    CASE_EXPECT_EQ(2, lru.size());

    CASE_EXPECT_EQ(2, lru.front().first);
    CASE_EXPECT_EQ(102, *lru.front().second);
    CASE_EXPECT_EQ(3, lru.back().first);
    CASE_EXPECT_EQ(103, *lru.back().second);
    CASE_EXPECT_EQ(2, (*lru.cbegin()).first);
    CASE_EXPECT_EQ(102, *(*lru.cbegin()).second);
    CASE_EXPECT_FALSE(lru.cbegin() == lru.cend());

    // swap
    lru_t lru2;
    lru2.swap(lru);
    CASE_EXPECT_TRUE(lru.empty());
    CASE_EXPECT_EQ(0, lru.size());

    CASE_EXPECT_FALSE(lru2.empty());
    CASE_EXPECT_EQ(2, lru2.size());

    // find - erase(iterator)
    res.first = lru2.find(3);
    CASE_EXPECT_FALSE(lru2.end() == res.first);
    CASE_EXPECT_TRUE(lru2.end() == lru2.erase(res.first));

    CASE_EXPECT_EQ(1, lru2.erase(2));
    CASE_EXPECT_EQ(0, lru2.erase(2));

    CASE_EXPECT_TRUE(lru2.empty());
}

CASE_TEST(lru_map_test, erase_range) {
    typedef util::mempool::lru_map<int, long> lru_t;
    lru_t                                     lru;
    lru.reserve(128);

    for (int i = 1; i <= 128; ++i) {
        lru[i] = 100 + i;
    }

    int range_idx = 1;
    for (lru_t::iterator it = lru.begin(); it != lru.end(); ++it) {
        CASE_EXPECT_EQ(range_idx, (*it).first);
        CASE_EXPECT_EQ(range_idx + 100, *(*it).second);

        ++range_idx;
    }

    CASE_EXPECT_EQ(128, lru.size());

    CASE_EXPECT_TRUE(lru.end() == lru.erase(lru.begin(), lru.end()));
    CASE_EXPECT_TRUE(lru.empty());
    CASE_EXPECT_EQ(0, lru.size());
}

#if UTIL_CONFIG_COMPILER_CXX_RVALUE_REFERENCES
CASE_TEST(lru_map_test, emplace) {
    typedef util::mempool::lru_map<int, std::vector<long> > lru_t;
    lru_t                                                   lru;
    typedef std::pair<lru_t::iterator, bool>                insert_pair_t;

    std::vector<long> vec;
    vec.push_back(1001);
    vec.push_back(1002);
    vec.push_back(1003);

    insert_pair_t res = lru.insert(lru_t::value_type(1, std::make_shared<std::vector<long> >(std::move(vec))));
    CASE_EXPECT_TRUE(res.second);

    vec.push_back(1004);
    res = lru.insert(lru_t::value_type(1, std::make_shared<std::vector<long> >(vec)));
    CASE_EXPECT_FALSE(res.second);

    CASE_EXPECT_EQ(3, lru.front().second->size());
}
#endif


CASE_TEST(lru_map_test, lru_reorder) {
    typedef util::mempool::lru_map<int, long> lru_t;
    lru_t                                     lru;

    for (int i = 1; i <= 60; ++i) {
        lru[i] = 100 + i;
    }

    lru_t::iterator iter = lru.find(1);
    CASE_EXPECT_FALSE(iter == lru.end());

    CASE_EXPECT_EQ(2, lru.front().first);
    CASE_EXPECT_EQ(102, *lru.front().second);
    CASE_EXPECT_EQ(1, lru.back().first);
    CASE_EXPECT_EQ(101, *lru.back().second);

    int range_idx = 2;
    for (lru_t::iterator it = lru.begin(); it != lru.end(); ++it) {
        CASE_EXPECT_EQ(range_idx, (*it).first);
        CASE_EXPECT_EQ(range_idx + 100, *(*it).second);

        if (range_idx == 60) {
            range_idx = 1;
        } else {
            ++range_idx;
        }
    }
}
