#include "pch.h"

#include <../../ArCalc/Source/Parser.h>
#include "Util/Str.h"
#include <Util/IO.h>
#include <Util/Random.h>
#include <Util/Ser.h>

#define PARSER_TEST(_testName) TEST_F(ParserTests, _testName)

using namespace ArCalc;

class ParserTests : public testing::Test {
private:
	inline static std::stringstream s_BullshitSS{};

public:
	Parser GenerateTestingInstance() {
		Parser par{s_BullshitSS};
		return par;
	}

	void TestSelectionStatement(std::string_view caseName, std::string_view code) {
		if (Test::HasFailure()) {
			return;
		}

		Parser par{GenerateTestingInstance()};
		auto& litMan{par.GetLitMan()};

		// Define the function.
		for (auto const lines{Str::SplitOn(code, "\n")}; auto const& line : lines) {
			par.ParseLine(line);
		}

		for (constexpr auto Lim{3}; auto const lhs : view::iota(-Lim, Lim)) {
			for (auto const rhs : view::iota(-Lim, Lim)) {
				ASSERT_NO_THROW(par.ParseLine(std::format("{} {} {}", lhs, rhs, "Max"))) << caseName;
				ASSERT_DOUBLE_EQ(std::max(lhs, rhs), litMan.GetLast()) << caseName;
			}
		}
	};
};

PARSER_TEST(Semicolon_at_end_of_line) {
	constexpr auto code{
R"(5 10 +
   _List
   _Func Max a b
   _Set ret a
   _If a b <
   _Set ret b
   _Return ret)"
	};
	
	auto const lines{Str::SplitOn(code, "\n")};
	// Without ;
	{
		auto par{GenerateTestingInstance()};
		for (auto const& line : lines) {
			ASSERT_NO_THROW(par.ParseLine(line));
			ASSERT_FALSE(par.IsLineEndsWithSemiColon());
		}
	}

	// With ;
	{
		auto par{GenerateTestingInstance()};
		for (auto const& line : lines) {
			ASSERT_NO_THROW(par.ParseLine(line + ";"));
			ASSERT_TRUE(par.IsLineEndsWithSemiColon());
		}
	}
}

PARSER_TEST(Normal_expression_and_Last_keyword) {
	auto par{GenerateTestingInstance()};

	par.ParseLine("5 10 +");
	ASSERT_DOUBLE_EQ(15.0, par.GetLitMan().GetLast());
}

