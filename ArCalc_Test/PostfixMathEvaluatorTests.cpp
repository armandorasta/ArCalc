#include "pch.h"

#include <PostfixMathEvaluator.h>
#include <../../ArCalc/Source/Parser.h>	
#include <Util/MathOperator.h>
#include <Util/IO.h>

#ifndef ARCALC_SOURCE_FILES_H_
#include "ArCalcSourceFiles.h"
#endif

#define EVALUATOR_TEST(_testName) TEST_F(PostfixMathEvaluatorTests, _testName)

using namespace ArCalc;

class PostfixMathEvaluatorTests : public testing::Test {
private:
	inline static FunctionManager s_FunMan{std::cout};

public:
	static LiteralManager LitManFromLits(std::unordered_map<std::string_view, double>&& lits) {
		LiteralManager litMan{std::cout};
		litMan.ToggleOutput();
		for (auto const& [name, value] : lits) {
			litMan.Add(name, value);
		}

		return litMan;
	}

	static PostfixMathEvaluator GenerateTestingInstance(LiteralManager& litMan) {
		if (s_FunMan.IsOutputEnabled()) {
			s_FunMan.ToggleOutput();
		}

		return PostfixMathEvaluator{litMan, s_FunMan};
	}

	static PostfixMathEvaluator GenerateTestingInstance() {
		static LiteralManager s_LitMan{std::cout};
		s_LitMan.ToggleOutput();
		return GenerateTestingInstance(s_LitMan);
	}

	static void TestBothSigns(PostfixMathEvaluator& ev, double value, std::string expr) {
		if (HasFailure()) {
			return;
		}

		ASSERT_NO_THROW(ev.Eval(expr)) << expr;
		ASSERT_DOUBLE_EQ(value, *ev.Eval(expr)) << expr;

		ASSERT_NO_THROW(ev.Eval("-" + expr)) << ("-" + expr);
		ASSERT_DOUBLE_EQ(-value, *ev.Eval("-" + expr)) << ("-" + expr);
	};
};

