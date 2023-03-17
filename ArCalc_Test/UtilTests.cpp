#include "pch.h"

#include <Util/Util.h>
#include <Util/Random.h>

#include "ArCalcSourceFiles.h"

#define UTIL_TEST(_testName) TEST_F(UtilTests, _testName)

using namespace ArCalc;

class UtilTests : public testing::Test {
};

UTIL_TEST(Eq) {
	constexpr std::int32_t Min{-50};
	constexpr std::int32_t Max{+50};

	for (auto const currLhs : view::iota(Min, Max)) {
		auto const func{Util::Eq(currLhs)};
		for (auto const currRhs : view::iota(Min, Max)) {
			ASSERT_TRUE(currLhs == currRhs ? func(currRhs) : !func(currRhs));
		}
	}
}

UTIL_TEST(Negate_with_binary_operators) {
	static std::vector<std::function<bool(int32_t, int32_t)>> const sc_Funcs{
		std::greater<int32_t>{},
		std::less<int32_t>{},
		std::equal_to<int32_t>{},
		std::not_equal_to<int32_t>{},
	};

	for (auto const lhs : view::iota(-10I32, 10)) {
		constexpr decltype(lhs) rhs{0};

		for (auto& func : sc_Funcs) {
			ASSERT_EQ(func(lhs, rhs), !Util::Negate(func)(lhs, rhs));
		}
	}
}

UTIL_TEST(Negate_with_unary_operators) {
	static std::vector<std::function<bool(int32_t)>> const sc_Funcs{
		[](int32_t n) { return n == 5; },
		[](int32_t n) { return n >  5; },
		[](int32_t n) { return n >= 5; },
		[](int32_t n) { return n <  5; },
		[](int32_t n) { return n <= 5; },
		[](int32_t n) { return n != 5; },
	};

	for (auto const operand : view::iota(-10I32, 10)) {
		for (auto& func : sc_Funcs) {
			ASSERT_EQ(func(operand), !Util::Negate(func)(operand));
		}
	}
}

UTIL_TEST(Intersect) {
	static std::vector<std::function<bool(int32_t)>> const sc_Funcs{
		[](int32_t n) { return n == 5; },
		[](int32_t n) { return n >  5; },
		[](int32_t n) { return n >= 5; },
		[](int32_t n) { return n <  5; },
		[](int32_t n) { return n <= 5; },
		[](int32_t n) { return n != 5; },
	};

	for (auto const operand : view::iota(-10I32, 10)) {
		for (auto& func0 : sc_Funcs) {
			for (auto& func1 : sc_Funcs) {
				ASSERT_EQ(func0(operand) && func1(operand), Util::Intersect(func0, func1)(operand));
			}
		}
	}
}

UTIL_TEST(Union) {
	static std::vector<std::function<bool(int32_t)>> const sc_Funcs{
		[](int32_t n) { return n == 5; },
		[](int32_t n) { return n >  5; },
		[](int32_t n) { return n >= 5; },
		[](int32_t n) { return n <  5; },
		[](int32_t n) { return n <= 5; },
		[](int32_t n) { return n != 5; },
	};

	for (auto const operand : view::iota(-10I32, 10)) {
		for (auto& func0 : sc_Funcs) {
			for (auto& func1 : sc_Funcs) {
				ASSERT_EQ(func0(operand) || func1(operand), Util::Union(func0, func1)(operand));
			}
		}
	}
}
