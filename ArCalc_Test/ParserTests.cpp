#include "pch.h"

#include <../../ArCalc/Source/Parser.h>
#include "Util/Str.h"
#include <Util/IO.h>
#include <Util/Random.h>
#include <Util/MathConstant.h>

#include "TestingHelperMacros.h"

#define PARSER_TEST(_testName) TEST_F(ParserTests, _testName)

using namespace ArCalc;

class ParserTests : public testing::Test {
public:
	Parser GenerateTestingInstance() {
		Parser par{std::cout};
		par.ToggleOutput();
		return par;
	}
};

PARSER_TEST(Semicolon_at_end_of_line) {
	// Verbose, but easy to debug.

	/* Without */ {
		auto par{GenerateTestingInstance()};
		ASSERT_NO_THROW(par.ParseLine("5 10 +"));
		ASSERT_FALSE(par.IsLineEndsWithSemiColon());
		ASSERT_NO_THROW(par.ParseLine("_List"));
		ASSERT_FALSE(par.IsLineEndsWithSemiColon());
		ASSERT_NO_THROW(par.ParseLine("_Func Max a b"));
		ASSERT_FALSE(par.IsLineEndsWithSemiColon());
		ASSERT_NO_THROW(par.ParseLine("_Set ret a"));
		ASSERT_FALSE(par.IsLineEndsWithSemiColon());
		ASSERT_NO_THROW(par.ParseLine("_If a b <: _Set ret b"));
		ASSERT_FALSE(par.IsLineEndsWithSemiColon());
		ASSERT_NO_THROW(par.ParseLine("_Return ret"));
		ASSERT_FALSE(par.IsLineEndsWithSemiColon());
	}

	/* With ; */ {
		auto par{GenerateTestingInstance()};
		ASSERT_NO_THROW(par.ParseLine("5 10 +;"));
		ASSERT_TRUE(par.IsLineEndsWithSemiColon());
		ASSERT_NO_THROW(par.ParseLine("_List;"));
		ASSERT_TRUE(par.IsLineEndsWithSemiColon());
		ASSERT_NO_THROW(par.ParseLine("_Func Max a b;"));
		ASSERT_TRUE(par.IsLineEndsWithSemiColon());
		ASSERT_NO_THROW(par.ParseLine("_Set ret a;"));
		ASSERT_TRUE(par.IsLineEndsWithSemiColon());
		ASSERT_NO_THROW(par.ParseLine("_If a b <: _Set ret b;"));
		ASSERT_TRUE(par.IsLineEndsWithSemiColon());
		ASSERT_NO_THROW(par.ParseLine("_Return ret;"));
		ASSERT_TRUE(par.IsLineEndsWithSemiColon());
	}
}

PARSER_TEST(Normal_expression_and_Last_keyword) {
	auto par{GenerateTestingInstance()};

	par.ParseLine("5 10 +");
	ASSERT_DOUBLE_EQ(15.0, par.GetLitMan().GetLast());
}

PARSER_TEST(Set_statement_and_literal_name_clashes) {
	auto par{GenerateTestingInstance()};
	auto& litMan{par.GetLitMan()};

	par.ParseLine("_Set x 5;");
	ASSERT_TRUE(litMan.IsVisible("x"));
	ASSERT_DOUBLE_EQ(5.0, *litMan.Get("x"));
}

PARSER_TEST(Name_collisions) {
	auto par{GenerateTestingInstance()};

	// Already defined functions may not be overriden.
	par.ParseLine("_Func MyFunc param");
	par.ParseLine("_Return;");
	ASSERT_ANY_THROW(par.ParseLine("_Set MyFunc 5.0;"));

	// ...Unless unscoped.
	par.ParseLine("_Unscope MyFunc");
	ASSERT_NO_THROW(par.ParseLine("_Set MyFunc 5.0;"));
}

PARSER_TEST(Shadowing_constants_with_literals) {
	auto par{GenerateTestingInstance()};

	ASSERT_NO_THROW(par.ParseLine("_Set _pi 5.0;"));
	ASSERT_TRUE(par.GetLitMan().IsVisible("_pi"));
	ASSERT_DOUBLE_EQ(5.0, *par.GetLitMan().Get("_pi"));

	// Unscoping makes the constant visible again.
	ASSERT_NO_THROW(par.ParseLine("_Unscope _pi;"));
	par.ParseLine("_pi");
	ASSERT_DOUBLE_EQ(MathConstant::ValueOf("_pi"), par.GetLitMan().GetLast());
}

PARSER_TEST(Shadowing_constants_with_functions) {
	auto par{GenerateTestingInstance()};

	ASSERT_NO_THROW(par.ParseLine("_Func _pi n")); // Will never happen tho...
	ASSERT_NO_THROW(par.ParseLine("_Return n 2 *;"));
	ASSERT_TRUE(par.GetFunMan().IsDefined("_pi"));
	ASSERT_NO_THROW(par.ParseLine("5 _pi;")) << "_pi should've been treated like a function";
	ASSERT_DOUBLE_EQ(10.0, par.GetLitMan().GetLast());

	// Unscoping makes the constant visible again.
	ASSERT_NO_THROW(par.ParseLine("_Unscope _pi;"));
	par.ParseLine("_pi");
	ASSERT_DOUBLE_EQ(MathConstant::ValueOf("_pi"), par.GetLitMan().GetLast());
}

PARSER_TEST(Shadowing_operators_with_literals) {
	auto par{GenerateTestingInstance()};

	ASSERT_NO_THROW(par.ParseLine("_Set abs 5.0;"));
	ASSERT_TRUE(par.GetLitMan().IsVisible("abs"));
	par.ParseLine("abs;");
	ASSERT_DOUBLE_EQ(5.0, par.GetLitMan().GetLast());

	// Unscoping makes the operator usable again.
	ASSERT_NO_THROW(par.ParseLine("_Unscope abs;"));
	par.ParseLine("-5.0 abs;");
	ASSERT_DOUBLE_EQ(std::abs(5.0), par.GetLitMan().GetLast());
}

