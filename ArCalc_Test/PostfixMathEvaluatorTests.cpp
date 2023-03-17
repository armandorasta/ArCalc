#include "pch.h"

#include <PostfixMathEvaluator.h>
#include <Util/MathOperator.h>
#include <Util/IO.h>

#define EVALUATOR_TEST(_testName) TEST_F(PostfixMathEvaluatorTests, _testName)

using namespace ArCalc;

class PostfixMathEvaluatorTests : public testing::Test {
public:
	void SetUp() override {
		if (!MathOperator::IsInitialized()) {
			MathOperator::Initialize();
		}
	}
};

EVALUATOR_TEST(Number_parsing) {
	auto const testBothSigns = [&](double value, std::string expr) {
		static PostfixMathEvaluator ev{{}};

		ASSERT_DOUBLE_EQ(value, ev.Eval(expr));
		ASSERT_DOUBLE_EQ(-value, ev.Eval("-" + expr));
	};

	testBothSigns(5.0, "5");
	testBothSigns(5.0, "5.");
	testBothSigns(5.0, "5.00000");
	testBothSigns(5.0, "000005");
	testBothSigns(5.0, "000005.");
	testBothSigns(5.0, "000005.000000");

	testBothSigns(0.83, ".83");
	testBothSigns(0.83, ".8300000");
	testBothSigns(0.83, "000000.83");
	testBothSigns(0.83, "000000.8300000");

	testBothSigns(5.83, "5.83");
	testBothSigns(5.83, "5.8300000");
	testBothSigns(5.83, "0000005.8300000");
}

EVALUATOR_TEST(Identifier_parsing) {
	constexpr std::array Names{
		"some", // Alphabetic characters
		"_some", "some_", "_some_", "_so_me_", // _
		"some1", "so1me", // Numbers
	};

	PostfixMathEvaluator::VarMap lits{};
	for (auto const n : view::iota(0U, std::size(Names))) {
		lits.emplace(Names[n], n * 1.0);
	}
	static PostfixMathEvaluator ev{lits};

	for (auto const name : Names) {
		ASSERT_DOUBLE_EQ(lits.at(name), ev.Eval(name));
	}
}

EVALUATOR_TEST(Math_constant_parsing) {
	PostfixMathEvaluator ev{{}};

	ASSERT_DOUBLE_EQ(std::numbers::pi, ev.Eval("pi"));
	ASSERT_DOUBLE_EQ(-(2 * std::numbers::pi), ev.Eval("pi 2 * negate"));
}

EVALUATOR_TEST(Three_operands) {
	constexpr auto A{1.0};
	constexpr auto B{-5.0};
	constexpr auto C{10.0};

	PostfixMathEvaluator ev{{
		{"a", A},
		{"b", B},
		{"c", C},
	}};

	ASSERT_DOUBLE_EQ(A + B + C, ev.Eval("a b c + +"));
	ASSERT_DOUBLE_EQ(A + B + C, ev.Eval("a b + c +"));
	ASSERT_DOUBLE_EQ(A * B + C, ev.Eval("a b * c +"));
	ASSERT_DOUBLE_EQ(A + B * C, ev.Eval("b c * a +"));

	ASSERT_DOUBLE_EQ((A + B) * C, ev.Eval("a b + c *"));
	ASSERT_DOUBLE_EQ(C * (A + B), ev.Eval("c a b + *"));
}

EVALUATOR_TEST(Unary_operators) {
	constexpr auto A{1.0};
	constexpr auto B{-5.0};
	constexpr auto C{10.0};

	PostfixMathEvaluator ev{{
		{"a", A},
		{"b", B},
		{"c", C},
	}};

	// Unary operators
	ASSERT_DOUBLE_EQ(-A, ev.Eval("a negate"));
	ASSERT_DOUBLE_EQ(std::abs(A + B), ev.Eval("a b + abs"));
	ASSERT_DOUBLE_EQ(std::sqrt(std::abs(A + B)), ev.Eval("a b + abs sqrt"));
	ASSERT_DOUBLE_EQ(std::abs(A + B * C), ev.Eval("a b c * + abs"));
	ASSERT_DOUBLE_EQ(std::abs(A + B) * C, ev.Eval("a b + abs c *"));
	ASSERT_DOUBLE_EQ(std::sqrt(std::abs(A + B * C)), ev.Eval("a b c * + abs sqrt"));
	ASSERT_DOUBLE_EQ(std::sqrt(A*A + B*B + C*C), ev.Eval("a a * b b * c c * + + sqrt"));
}