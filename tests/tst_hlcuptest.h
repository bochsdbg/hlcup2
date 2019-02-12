#pragma once

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "../ParseUtils.hpp"

using namespace testing;

TEST(ParseUtilsTest, HexToIntTest) {
    EXPECT_EQ(3, hlcup::hex_to_int('3'));
    EXPECT_EQ(8, hlcup::hex_to_int('8'));
    EXPECT_EQ(10, hlcup::hex_to_int('A'));
    EXPECT_EQ(10, hlcup::hex_to_int('a'));
    EXPECT_EQ(15, hlcup::hex_to_int('f'));
    EXPECT_EQ(14, hlcup::hex_to_int('E'));
}