PARSER_TEST(Shadowing_operators_with_functions) {
	auto par{GenerateTestingInstance()};

	ASSERT_NO_THROW(par.ParseLine("_Func abs n")); // Will never happen tho...
	ASSERT_NO_THROW(par.ParseLine("_Return n 2 *;"));
	ASSERT_TRUE(par.GetFunMan().IsDefined("abs"));
	par.ParseLine("5 abs;");
	ASSERT_DOUBLE_EQ(10.0, par.GetLitMan().GetLast());

	// Unscoping makes the operator usable again.
	ASSERT_NO_THROW(par.ParseLine("_Unscope abs;"));
	par.ParseLine("-5 abs");
	ASSERT_DOUBLE_EQ(5.0, par.GetLitMan().GetLast());
}

PARSER_TEST(Set_statement_setting_a_literal_more_than_once) {
	constexpr auto VarName{"myVar"};
	auto par{GenerateTestingInstance()};

	par.ParseLine(std::format("_Set {} 0;", VarName));
	for (auto const i : view::iota(-10, 10)) {
		ASSERT_NO_THROW(par.ParseLine(std::format("_Set {} {};", VarName, i)));
		ASSERT_DOUBLE_EQ(i, *par.GetLitMan().Get(VarName));
	}
}

PARSER_TEST(Func_keyword_edge_cases) {
	/* Parameterless functions are now allowed */ {
		auto par{GenerateTestingInstance()};
		ASSERT_NO_THROW(par.ParseLine("_Func Parameterless;"));
	}

	/* No name, no parameters */ {
		auto par{GenerateTestingInstance()};
		ASSERT_THROW(par.ParseLine("_Func"), SyntaxError);
		// Make sure the state is reset properly.
		ASSERT_NO_THROW(par.ParseLine("_Func Parameterless;"));
		par.ParseLine("_Unscope");
	}

	/* Invalid name */ {
		auto par{GenerateTestingInstance()};
		ASSERT_THROW(par.ParseLine("_Func 123"), ParseError);
		// Make sure the state is reset properly.
		ASSERT_NO_THROW(par.ParseLine("_Func Parameterless;"));
	}


	/* Invalid parameter name */ {
		auto par{GenerateTestingInstance()};
		ASSERT_THROW(par.ParseLine("_Func MyFunc 123"), ParseError);
		// Make sure the state is reset properly.
		ASSERT_NO_THROW(par.ParseLine("_Func Parameterless;"));
	}

	/* Doublicate parameter name */ {
		auto par{GenerateTestingInstance()};
		ASSERT_THROW(par.ParseLine("_Func MyFunc param param"), SyntaxError);
		// Make sure the state is reset properly.
		ASSERT_NO_THROW(par.ParseLine("_Func Parameterless;"));
	}

	/* Parameter name same as the function */ {
		auto par{GenerateTestingInstance()};
		ASSERT_THROW(par.ParseLine("_Func MyFunc MyFunc"), SyntaxError);
		// Make sure the state is reset properly.
		ASSERT_NO_THROW(par.ParseLine("_Func Parameterless;"));
	}
}

PARSER_TEST(Defining_a_function_with_no_refs) {
	Parser par{GenerateTestingInstance()};

	ASSERT_FALSE(par.IsParsingFunction());
	
	ASSERT_NO_THROW(par.ParseLine("_Func Add a b"));
	ASSERT_TRUE(par.IsParsingFunction());

	ASSERT_NO_THROW(par.ParseLine("_Set sum a b +;"));
	ASSERT_TRUE(par.IsParsingFunction());

	ASSERT_NO_THROW(par.ParseLine("_Return sum;"));
	ASSERT_FALSE(par.IsParsingFunction());

	ASSERT_TRUE(par.GetFunMan().IsDefined("Add"));
}

PARSER_TEST(Defining_a_function_with_refs) {
	constexpr auto FuncName{"SetMultiple"};

	std::vector<std::string> paramNames{};
	for (auto const i : view::iota(0U, 4U)) {
		paramNames.push_back(std::format("param{}", i));
	}

	std::vector<std::string> const lines{
		std::format("_Func {} {} &{} &{} &{};", 
		FuncName, paramNames.front(), paramNames[1], paramNames[2], paramNames[3]),
		std::format("    _Set {} {};", paramNames[1], paramNames.front()),
		std::format("    _Set {} {};", paramNames[2], paramNames.front()),
		std::format("    _Set {} {};", paramNames[3], paramNames.front()),
		"    _Return;",
	};

	Parser par{GenerateTestingInstance()};
	ASSERT_FALSE(par.IsParsingFunction());
	for (auto const& line : lines | view::take(std::size(lines) - 1)) {
		ASSERT_NO_THROW(par.ParseLine(line));
		ASSERT_TRUE(par.IsParsingFunction());
	}

	ASSERT_NO_THROW(par.ParseLine(lines.back()));
	ASSERT_FALSE(par.IsParsingFunction());
	ASSERT_TRUE(par.GetFunMan().IsDefined("SetMultiple"));

	auto const& func{par.GetFunMan().Get(FuncName)};
	for (auto const i : view::iota(0U, paramNames.size())) {
		ASSERT_EQ(paramNames[i], func.Params[i].GetName());
	}

	for (auto const& param : func.Params | view::drop(1U)) {
		ASSERT_TRUE(param.IsPassedByRef());
	}
}

