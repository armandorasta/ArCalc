#include "pch.h"

#include <Util/MathOperator.h>

#define MATHOP_TEST(_testName) TEST_F(MathOperatorTests, _testName)

using namespace ArCalc;

class MathOperatorTests : public ::testing::Test {
public:
	inline static std::vector<char const*> const sc_MyOperators{
		"+", "-", "*", "/",
		"sin", "cos", "tan", "abs", "cosh",
		">", ">=", "<", "<=", "==", "!=",
	};
};

MATHOP_TEST(IsValid) {
	for (auto const op : sc_MyOperators) {
		ASSERT_TRUE(MathOperator::IsValid(op)) << "Operator was: " << op;
	}
}

MATHOP_TEST(IsUnary_and_IsBinary) {
	constexpr auto A{5.0};
	constexpr auto B{-8.0};

	for (auto const op : sc_MyOperators) {
		ASSERT_TRUE(MathOperator::IsValid(op));
		// All operators m_By the time of this writing are unary or binary.
		EXPECT_TRUE(MathOperator::IsBinary(op) || MathOperator::IsUnary(op));

		if (MathOperator::IsUnary(op)) {
			ASSERT_NO_THROW(MathOperator::EvalUnary(op, A)) 
				<< "Operator: " << op << ", operand: " << A;
			ASSERT_NO_THROW(MathOperator::EvalUnary(op, B)) 
				<< "Operator: " << op << ", operand: " << B;
		} else if (MathOperator::IsBinary(op)) {
			ASSERT_NO_THROW(MathOperator::EvalBinary(op, A, B))
				<< "Operator: " << op << ", operands: " << A << ", " << B;
			ASSERT_NO_THROW(MathOperator::EvalBinary(op, B, A))
				<< "Operator: " << op << ", operands: " << B << ", " << A;
		}
	}
}

MATHOP_TEST(EvalBinary) {
	constexpr auto A{5.0};
	constexpr auto B{-8.0};

	auto const test = [=](std::string_view glyph, auto&& op) {
		ASSERT_DOUBLE_EQ(op(A, B), MathOperator::EvalBinary(glyph, A, B));
		ASSERT_DOUBLE_EQ(op(B, A), MathOperator::EvalBinary(glyph, B, A));
	};

	test("+", std::plus<>{});
	test("-", std::minus<>{});
	test("*", std::multiplies<>{});
	test("/", [](double l, double r) { return l / r; });
	test("max", [](double l, double r) { return std::max(l, r); });

#ifndef NDEBUG
	// Evaluating unary function using EvalBinary
	ASSERT_ANY_THROW(MathOperator::EvalBinary("sin", A, B));
#endif // ^^^^ Debug mode only.
}

MATHOP_TEST(EvalUnary) {
	constexpr auto A{5.0};
	constexpr auto B{-8.0};

	auto const test = [=](std::string_view glyph, auto&& op) {
		ASSERT_DOUBLE_EQ(op(A), MathOperator::EvalUnary(glyph, A));
		ASSERT_DOUBLE_EQ(op(B), MathOperator::EvalUnary(glyph, B));
	};

	test("sin",    [](double o) { return std::sin(o); });
	test("negate", [](double o) { return -o; });
	test("abs",    [](double o) { return std::abs(o); });

#ifndef NDEBUG
	// Evaluating unary function using EvalBinary
	ASSERT_ANY_THROW(MathOperator::EvalUnary("+", A));
#endif // ^^^^ Debug mode only.
}