PARSER_TEST(Set_statement) {
	auto par{GenerateTestingInstance()};
	auto& litMan{par.GetLitMan()};

	par.ParseLine("_Set x 5;");
	ASSERT_TRUE(par.IsLineEndsWithSemiColon());
	ASSERT_TRUE(litMan.IsVisible("x"));
	ASSERT_DOUBLE_EQ(5.0, *litMan.Get("x"));
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

PARSER_TEST(Defining_a_function_with_no_refs) {
	using namespace std::string_view_literals;
	constexpr std::array Lines{
		"_Func Add a b"sv,
		"_Set sum a b +"sv,
		"_Return sum"sv,
	};

	Parser par{GenerateTestingInstance()};
	ASSERT_FALSE(par.IsParsingFunction());
	for (auto const& line : Lines | view::take(std::size(Lines) - 1)) {
		ASSERT_NO_THROW(par.ParseLine(line));
		ASSERT_TRUE(par.IsParsingFunction());
	}

	ASSERT_NO_THROW(par.ParseLine(Lines.back()));
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

	using namespace std::string_literals;
	std::array Lines{
		std::format("_Func {} a b\n", FuncName),
		"_Set res a b -\n"s,
		"_Return res\n"s,
	};

	Parser par{GenerateTestingInstance()};
	for (auto const& line : Lines) {
		par.ParseLine(line); // Already tested above.
	}

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
	ASSERT_NO_THROW(par.ParseLine("pi FuncByValue"));
	ASSERT_NO_THROW(par.ParseLine(std::format("{} FuncByValue", Keyword::ToString(KeywordType::Last))));
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
	ASSERT_ANY_THROW(par.ParseLine(std::format("{} FuncByValue", Keyword::ToString(KeywordType::Last))));
}

PARSER_TEST(Selection_statement_one_branch) {
	TestSelectionStatement("Same line",
		"_Func Max a b\n"
		"_Set ret a;\n"
		"_If b a > _Set ret b;\n"
		"_Return ret;\n"
	);

	TestSelectionStatement("Next line",
		"_Func Max a b\n"
		"_Set ret a;\n"
		"_If b a >\n"
		"    _Set ret b;\n"
		"_Return ret;\n"
	);
}

PARSER_TEST(Selection_statement_if_else) {
	TestSelectionStatement("Same same",
		"_Func Max a b\n"
		"_If a b > _Set ret a;\n"
		"_Else     _Set ret b;\n"
		"_Return ret;\n"
	);

	TestSelectionStatement("Same next",
		"_Func Max a b\n"
		"_If a b > _Set ret a;\n"
		"_Else\n"
		"    _Set ret b;\n"
		"_Return ret;\n"
	);

	TestSelectionStatement("Next same",
		"_Func Max a b\n"
		"_If a b >\n"
		"    _Set ret a;\n"
		"_Else _Set ret b;\n"
		"_Return ret;\n"
	);

	TestSelectionStatement("Next next",
		"_Func Max a b\n"
		"_If a b >\n"
		"    _Set ret a;\n"
		"_Else\n"
		"    _Set ret b;\n"
		"_Return ret;\n"
	);
}

PARSER_TEST(Selection_statement_if_single_elif) {
	TestSelectionStatement("Same same",
		"_Func Max a b\n"
		"_If a b >    _Set ret a;\n"
		"_Elif a b <= _Set ret b;\n"
		"_Return ret;\n"
	);

	TestSelectionStatement("Same next",
		"_Func Max a b\n"
		"_If a b > _Set ret a;\n"
		"_Elif a b <=\n"
		"    _Set ret b;\n"
		"_Return ret;\n"
	);

	TestSelectionStatement("Next same",
		"_Func Max a b\n"
		"_If a b >\n"
		"    _Set ret a;\n"
		"_Elif a b <= _Set ret b;\n"
		"_Return ret;\n"
	);

	TestSelectionStatement("Next next",
		"_Func Max a b\n"
		"_If a b >\n"
		"    _Set ret a;\n"
		"_Elif a b <=\n"
		"    _Set ret b;\n"
		"_Return ret;\n"
	);
}

PARSER_TEST(Selection_statement_multiple_elifs_rounding) {
	for (constexpr auto ElifCountLim{10}; auto const currElifCount : view::iota(1, ElifCountLim + 1)) {
		// Defining the function
		auto par{GenerateTestingInstance()};
		par.ParseLine("_Func MyFunc a");
		par.ParseLine("_If a 0 < _Set ret 0;");
		for (auto const i : view::iota(1, currElifCount + 1)) {
			ASSERT_NO_THROW(par.ParseLine(std::format("_Elif a {0} < _Set ret {0};", 5.0 * i)));
		}
		par.ParseLine(std::format("_Else _Set ret {};", 5.0 * currElifCount));
		par.ParseLine("_Return ret;");

		auto const cppVersionOfMyFunc = [=](double a) -> double {
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
			ASSERT_DOUBLE_EQ(cppVersionOfMyFunc(i), par.GetLitMan().GetLast()) 
				<< std::format("Parameter was {:d}", i);
		}
	}
}

PARSER_TEST(Selection_statement_multiple_decoder) {
	auto par{GenerateTestingInstance()};
	par.ParseLine("_Func ExecuteOpCode code a b");
	par.ParseLine("_If   code 0 == _Set ret a b +;");
	par.ParseLine("_Elif code 1 == _Set ret a b -;");
	par.ParseLine("_Elif code 2 == _Set ret a b *;");
	par.ParseLine("_Elif code 3 == _Set ret a b /;");
	par.ParseLine("_Else           _Set ret _Inf;");
	par.ParseLine("_Return ret;");

	constexpr auto cppVersionOfExecuteOpCode = [](double code, double a, double b) constexpr {
		double ret{};
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
	// separated from its else statement if it exists, so we only need to test one case of the if.

	TestSelectionStatement("Same same same",
		"_Func Max a b\n"
		"_If a b >   _Set ret a;\n"
		"_Elif a b < _Set ret b;\n"
		"_Else       _Set ret a;\n"
		"_Return ret;\n"
	);

	TestSelectionStatement("Same same next",
		"_Func Max a b\n"
		"_If a b >   _Set ret a;\n"
		"_Elif a b < _Set ret b;\n"
		"_Else\n"
		"    _Set ret a;\n"
		"_Return ret;\n"
	);

	TestSelectionStatement("Same next same",
		"_Func Max a b\n"
		"_If a b >   _Set ret a;\n"
		"_Elif a b <\n"
		"    _Set ret b;\n"
		"_Else       _Set ret a;\n"
		"_Return ret;\n"
	);

	TestSelectionStatement("Same next next",
		"_Func Max a b\n"
		"_If a b >   _Set ret a;\n"
		"_Elif a b <\n"
		"    _Set ret b;\n"
		"_Else\n"
		"    _Set ret a;\n"
		"_Return ret;\n"
	);
}

PARSER_TEST(Selection_statement_hanging_elif) {
	auto par{GenerateTestingInstance()};
	par.ParseLine("_Func DoesNotMatter a b");
	ASSERT_ANY_THROW(par.ParseLine("_Elif a b < _List;"));
}

PARSER_TEST(Selection_statement_hanging_else) {
	auto par{GenerateTestingInstance()};
	par.ParseLine("_Func DoesNotMatter unused");
	ASSERT_ANY_THROW(par.ParseLine("_Else _List;"));
}

PARSER_TEST(Selection_statement_elif_after_else) {
	auto optPar = std::optional{GenerateTestingInstance()};

	// Same line else.
	auto szCaseName{"Same line"};
	optPar->ParseLine("_Func Max a b          ");
	optPar->ParseLine("_If a b >   _Set ret a;");
	optPar->ParseLine("_Else       _Set ret a;");
	ASSERT_ANY_THROW(optPar->ParseLine("_Elif a b < _Set ret b;")) << szCaseName;

	// Next line else.
	optPar.emplace(GenerateTestingInstance());
	szCaseName = "Next line";
	optPar->ParseLine("_Func Max a b          ");
	optPar->ParseLine("_If a b >   _Set ret a;");
	optPar->ParseLine("_Else                  ");
	optPar->ParseLine("    _Set ret a;        ");
	ASSERT_ANY_THROW(optPar->ParseLine("_Elif a b < _Set ret b;")) << szCaseName;
}

PARSER_TEST(Selection_statement_multiple_else_s) {
	auto optPar = std::optional{GenerateTestingInstance()};

	// Same line else.
	auto szCaseName{"Same line"};
	optPar->ParseLine("_Func Max a b          ");
	optPar->ParseLine("_If a b >   _Set ret a;");
	optPar->ParseLine("_Else       _Set ret a;");
	ASSERT_ANY_THROW(optPar->ParseLine("_Else      _Set ret b; ")) << szCaseName;

	// Next line else.
	optPar.emplace(GenerateTestingInstance());
	szCaseName = "Next line";
	optPar->ParseLine("_Func Max a b        ");
	optPar->ParseLine("_If a b > _Set ret a;");
	optPar->ParseLine("_Else                ");
	optPar->ParseLine("    _Set ret a;      ");
	ASSERT_ANY_THROW(optPar->ParseLine("_Else     _Set ret b;")) << szCaseName;
}

PARSER_TEST(Serializing_a_constant) {
	auto optPar = std::optional{GenerateTestingInstance()};

	optPar->ParseLine("_Set fourThirteen 413");
	optPar->ParseLine("_Set tenEighty 1080");
	// Saving...
	ASSERT_NO_THROW(optPar->ParseLine("_Save fourThirteen Testing")) << "Threw while saving.";
	ASSERT_NO_THROW(optPar->ParseLine("_Save tenEighty Testing")) << "Threw while saving.";

	// Restarting...
	optPar.emplace(GenerateTestingInstance());

	// Loading...
	ASSERT_NO_THROW(optPar->ParseLine("_Load Testing")) << "Threw while loading.";

	// Verifying...
	ASSERT_NO_THROW(optPar->ParseLine("fourThirteen"));
	ASSERT_DOUBLE_EQ(413.0, optPar->GetLitMan().GetLast());

	ASSERT_NO_THROW(optPar->ParseLine("tenEighty"));
	ASSERT_DOUBLE_EQ(1080.0, optPar->GetLitMan().GetLast());

	// Clean up by opening the file without the append flag.
	std::ofstream{Ser::GetPath() / "Testing.txt"};
}

PARSER_TEST(Serializing_a_function) {
	auto optPar = std::optional{GenerateTestingInstance()};

	optPar->ParseLine("_Func Add a b");
	optPar->ParseLine("    _Return a b +;");
	optPar->ParseLine("_Func Sub a b");
	optPar->ParseLine("    _Return a b -;");
	// Saving...
	ASSERT_NO_THROW(optPar->ParseLine("_Save Add Testing;")) << "Threw while saving.";
	ASSERT_NO_THROW(optPar->ParseLine("_Save Sub Testing;")) << "Threw while saving.";

	// Restarting...
	optPar.emplace(GenerateTestingInstance());

	// Loading...
	ASSERT_NO_THROW(optPar->ParseLine("_Load Testing;")) << "Threw while loading.";

	// Verifying...
	ASSERT_NO_THROW(optPar->ParseLine("5 10 Add;"));
	ASSERT_DOUBLE_EQ(5 + 10, optPar->GetLitMan().GetLast());

	ASSERT_NO_THROW(optPar->ParseLine("5 10 Sub;"));
	ASSERT_DOUBLE_EQ(5 - 10, optPar->GetLitMan().GetLast());

	// Clean up by opening the file without the append flag.
	std::ofstream{Ser::GetPath() / "Testing.txt"};
}