PARSER_TEST(Executing_a_function_with_no_refs) {
	constexpr std::string_view FuncName{"Sub"};

	Parser par{GenerateTestingInstance()};
	par.ParseLine(std::format("_Func {} a b\n", FuncName));
	par.ParseLine("_Set res a b -\n");
	par.ParseLine("_Return res\n");

	for (auto const lhs : view::iota(-10, 10)) {
		auto const rhs{std::ceil(Random::Double(-10.0, 10.0))};
		auto& litMan{par.GetLitMan()};

		// Here we are just regular numbers as arguments.
		ASSERT_NO_THROW(par.ParseLine(std::format("{} {} {}", lhs, rhs, FuncName)));
		ASSERT_DOUBLE_EQ(lhs - rhs, litMan.GetLast());

		// Here we are using variables as arguments
		ASSERT_NO_THROW(par.ParseLine(std::format("_Set lhs {}", lhs)));
		ASSERT_NO_THROW(par.ParseLine(std::format("_Set rhs {}", rhs)));
		ASSERT_NO_THROW(par.ParseLine(std::format("{} {} {}", lhs, rhs, FuncName)));
		ASSERT_DOUBLE_EQ(lhs - rhs, litMan.GetLast());
	}
}

PARSER_TEST(Executing_a_function_with_refs) {
	constexpr std::array Lines{
		"_Func SetMultiple value &a &b",
		"    _Set a value;",
		"    _Set b value;",
		"    _Return;"
	};

	auto par{GenerateTestingInstance()};
	for (auto const line : Lines) {
		par.ParseLine(line);
	}

	par.ParseLine("_Set a 0");
	par.ParseLine("_Set b 0");
	ASSERT_NO_THROW(par.ParseLine("5 a b SetMultiple"));

	auto const& litMan{par.GetLitMan()};
	ASSERT_DOUBLE_EQ(5.0, *litMan.Get("a"));
	ASSERT_DOUBLE_EQ(5.0, *litMan.Get("b"));
}

PARSER_TEST(Nested_function_calls_no_refs) {
	auto par{GenerateTestingInstance()};

	par.ParseLine("_Func Identity n;");
	par.ParseLine("    _Return n;");

	par.ParseLine("_Func AddOne n;");
	ASSERT_NO_THROW(par.ParseLine("    _Return n Identity 1 +;"));

	ASSERT_NO_THROW(par.ParseLine("_Func AddOneThenDouble n;"));
	ASSERT_NO_THROW(par.ParseLine("    _Return n AddOne Identity 2 *;"));

	ASSERT_NO_THROW(par.ParseLine("5 AddOneThenDouble;"));
	ASSERT_DOUBLE_EQ((5.0 + 1.0) * 2.0, par.GetLitMan().GetLast());
}

PARSER_TEST(Nested_function_calls_with_refs) {
	auto par{GenerateTestingInstance()};

	par.ParseLine("_Func Setter &lhs rhs;");
	par.ParseLine("_Set lhs rhs;");
	par.ParseLine("_Return;");

	par.ParseLine("_Func AddOne n;");
	par.ParseLine("    _Set res 0;");
	ASSERT_NO_THROW(par.ParseLine("    res n Setter;"));
	ASSERT_NO_THROW(par.ParseLine("    _Return res 1 +;"));

	ASSERT_NO_THROW(par.ParseLine("_Func AddOneThenDouble n;"));
	ASSERT_NO_THROW(par.ParseLine("    _Set res 0;"));
	ASSERT_NO_THROW(par.ParseLine("    res n Setter;"));
	ASSERT_NO_THROW(par.ParseLine("    _Return res AddOne 2 *;"));

	ASSERT_NO_THROW(par.ParseLine("5 AddOneThenDouble;"));
	ASSERT_DOUBLE_EQ(12.0, par.GetLitMan().GetLast());
}

PARSER_TEST(Recursive_functions) {
	auto par{GenerateTestingInstance()};

	par.ParseLine("_Func MyFac n;");
	par.ParseLine("_If n 1 <=: _Return 1;");
	ASSERT_NO_THROW(par.ParseLine("_Return n 1 - MyFac n *;"));

	constexpr auto fac = [](auto n) constexpr {
		if (n <= 1) {
			n = 1;
		} else for (auto const i : view::iota(1U, n)) {
			n *= i;
		}

		return n;
	};

	for (auto const i : view::iota(0U, 10U)) {
		ASSERT_NO_THROW(par.ParseLine(std::format("{} MyFac", i)));
		ASSERT_DOUBLE_EQ(fac(i), par.GetLitMan().GetLast()) << "Param is: " << i;
	}
}

PARSER_TEST(Passing_through_a_by_value_param) {
	constexpr std::array Lines{
		"_Func FuncByValue param",
		"    _Set param 5;",
		"    _Return;"
	};

	auto par{GenerateTestingInstance()};
	for (auto const line : Lines) {
		par.ParseLine(line);
	}

	par.ParseLine("_Set a 0");
	ASSERT_NO_THROW(par.ParseLine("a FuncByValue"));
	ASSERT_DOUBLE_EQ(0.0, *par.GetLitMan().Get("a")); // No modification please.

	ASSERT_NO_THROW(par.ParseLine("-a FuncByValue")); // Negative sign will turn it into an rvalue.
	ASSERT_DOUBLE_EQ(0.0, *par.GetLitMan().Get("a")); // No modification please.

	ASSERT_NO_THROW(par.ParseLine("5 FuncByValue"));
	ASSERT_NO_THROW(par.ParseLine("_pi FuncByValue"));
	ASSERT_NO_THROW(par.ParseLine(std::format("{} FuncByValue", Keyword::ToStringView(KeywordType::Last))));
}

PARSER_TEST(Passing_through_a_by_ref_param) {
	constexpr std::array Lines{
		"_Func FuncByRef &param",
		"    _Return;"
	};

	auto par{GenerateTestingInstance()};
	for (auto const line : Lines) {
		par.ParseLine(line);
	}

	par.ParseLine("_Set a 0");
	ASSERT_NO_THROW(par.ParseLine("a FuncByRef"));
	ASSERT_ANY_THROW(par.ParseLine("-a FuncByRef")); // Negative sign will turn it into an rvalue.
	ASSERT_ANY_THROW(par.ParseLine("5 FuncByRef"));
	ASSERT_ANY_THROW(par.ParseLine("pi FuncByValue"));
	// Last is always an rvalue.
	ASSERT_ANY_THROW(par.ParseLine(std::format("{} FuncByValue", Keyword::ToStringView(KeywordType::Last))));
}

