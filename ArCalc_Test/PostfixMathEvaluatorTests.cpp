#include "pch.h"

#include <PostfixMathEvaluator.h>
#include <../../ArCalc/Source/Parser.h>	
#include <Util/MathOperator.h>
#include <Util/IO.h>

#define EVALUATOR_TEST(_testName) TEST_F(PostfixMathEvaluatorTests, _testName)

using namespace ArCalc;

class PostfixMathEvaluatorTests : public testing::Test {
private:
	inline static std::stringstream s_BullshitSS{};
	inline static FunctionManager s_FunMan{s_BullshitSS};

public:
	static std::stringstream& GetStringStream() {
		return s_BullshitSS;
	}

public:
	static LiteralManager LitManFromLits(std::unordered_map<std::string_view, double>&& lits) {
		LiteralManager litMan{GetStringStream()};
		for (auto const& [name, value] : lits) {
			litMan.Add(name, value);
		}

		return litMan;
	}

	static PostfixMathEvaluator GenerateTestingInstance(LiteralManager& litMan) {
		return PostfixMathEvaluator{litMan, s_FunMan};
	}
};

EVALUATOR_TEST(Number_parsing) {
	auto litMan{LitManFromLits({})};
	auto ev{GenerateTestingInstance(litMan)};
	
	auto const testBothSigns = [&](double value, std::string expr) {
		if (HasFailure()) {
			return;
		}

		ASSERT_DOUBLE_EQ(value, *ev.Eval(expr)) << expr;
		ASSERT_DOUBLE_EQ(-value, *ev.Eval("-" + expr)) << ("-" + expr);
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

	testBothSigns(5432.1, "5432.1");
	testBothSigns(5432.1, "5'432.1");
	testBothSigns(5432.1, "5'4'3'2.1");
	testBothSigns(5432.1, "5'4'3'2.10000");
	testBothSigns(5432.1, "5'4'32.1'0'00'0");
	testBothSigns(5432.1, "5432.1'0'00'0");

	testBothSigns(1e6, "1e6");
	testBothSigns(1e6, "1e0006");
	testBothSigns(1e6, "00001e0006");
	testBothSigns(1e6, "00'00'1e0006");
	testBothSigns(1e6, "00'00'1e00'06");
	testBothSigns(1.5e6, "1.5e6");
	testBothSigns(1.5e6, "1.5e0006");
	testBothSigns(1.5e6, "00001.50000e0006");
	testBothSigns(1.5e6, "00'00'1.50000e0006");
	testBothSigns(1.5e6, "00'00'1.50000e00'06");
	testBothSigns(1.5e6, "00'00'1.50'0'00e00'06"); // Man...

	// No multiple floating points
	for (std::string Ones{"11111111"}; auto const i : view::iota(1U, Ones.size())) {
		Ones[i] = '.';
		for (auto const j : view::iota(i + 1, Ones.size())) {
			Ones[j] = '.';

			ASSERT_ANY_THROW(ev.Eval(Ones));
			ev.Reset(); // Throwing leaves the evaluator in an undefined state.

			Ones[j] = '1';
		}
		Ones[i] = '1';
	}

	// No multiple e's
	for (std::string Ones{"11111111"}; auto const i : view::iota(1U, Ones.size())) {
		// I could combine this loop with the last one, but that will just make the testing
		// process harder.

		Ones[i] = 'e';
		for (auto const j : view::iota(i + 1, Ones.size())) {
			Ones[j] = 'e';

			ASSERT_ANY_THROW(ev.Eval(Ones));
			ev.Reset(); // Throwing leaves the evaluator in an undefined state.

			Ones[j] = '1';
		}
		Ones[i] = '1';
	}

	// No ' right after the .
	ASSERT_ANY_THROW(ev.Eval("1.'1"));
	ev.Reset(); // Throwing leaves the evaluator in an undefined state.

	// No ' just before the .
	ASSERT_ANY_THROW(ev.Eval("1'.1"));
	ev.Reset(); // Throwing leaves the evaluator in an undefined state.

	// No two ' in a row.
	ASSERT_ANY_THROW(ev.Eval("1''1"));
	
	// No ' at the end.
	ASSERT_ANY_THROW(ev.Eval("1234'"));

	// No 'e' at the end.
	ASSERT_ANY_THROW(ev.Eval("1234e"));

	// No ' just before or after 'e'.
	ASSERT_ANY_THROW(ev.Eval("1234'e5678"));
	ASSERT_ANY_THROW(ev.Eval("1234e'5678"));
	ASSERT_ANY_THROW(ev.Eval("1234'e'5678"));

	// No floating points in exponent.
	ASSERT_ANY_THROW(ev.Eval("1234e45.6"));
	ASSERT_ANY_THROW(ev.Eval("1234e.456"));
	ASSERT_ANY_THROW(ev.Eval("1234e456."));
}

EVALUATOR_TEST(Identifier_parsing) {
	constexpr std::array Names{
		"some", // Alphabetic characters
		"_some", "some_", "_some_", "_so_me_", // _
		"some1", "so1me", // Numbers
	};

	LiteralManager litMan{GetStringStream()};
	for (auto const n : view::iota(0U, std::size(Names))) {
		litMan.Add(Names[n], n * 1.0);
	}

	auto ev{GenerateTestingInstance(litMan)};
	for (auto const name : Names) {
		ASSERT_DOUBLE_EQ(*litMan.Get(name), *ev.Eval(name));
	}
}

EVALUATOR_TEST(Math_constant_parsing) {
	auto litMan{LitManFromLits({})};
	auto ev{GenerateTestingInstance(litMan)};

	ASSERT_DOUBLE_EQ(std::numbers::pi, *ev.Eval("_pi"));
	ASSERT_DOUBLE_EQ(-(2 * std::numbers::pi), *ev.Eval("_pi 2 * negate"));
}

EVALUATOR_TEST(Three_operands) {
	constexpr auto A{1.0};
	constexpr auto B{-5.0};
	constexpr auto C{10.0};

	auto litMan{LitManFromLits({{"a", A}, {"b", B}, {"c", C}})};
	auto ev{GenerateTestingInstance(litMan)};

	ASSERT_DOUBLE_EQ(A + B + C, *ev.Eval("a b c + +"));
	ASSERT_DOUBLE_EQ(A + B + C, *ev.Eval("a b + c +"));
	ASSERT_DOUBLE_EQ(A * B + C, *ev.Eval("a b * c +"));
	ASSERT_DOUBLE_EQ(A + B * C, *ev.Eval("b c * a +"));

	ASSERT_DOUBLE_EQ((A + B) * C, *ev.Eval("a b + c *"));
	ASSERT_DOUBLE_EQ(C * (A + B), *ev.Eval("c a b + *"));
}

EVALUATOR_TEST(Unary_operators) {
	constexpr auto A{1.0};
	constexpr auto B{-5.0};
	constexpr auto C{10.0};

	auto litMan{LitManFromLits({{"a", A}, {"b", B}, {"c", C}})};
	auto ev{GenerateTestingInstance(litMan)};

	ASSERT_DOUBLE_EQ(-A, *ev.Eval("a negate"));
	ASSERT_DOUBLE_EQ(std::abs(A + B), *ev.Eval("a b + abs"));
	ASSERT_DOUBLE_EQ(std::sqrt(std::abs(A + B)), *ev.Eval("a b + abs sqrt"));
	ASSERT_DOUBLE_EQ(std::abs(A + B * C), *ev.Eval("a b c * + abs"));
	ASSERT_DOUBLE_EQ(std::abs(A + B) * C, *ev.Eval("a b + abs c *"));
	ASSERT_DOUBLE_EQ(std::sqrt(std::abs(A + B * C)), *ev.Eval("a b c * + abs sqrt"));
	ASSERT_DOUBLE_EQ(std::sqrt(A*A + B*B + C*C), *ev.Eval("a a * b b * c c * + + sqrt"));
}