EVALUATOR_TEST(Normal_number_parsing) {
	auto ev{GenerateTestingInstance()};
	
	TestBothSigns(ev, 5.0, "5");
	TestBothSigns(ev, 5.0, "5.");
	TestBothSigns(ev, 5.0, "5.00000");
	TestBothSigns(ev, 5.0, "000005");
	TestBothSigns(ev, 5.0, "000005.");
	TestBothSigns(ev, 5.0, "000005.000000");

	TestBothSigns(ev, 0.83, ".83");
	TestBothSigns(ev, 0.83, ".8300000");
	TestBothSigns(ev, 0.83, "000000.83");
	TestBothSigns(ev, 0.83, "000000.8300000");

	TestBothSigns(ev, 5.83, "5.83");
	TestBothSigns(ev, 5.83, "5.8300000");
	TestBothSigns(ev, 5.83, "0000005.8300000");

	TestBothSigns(ev, 5432.1, "5432.1");
	TestBothSigns(ev, 5432.1, "5'432.1");
	TestBothSigns(ev, 5432.1, "5'4'3'2.1");
	TestBothSigns(ev, 5432.1, "5'4'3'2.10000");
	TestBothSigns(ev, 5432.1, "5'4'32.1'0'00'0");
	TestBothSigns(ev, 5432.1, "5432.1'0'00'0");

	TestBothSigns(ev, 1e6, "1e6");
	TestBothSigns(ev, 1e6, "1e0006");
	TestBothSigns(ev, 1e6, "00001e0006");
	TestBothSigns(ev, 1e6, "00'00'1e0006");
	TestBothSigns(ev, 1e6, "00'00'1e00'06");
	TestBothSigns(ev, 1.5e6, "1.5e6");
	TestBothSigns(ev, 1.5e6, "1.5e0006");
	TestBothSigns(ev, 1.5e6, "00001.50000e0006");
	TestBothSigns(ev, 1.5e6, "00'00'1.50000e0006");
	TestBothSigns(ev, 1.5e6, "00'00'1.50000e00'06");
	TestBothSigns(ev, 1.5e6, "00'00'1.50'0'00e00'06"); // Man...

	// No multiple floating points
	for (std::string Ones{"11111111"}; auto const i : view::iota(0U, Ones.size())) {
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
	ASSERT_ANY_THROW(ev.Eval("11.1''1"));
	ASSERT_ANY_THROW(ev.Eval("11.1e1''1"));
	
	// No ' at the end.
	ASSERT_ANY_THROW(ev.Eval("1234'"));
	ASSERT_ANY_THROW(ev.Eval("1234.56'"));
	ASSERT_ANY_THROW(ev.Eval("1234.56e78'"));

	// No 'e' at the end.
	ASSERT_ANY_THROW(ev.Eval("1234e"));
	ASSERT_ANY_THROW(ev.Eval("1234.56e"));

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

	LiteralManager litMan{std::cout};
	litMan.ToggleOutput();
	for (auto const n : view::iota(0U, std::size(Names))) {
		litMan.Add(Names[n], n * 1.0);
	}

	auto ev{GenerateTestingInstance(litMan)};
	for (auto const name : Names) {
		ASSERT_DOUBLE_EQ(*litMan.Get(name), *ev.Eval(name));
	}
}

EVALUATOR_TEST(Math_constant_parsing) {
	auto ev{GenerateTestingInstance()};

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


EVALUATOR_TEST(Hex_number_parsing) {
	auto ev{GenerateTestingInstance()};

	constexpr auto myPow = [](auto n, auto p) {
		return n * std::pow(16.0, p);
	};


	// Quick look at support for all digits
	for (auto const i : view::iota(0, 16)) {
		auto expr{std::format(
			"0x{:c}", 
			i < 10 ? ('0' + i) : ('A' + i - 10)
		)};

		ASSERT_NO_THROW(ev.Eval(expr)) << expr;
		ASSERT_TRUE(ev.Eval(expr).has_value()) << expr;
		ASSERT_DOUBLE_EQ(i, *ev.Eval(expr)) << expr;

		expr = '-' + expr;
		ASSERT_NO_THROW(ev.Eval(expr)) << expr;
		ASSERT_TRUE(ev.Eval(expr).has_value()) << expr;
		ASSERT_DOUBLE_EQ(-i, *ev.Eval(expr)) << expr;
	}

	TestBothSigns(ev, 0x12AF, "0x12AF");
	TestBothSigns(ev, 0x12AF, "0x12'AF");
	TestBothSigns(ev, 0x12AF, "0x12AF.");
	TestBothSigns(ev, 0x12AF, "0x12'AF.");
	TestBothSigns(ev, myPow(0x12AF'5, -1), "0x12AF.5");
	TestBothSigns(ev, 0x12AF, "0x000012AF");
	TestBothSigns(ev, 0x12AF, "0x000012AF.");
	TestBothSigns(ev, myPow(0x12AF'05, -2), "0x000012AF.05");
	TestBothSigns(ev, myPow(0x12AF'05, -2), "0x000012AF.050000");
	TestBothSigns(ev, myPow(0x12AF'05, -2), "0x00'001'2AF.050000");
	TestBothSigns(ev, myPow(0x12AF'05, -2), "0x00'001'2AF.0'50'000");

	TestBothSigns(ev, myPow(0x55, -2), "0x0.5'5");
	TestBothSigns(ev, myPow(0x55, -2), "0x0.5500000");
	TestBothSigns(ev, myPow(0x55, -2), "0x00000.55");
	TestBothSigns(ev, myPow(0x55, -2), "0x00000.5500000");

	// No multiple floating points
	for (std::string Fs{"0xFFFFFF"}; auto const i : view::iota(2U, Fs.size())) {
		Fs[i] = '.';
		for (auto const j : view::iota(i + 1, Fs.size())) {
			Fs[j] = '.';

			ASSERT_ANY_THROW(ev.Eval(Fs));
			ev.Reset(); // Throwing leaves the evaluator in an undefined state.

			Fs[j] = 'F';
		}
		Fs[i] = 'F';
	}

	// No ' right after the .
	ASSERT_ANY_THROW(ev.Eval("0xF.'1"));
	ev.Reset(); // Throwing leaves the evaluator in an undefined state.

	// No ' just before the .
	ASSERT_ANY_THROW(ev.Eval("0xC'.1"));
	ev.Reset(); // Throwing leaves the evaluator in an undefined state.

	// No two ' in a row.
	ASSERT_ANY_THROW(ev.Eval("0xD''1"));
	ASSERT_ANY_THROW(ev.Eval("0xD1.A''3"));

	// No ' at the end.
	ASSERT_ANY_THROW(ev.Eval("0x12B4'"));
	ASSERT_ANY_THROW(ev.Eval("0x12B4.1F'"));
}

EVALUATOR_TEST(Binary_number_parsing) {
	auto ev{GenerateTestingInstance()};

	constexpr auto myPow = [](auto n, auto p) {
		return n * std::pow(2.0, p);
	};

	TestBothSigns(ev, 0b1101, "0b1101");
	TestBothSigns(ev, 0b1101, "0b00001101");
	TestBothSigns(ev, 0b1101, "0b00'001'101");
	TestBothSigns(ev, myPow(0b1101'011, -3), "0b00'001'101.011");
	TestBothSigns(ev, myPow(0b1101'011, -3), "0b00'001'101.0110000");
	TestBothSigns(ev, myPow(0b1101'011, -3), "0b00'001'101.01'100'00");

	// No multiple floating points
	for (std::string Ones{"0b11111111"}; auto const i : view::iota(2U, Ones.size())) {
		Ones[i] = '.';
		for (auto const j : view::iota(i + 1, Ones.size())) {
			Ones[j] = '.';

			ASSERT_ANY_THROW(ev.Eval(Ones));
			ev.Reset(); // Throwing leaves the evaluator in an undefined state.

			Ones[j] = '1';
		}
		Ones[i] = '1';
	}


	// No ' right after the .
	ASSERT_ANY_THROW(ev.Eval("0b1.'1"));
	ev.Reset(); // Throwing leaves the evaluator in an undefined state.

	// No ' just before the .
	ASSERT_ANY_THROW(ev.Eval("0b1'.1"));
	ev.Reset(); // Throwing leaves the evaluator in an undefined state.

	// No two ' in a row.
	ASSERT_ANY_THROW(ev.Eval("0b1''1"));
	ASSERT_ANY_THROW(ev.Eval("0b11.1''1"));

	// No ' at the end.
	ASSERT_ANY_THROW(ev.Eval("0b1101'"));
	ASSERT_ANY_THROW(ev.Eval("0b1101.011'"));
}

EVALUATOR_TEST(Octal_number_parsing) {
	auto ev{GenerateTestingInstance()};

	constexpr auto myPow = [](auto n, auto p) {
		return n * std::pow(8.0, p);
	};

	TestBothSigns(ev, 05672, "0o5672");
	TestBothSigns(ev, 05672, "0o00005672");
	TestBothSigns(ev, 05672, "0o00'005'67'2");
	//                      vvvvvvvvvv contains all octal digits.
	TestBothSigns(ev, myPow(05672'0431, -4), "0o00'005'67'2.0431");
	TestBothSigns(ev, myPow(05672'0431, -4), "0o00'005'67'2.04310000");
	TestBothSigns(ev, myPow(05672'0431, -4), "0o00'005'67'2.043'100'00");

	// No multiple floating points
	for (std::string sevens{"0o77777777"}; auto const i : view::iota(2U, sevens.size())) {
		sevens[i] = '.';
		for (auto const j : view::iota(i + 1, sevens.size())) {
			sevens[j] = '.';

			ASSERT_ANY_THROW(ev.Eval(sevens));
			ev.Reset(); // Throwing leaves the evaluator in an undefined state.

			sevens[j] = '7';
		}
		sevens[i] = '7';
	}


	// No ' right after the .
	ASSERT_ANY_THROW(ev.Eval("0o1.'1"));
	ev.Reset(); // Throwing leaves the evaluator in an undefined state.

	// No ' just before the .
	ASSERT_ANY_THROW(ev.Eval("0o1'.1"));
	ev.Reset(); // Throwing leaves the evaluator in an undefined state.

	// No two ' in a row.
	ASSERT_ANY_THROW(ev.Eval("0o1''1"));
	ASSERT_ANY_THROW(ev.Eval("0o11.1''1"));

	// No ' at the end.
	ASSERT_ANY_THROW(ev.Eval("0o1101'"));
	ASSERT_ANY_THROW(ev.Eval("0o1101.011'"));
}