PARSER_TEST(Selection_statement_one_branch) {
	constexpr auto Lines = std::array{
		"_Func Max a b",
		"_Set ret a;",
		"_If b a >: _Set ret b;",
		"_Return ret;",
	};

	Parser par{GenerateTestingInstance()};
	auto& litMan{par.GetLitMan()};

	// Define the function.
	for (auto const& line : Lines) {
		par.ParseLine(line);
	}

	for (constexpr auto Lim{3}; auto const lhs : view::iota(-Lim, Lim)) {
		for (auto const rhs : view::iota(-Lim, Lim)) {
			ASSERT_NO_THROW(par.ParseLine(std::format("{} {} {}", lhs, rhs, "Max")))
				<< "Params: " << lhs << ", " << rhs;
			ASSERT_DOUBLE_EQ(std::max(lhs, rhs), litMan.GetLast())
				<< "Params: " << lhs << ", " << rhs;
		}
	}
}

PARSER_TEST(Selection_statement_if_else) {
	constexpr auto Lines = std::array{
		"_Func Max a b",
		"_If a b >: _Set ret a;",
		"_Else      _Set ret b;",
		"_Return ret;",
	};

	Parser par{GenerateTestingInstance()};
	auto& litMan{par.GetLitMan()};

	// Define the function.
	for (auto const& line : Lines) {
		par.ParseLine(line);
	}

	for (constexpr auto Lim{3}; auto const lhs : view::iota(-Lim, Lim)) {
		for (auto const rhs : view::iota(-Lim, Lim)) {
			ASSERT_NO_THROW(par.ParseLine(std::format("{} {} {}", lhs, rhs, "Max")));
			ASSERT_DOUBLE_EQ(std::max(lhs, rhs), litMan.GetLast());
		}
	}
}

PARSER_TEST(Selection_statement_if_single_elif) {
	constexpr auto Lines = std::array{
		"_Func Max a b",
		"_If a b >:    _Set ret a;",
		"_Elif a b <=: _Set ret b;",
		"_Return ret;",
	};

	Parser par{GenerateTestingInstance()};
	auto& litMan{par.GetLitMan()};

	// Define the function.
	for (auto const& line : Lines) {
		par.ParseLine(line);
	}

	for (constexpr auto Lim{3}; auto const lhs : view::iota(-Lim, Lim)) {
		for (auto const rhs : view::iota(-Lim, Lim)) {
			ASSERT_NO_THROW(par.ParseLine(std::format("{} {} {}", lhs, rhs, "Max")));
			ASSERT_DOUBLE_EQ(std::max(lhs, rhs), litMan.GetLast());
		}
	}
}

PARSER_TEST(Selection_statement_multiple_elifs_rounding) {
	for (constexpr auto ElifCountLim{10}; auto const currElifCount : view::iota(1, ElifCountLim + 1)) {
		// Defining the function
		auto par{GenerateTestingInstance()};
		par.ParseLine("_Func MyFunc a");
		par.ParseLine("_If a 0 <: _Set ret 0;");
		for (auto const i : view::iota(1, currElifCount + 1)) {
			ASSERT_NO_THROW(par.ParseLine(std::format("_Elif a {0} <: _Set ret {0};", 5.0 * i)));
		}
		par.ParseLine(std::format("_Else _Set ret {};", 5.0 * currElifCount));
		par.ParseLine("_Return ret;");

		auto const MyFunc = [=](double a) -> double {
			double ret{};
			if (a < 0.0) { ret = 0.0; } // _If a 0 < _Set ret 0;
			for (auto const i : view::iota(1, currElifCount + 1)) {
				if (a < 5.0 * i) { ret = 5.0 * i; } // _Elif a {5*i} < _Set ret {5*i};
			}
			// Can't put else here, inverting the last condition instead (a < 5 * currElifCount).
			if (a >= 5.0 * currElifCount) { ret = 5.0 * currElifCount; } // _Else _Set ret {5*currElifCount};
			return ret; // _Return ret;
		};

		// Calling the function.
		for (auto const i : view::iota(-5, 5 * (currElifCount + 1))) {
			par.ParseLine(std::format("{} MyFunc", i));
			ASSERT_DOUBLE_EQ(MyFunc(i), par.GetLitMan().GetLast()) 
				<< std::format("Parameter was {:d}", i);
		}
	}
}

PARSER_TEST(Selection_statement_multiple_decoder) {
	auto par{GenerateTestingInstance()};
	par.ParseLine("_Func ExecuteOpCode code a b");
	par.ParseLine("_If   code 0 ==: _Set ret a b +;");
	par.ParseLine("_Elif code 1 ==: _Set ret a b -;");
	par.ParseLine("_Elif code 2 ==: _Set ret a b *;");
	par.ParseLine("_Elif code 3 ==: _Set ret a b /;");
	par.ParseLine("_Else            _Set ret _inf;");
	par.ParseLine("_Return ret;");

	constexpr auto cppVersionOfExecuteOpCode = [](auto code, auto a, auto b) constexpr {
		double ret{}; // Not needed in the script, because the concept of scopes is non-existant.
		if (code == 0.0)      { ret = a + b; }
		else if (code == 1.0) { ret = a - b; }
		else if (code == 2.0) { ret = a * b; }
		else if (code == 3.0) { ret = a / b; }
		else                  { ret = std::numeric_limits<double>::infinity(); }
		return ret;
	};

	for (auto const code : view::iota(-1, 10)) {
		constexpr auto A{-5.0};
		constexpr auto B{15.0};
		par.ParseLine(std::format("{} {} {} ExecuteOpCode;", code, A, B));
		ASSERT_DOUBLE_EQ(cppVersionOfExecuteOpCode(code, A, B), par.GetLitMan().GetLast())
			<< std::format("Code was {:d}", code);
	}
}

PARSER_TEST(Selection_statement_if_elif_else) {
	// Depending on the above test, we can be confident that the behaviour of the if is completely
	// independent of the behaveiuor of its else statement if it exists, so we only need to test 
	// one case of the if.

	constexpr auto Lines = std::array{
		"_Func Max a b",
		"_If a b >:   _Set ret a;",
		"_Elif a b <: _Set ret b;",
		"_Else        _Set ret a;",
		"_Return ret;",
	};

	Parser par{GenerateTestingInstance()};
	auto& litMan{par.GetLitMan()};

	// Define the function.
	for (auto const& line : Lines) {
		par.ParseLine(line);
	}

	for (constexpr auto Lim{3}; auto const lhs : view::iota(-Lim, Lim)) {
		for (auto const rhs : view::iota(-Lim, Lim)) {
			ASSERT_NO_THROW(par.ParseLine(std::format("{} {} {}", lhs, rhs, "Max")));
			ASSERT_DOUBLE_EQ(std::max(lhs, rhs), litMan.GetLast());
		}
	}
}

PARSER_TEST(Selection_statement_hanging_elif) {
	auto par{GenerateTestingInstance()};
	par.ParseLine("_Func DoesNotMatter a b");
	ASSERT_ANY_THROW(par.ParseLine("_Elif a b <: _List;"));
}

PARSER_TEST(Selection_statement_hanging_else) {
	auto par{GenerateTestingInstance()};
	par.ParseLine("_Func DoesNotMatter unused");
	ASSERT_ANY_THROW(par.ParseLine("_Else _List;"));
}

PARSER_TEST(Selection_statement_elif_after_else) {
	auto optPar = std::optional{GenerateTestingInstance()};
	optPar->ParseLine("_Func Max a b           ");
	optPar->ParseLine("_If a b >:   _Set ret a;");
	optPar->ParseLine("_Else        _Set ret a;");
	ASSERT_ANY_THROW(optPar->ParseLine("_Elif a b <: _Set ret b;"));
}

PARSER_TEST(Selection_statement_multiple_else_s) {
	auto optPar = std::optional{GenerateTestingInstance()};
	optPar->ParseLine("_Func Max a b          ");
	optPar->ParseLine("_If a b >:   _Set ret a;");
	optPar->ParseLine("_Else        _Set ret a;");
	ASSERT_ANY_THROW(optPar->ParseLine("_Else       _Set ret b; "));
}

PARSER_TEST(Selection_statement_in_between_statements) {
	auto par{GenerateTestingInstance()};

	par.ParseLine("_Func F n;");
	par.ParseLine("_If   n 1 ==: _Return n 1 +;");
	par.ParseLine("_Set x 5;");
	ASSERT_THROW(par.ParseLine("_Elif n 2 ==: _Return n 1 -;"), SyntaxError);
}

PARSER_TEST(Multiple_returns_and_branching) {
	for (auto const i : view::iota(0U, 7U)) {
		auto par{GenerateTestingInstance()};

		using namespace std::string_literals;
		par.ParseLine("_Func Sign n;");
		ASSERT_NO_THROW(par.ParseLine("_If n 0 >:   "s 
			+ ((i & 1) ? "_Return  1;" : "_Set ret  1;"))) << i;
		ASSERT_TRUE(par.IsParsingFunction()) << i;

		ASSERT_NO_THROW(par.ParseLine("_Elif n 0 <: "s 
			+ ((i & 2) ? "_Return -1;" : "_Set ret -1;"))) << i;
		ASSERT_TRUE(par.IsParsingFunction()) << i;

		ASSERT_NO_THROW(par.ParseLine("_Else        "s 
			+ ((i & 4) ? "_Return  0;" : "_Set ret  0;"))) << i;
		ASSERT_TRUE(par.IsParsingFunction()) << i;

		ASSERT_NO_THROW(par.ParseLine("_Return ret;")) << i;
		ASSERT_FALSE(par.IsParsingFunction()) << i;

		par.ParseLine("-1 Sign;");
		ASSERT_DOUBLE_EQ(-1.0, par.GetLitMan().GetLast());

		par.ParseLine("0 Sign;");
		ASSERT_DOUBLE_EQ(0.0, par.GetLitMan().GetLast());

		par.ParseLine("1 Sign;");
		ASSERT_DOUBLE_EQ(1.0, par.GetLitMan().GetLast());
	}
}

PARSER_TEST(Different_return_types_and_branching) {
	// [_Return;] = 1, [_Return n;] = 0
	/* 1, 5 */ {
		auto par{GenerateTestingInstance()};
		par.ParseLine("_Func Sign n;");
		ASSERT_NO_THROW(par.ParseLine("_If n 0 >:    _Return;"));
		ASSERT_ANY_THROW(par.ParseLine("_Elif n 0 <: _Return -1;"));
	}

	/* 2, 6 */ {
		auto par{GenerateTestingInstance()};
		par.ParseLine("_Func Sign n;");
		ASSERT_NO_THROW(par.ParseLine("_If n 0 >:    _Return -1;"));
		ASSERT_ANY_THROW(par.ParseLine("_Elif n 0 <: _Return;"));
	}

	/* 3 */ {
		auto par{GenerateTestingInstance()};
		par.ParseLine("_Func Sign n;");
		ASSERT_NO_THROW(par.ParseLine("_If n 0 >:   _Return;"));
		ASSERT_NO_THROW(par.ParseLine("_Elif n 0 <: _Return;"));
		ASSERT_ANY_THROW(par.ParseLine("Else        _Return 0;"));
	}
	
	/* 4 */ {
		auto par{GenerateTestingInstance()};
		par.ParseLine("_Func Sign n;");
		ASSERT_NO_THROW(par.ParseLine("_If n 0 >:   _Return 1;"));
		ASSERT_NO_THROW(par.ParseLine("_Elif n 0 <: _Return -1;"));
		ASSERT_ANY_THROW(par.ParseLine("Else        _Return;"));
	}

	/* Mutliple elifs (same as 4) */ {
		auto par{GenerateTestingInstance()};
		par.ParseLine("_Func Sign n;");
		ASSERT_NO_THROW(par.ParseLine("_If n 0 >:     _Return 1;"));
		ASSERT_NO_THROW(par.ParseLine("_Elif n 0 <:   _Return -1;"));
		ASSERT_ANY_THROW(par.ParseLine("_Elif n 0 ==: _Return;"));
	}
}

PARSER_TEST(Different_return_types_one_in_branch_one_is_not) {
	/* Number then none */ {
		auto par{GenerateTestingInstance()};
		ASSERT_NO_THROW(par.ParseLine("_Func MyFunc n;"));
		ASSERT_TRUE(par.IsParsingFunction());

		ASSERT_NO_THROW(par.ParseLine("    _If n 10 <: _Return 10;"));
		ASSERT_TRUE(par.IsParsingFunction());

		ASSERT_ANY_THROW(par.ParseLine("_Return;"));
	}

	/* None then number */ {
		auto par{GenerateTestingInstance()};
		ASSERT_NO_THROW(par.ParseLine("_Func MyFunc n;"));
		ASSERT_TRUE(par.IsParsingFunction());

		ASSERT_NO_THROW(par.ParseLine("    _If n 10 <: _Return;"));
		ASSERT_TRUE(par.IsParsingFunction());

		ASSERT_ANY_THROW(par.ParseLine("_Return 10;"));
	}
}

PARSER_TEST(Selection_statement_multiple_decoder_using_multiple_returns) {
	auto par{GenerateTestingInstance()};
	par.ParseLine("_Func ExecuteOpCode code a b");
	ASSERT_NO_THROW(par.ParseLine("_If   code 0 ==: _Return a b +;"));
	ASSERT_TRUE(par.IsParsingFunction());

	ASSERT_NO_THROW(par.ParseLine("_Elif code 1 ==: _Return a b -;"));
	ASSERT_TRUE(par.IsParsingFunction());

	ASSERT_NO_THROW(par.ParseLine("_Elif code 2 ==: _Return a b *;"));
	ASSERT_TRUE(par.IsParsingFunction());

	ASSERT_NO_THROW(par.ParseLine("_Elif code 3 ==: _Return a b /;"));
	ASSERT_TRUE(par.IsParsingFunction());

	ASSERT_NO_THROW(par.ParseLine("_Else            _Return _inf;"));
	ASSERT_FALSE(par.IsParsingFunction());

	constexpr auto ExecuteOpCode = [](auto code, auto a, auto b) constexpr {
		if (code == 0.0)      { return a + b; }
		else if (code == 1.0) { return a - b; }
		else if (code == 2.0) { return a * b; }
		else if (code == 3.0) { return a / b; }
		else                  { return std::numeric_limits<double>::infinity(); }
	};

	for (auto const code : view::iota(-1, 10)) {
		constexpr auto A{-5.0};
		constexpr auto B{15.0};
		par.ParseLine(std::format("{} {} {} ExecuteOpCode;", code, A, B));
		ASSERT_DOUBLE_EQ(ExecuteOpCode(code, A, B), par.GetLitMan().GetLast())
			<< std::format("Code was {:d}", code);
	}
}

PARSER_TEST(Serializing_a_constant) {
	auto optPar = std::optional{GenerateTestingInstance()};

	optPar->ParseLine("_Set fourThirteen 413");
	optPar->ParseLine("_Set tenEighty 1080");
	// Saving...
	EXPECT_NO_THROW(optPar->ParseLine("_Save fourThirteen Testing")) << "Threw while saving.";
	EXPECT_NO_THROW(optPar->ParseLine("_Save tenEighty Testing")) << "Threw while saving.";

	// Restarting...
	optPar.emplace(GenerateTestingInstance());

	// Loading...
	EXPECT_NO_THROW(optPar->ParseLine("_Load Testing")) << "Threw while loading.";

	// Verifying...
	EXPECT_NO_THROW(optPar->ParseLine("fourThirteen"));
	EXPECT_DOUBLE_EQ(413.0, optPar->GetLitMan().GetLast());

	EXPECT_NO_THROW(optPar->ParseLine("tenEighty"));
	EXPECT_DOUBLE_EQ(1080.0, optPar->GetLitMan().GetLast());

	// Can't use asserts because of this shit right here.
	// Clean up by opening the file without the append flag.
	std::ofstream{IO::GetSerializationPath() / "Testing.txt"};
}

PARSER_TEST(Serializing_a_function) {
	auto optPar = std::optional{GenerateTestingInstance()};

	optPar->ParseLine("_Func Add a b");
	optPar->ParseLine("    _Return a b +;");
	optPar->ParseLine("_Func Sub a b");
	optPar->ParseLine("    _Return a b -;");
	// Saving...
	EXPECT_NO_THROW(optPar->ParseLine("_Save Add Testing;")) << "Threw while saving.";
	EXPECT_NO_THROW(optPar->ParseLine("_Save Sub Testing;")) << "Threw while saving.";

	// Restarting...
	optPar.emplace(GenerateTestingInstance());

	// Loading...
	EXPECT_NO_THROW(optPar->ParseLine("_Load Testing;")) << "Threw while loading.";

	// Verifying...
	EXPECT_NO_THROW(optPar->ParseLine("5 10 Add;"));
	EXPECT_DOUBLE_EQ(5 + 10, optPar->GetLitMan().GetLast());

	EXPECT_NO_THROW(optPar->ParseLine("5 10 Sub;"));
	EXPECT_DOUBLE_EQ(5 - 10, optPar->GetLitMan().GetLast());

	// Can't use asserts because of this shit right here.
	// Clean up by opening the file without the append flag.
	std::ofstream{IO::GetSerializationPath() / "Testing.txt"};
}

PARSER_TEST(Unscope_in_global_scope) {
	auto par{GenerateTestingInstance()};

	ASSERT_NO_THROW(par.ParseLine("_Unscope;"));
}

PARSER_TEST(Unscope_in_function_scope) {
	auto par{GenerateTestingInstance()};

	par.ParseLine("_Func AddOne n;");
	ASSERT_NO_THROW(par.ParseLine("_Unscope;")); // Should do nothing.
	ASSERT_TRUE(par.IsParsingFunction());

	ASSERT_NO_THROW(par.ParseLine("_Set res n 1 +;"));
	ASSERT_NO_THROW(par.ParseLine("_Set res -_inf;"));
	ASSERT_NO_THROW(par.ParseLine("_Unscope;")); // Cancels last line.
	ASSERT_NO_THROW(par.ParseLine("_Return res"));
	ASSERT_NO_THROW(par.ParseLine("5 AddOne;"));
	ASSERT_DOUBLE_EQ(5. + 1., par.GetLitMan().GetLast());

	ASSERT_NO_THROW(par.ParseLine("_Func F;"));
	ASSERT_NO_THROW(par.ParseLine("_Unscope;")); // Should do nothing.
	ASSERT_TRUE(par.IsParsingFunction());

	ASSERT_NO_THROW(par.ParseLine("_Unscope _Func;"));
	ASSERT_FALSE(par.IsParsingFunction());

	// Now that we are in the global scope again.
	ASSERT_ANY_THROW(par.ParseLine("_Return;"));
}

PARSER_TEST(Unscope_in_conditional_if) {
	auto par{GenerateTestingInstance()};
	par.ParseLine("_Func MyAbs param;");
	ASSERT_NO_THROW(par.ParseLine("_If param 0 <: _Set param -param;"));
	ASSERT_NO_THROW(par.ParseLine("_Unscope")); // Now the function is useless.
	ASSERT_NO_THROW(par.ParseLine("_Return param;"));

	ASSERT_NO_THROW(par.ParseLine("-3 MyAbs;"));
	ASSERT_DOUBLE_EQ(-3.0, par.GetLitMan().GetLast()); // Would've been 3 without unscoping.

	par.ParseLine("_Func MyFunc param;");
	// Disallowed.
	ASSERT_THROW(par.ParseLine("_If param 0 >: _Unscope;"), SyntaxError);
}

PARSER_TEST(Unscope_in_conditional_elif) {
	auto par{GenerateTestingInstance()};
	par.ParseLine("_Func MyCompare lhs rhs;");
	ASSERT_NO_THROW(par.ParseLine("_If lhs rhs >: _Return 1;"));
	ASSERT_NO_THROW(par.ParseLine("_Elif lhs rhs <: _Return -1;"));
	// Now the function basically casts the number to bool xd.
	ASSERT_NO_THROW(par.ParseLine("_Unscope")); 
	ASSERT_NO_THROW(par.ParseLine("_Return 0;"));

	ASSERT_NO_THROW(par.ParseLine("1 2 MyCompare;"));
	ASSERT_DOUBLE_EQ(0.0, par.GetLitMan().GetLast()); // Would've been -1 without unscoping

	par.ParseLine("_Func MyFunc param;");
	par.ParseLine("_If param 0 >: _Return;");
	// Disallowed.
	ASSERT_THROW(par.ParseLine("_Elif param 0 <: _Unscope;"), SyntaxError);
}

PARSER_TEST(Unscoping_multiple_times_in_a_row) {
	auto par{GenerateTestingInstance()};
	par.ParseLine("_Func Meaningless n;");

	for ([[maybe_unused]] auto const i : view::iota(0U, 5U)) {
		ASSERT_NO_THROW(par.ParseLine("_Unscope;"));
		ASSERT_TRUE(par.IsParsingFunction());
	}

	par.ParseLine("_If n 0 ==: _Return 0;");

	constexpr auto BranchCount{5U};
	static_assert(BranchCount > 0);

	// Pushing...
	for (auto const i : view::iota(0U, BranchCount)) {
		par.ParseLine(std::format("_Elif n {0} ==: _Return {0};", i));
	}
	// Popping...
	for ([[maybe_unused]] auto const i : view::iota(0U, BranchCount)) {
		ASSERT_NO_THROW(par.ParseLine("_Unscope;"));
		ASSERT_TRUE(par.IsParsingFunction());
	}

	ASSERT_NO_THROW(par.ParseLine("_Unscope;")); // First line in the function.
	ASSERT_NO_THROW(par.ParseLine("_Return;")); // No return type conflict.
	ASSERT_FALSE(par.IsParsingFunction());
}

PARSER_TEST(Unscope_in_conditional_else) {
	auto par{GenerateTestingInstance()};
	par.ParseLine("_Func MyCompare lhs rhs;");
	// Cannot use returns here, otherwise the function will terminate too soon.
	// Parameter lhs must be used for the case of equal numbers.
	ASSERT_NO_THROW(par.ParseLine("_If lhs rhs >:   _Set lhs 1;"));
	ASSERT_NO_THROW(par.ParseLine("_Elif lhs rhs <: _Set lhs -1;"));
	ASSERT_NO_THROW(par.ParseLine("_Else            _Set lhs 0;"));
	ASSERT_NO_THROW(par.ParseLine("_Unscope")); 
	ASSERT_NO_THROW(par.ParseLine("_Return lhs;"));

	ASSERT_NO_THROW(par.ParseLine("10 10 MyCompare;"));
	ASSERT_DOUBLE_EQ(10.0, par.GetLitMan().GetLast()); // Would have been zero without unscoping.

	par.ParseLine("_Func MyFunc param;");
	par.ParseLine("_If param 0 >: _Return;");
	par.ParseLine("_Elif param 0 <: _Return;");
	// Disallowed.
	ASSERT_THROW(par.ParseLine("_Else _Unscope;"), SyntaxError);
}

PARSER_TEST(Unscope_deleting_literals) {
	auto par{GenerateTestingInstance()};

	// In global scope.
	par.ParseLine("_Set myLit 15.0;");
	ASSERT_NO_THROW(par.ParseLine("_Unscope myLit;"));
	ASSERT_FALSE(par.GetLitMan().IsVisible("myLit"));

	// In function scope.
	par.ParseLine("_Func MyFunc param;");
	par.ParseLine("_Set myLit param;");

	// due to how the keyword works, deleting literals in function scopes is very error
	// prone, and is not very useful in most cases, so I am gonna disallow it for now.
	ASSERT_ANY_THROW(par.ParseLine("_Unscope myLit;"));

	// Syntax errors
	ASSERT_ANY_THROW(par.ParseLine("_Unscope _If"));       // Keyword as first parameter.
	ASSERT_ANY_THROW(par.ParseLine("_Unscope sdaasdasd")); // 
	ASSERT_ANY_THROW(par.ParseLine("_Unscope +"));         // Invalid first parameter.
}

PARSER_TEST(Unscope_deleting_and_renaming_functions) {
	auto par{GenerateTestingInstance()};

	// Deleting.
	par.ParseLine("_Func UselessFunc unused");
	par.ParseLine("    _Return;");
	ASSERT_NO_THROW(par.ParseLine("_Unscope UselessFunc;"));
	ASSERT_FALSE(par.GetFunMan().IsDefined("UselessFunc"));

	// Renaming.
	par.ParseLine("_Func UselessFunc unused");
	par.ParseLine("    _Return;");
	ASSERT_NO_THROW(par.ParseLine("_Unscope UselessFunc BestFuncEver;"));
	ASSERT_FALSE(par.GetFunMan().IsDefined("UselessFunc"));
	ASSERT_TRUE(par.GetFunMan().IsDefined("BestFuncEver"));
	
	// The old name must valid and must exist.
	// The new name must be a valid idenitifier.
	ASSERT_ANY_THROW(par.ParseLine("_Unscope DoesNotExist NewName"));
	ASSERT_ANY_THROW(par.ParseLine("_Unscope hjsabdhjsasd NewName"));
	ASSERT_ANY_THROW(par.ParseLine("_Unscope BestFuncEver _If"));
	ASSERT_ANY_THROW(par.ParseLine("_Unscope BestFuncEver 123"));
	ASSERT_ANY_THROW(par.ParseLine("_Unscope hjsabdhjsasd oisjdlkadlksad"));
}

PARSER_TEST(Err_keyword) {
	auto par{GenerateTestingInstance()};

	// No errors in global scope
	ASSERT_THROW(par.ParseLine("_Err 'This is an error message'"), SyntaxError);

	par.ParseLine("_Func ErrorOut unused;");
	ASSERT_NO_THROW(par.ParseLine("_Err 'YAY!'"));
	ASSERT_FALSE(par.IsParsingFunction());
	ASSERT_THROW(par.ParseLine("5 ErrorOut;"), UserError);

	try { 
		par.ParseLine("5 ErrorOut;"); 
		// Made sure it throws in the test before.
	} catch (UserError const& err) {
		ASSERT_EQ("YAY!", err.GetMessage());
	}

	/* Inside a branch */ {
		par.ParseLine("_Func AssertPositive n;");
		ASSERT_NO_THROW(par.ParseLine("_If n 0 <: _Err 'Expected a positive number';"));
		par.ParseLine("_Return;");

		for (auto const i : view::iota(-10, 0)) {
			ASSERT_THROW(par.ParseLine(std::format("{} AssertPositive", i)), UserError);
		}
		for (auto const i : view::iota(0, 11)) {
			ASSERT_NO_THROW(par.ParseLine(std::format("{} AssertPositive", i)));
		}
	}

	// No quotes allowed ever.
	ASSERT_THROW(par.ParseLine("_Err 'error 'message right here'"), SyntaxError);
	
	// No empty error message.
	ASSERT_THROW(par.ParseLine("_Err ''"), SyntaxError);
}

PARSER_TEST(Multiple_returns_and_branching_with_error_statements_none) {
	for (auto const i : view::iota(1U, 7U)) {
		auto par{GenerateTestingInstance()};

		using namespace std::string_literals;
		par.ParseLine("_Func Shit n;");
		ASSERT_NO_THROW(par.ParseLine("_If n 0 >:   "s 
			+ ((i & 1) ? "_Return;" : "_Err 'yay!';"))) << i;
		ASSERT_TRUE(par.IsParsingFunction()) << i;

		ASSERT_NO_THROW(par.ParseLine("_Elif n 0 <: "s 
			+ ((i & 2) ? "_Return;" : "_Err 'yay!';"))) << i;
		ASSERT_TRUE(par.IsParsingFunction()) << i;

		ASSERT_NO_THROW(par.ParseLine("_Else        "s 
			+ ((i & 4) ? "_Return;" : "_Err 'yay!';"))) << i;
		ASSERT_FALSE(par.IsParsingFunction()) << i;
		ASSERT_EQ(FuncReturnType::None, par.GetFunMan().Get("Shit").ReturnType) << i;
	}
}

PARSER_TEST(Multiple_returns_and_branching_with_error_statements_number) {
	for (auto const i : view::iota(1U, 7U)) {
		auto par{GenerateTestingInstance()};

		using namespace std::string_literals;
		par.ParseLine("_Func Shit n;");
		ASSERT_NO_THROW(par.ParseLine("_If n 0 >:   "s 
			+ ((i & 1) ? "_Return 1.0;" : "_Err 'yay!';"))) << i;
		ASSERT_TRUE(par.IsParsingFunction()) << i;

		ASSERT_NO_THROW(par.ParseLine("_Elif n 0 <: "s 
			+ ((i & 2) ? "_Return 1.0;" : "_Err 'yay!';"))) << i;
		ASSERT_TRUE(par.IsParsingFunction()) << i;

		ASSERT_NO_THROW(par.ParseLine("_Else        "s 
			+ ((i & 4) ? "_Return 1.0;" : "_Err 'yay!';"))) << i;
		ASSERT_FALSE(par.IsParsingFunction()) << i;
		ASSERT_EQ(FuncReturnType::Number, par.GetFunMan().Get("Shit").ReturnType) << i;
	}
}