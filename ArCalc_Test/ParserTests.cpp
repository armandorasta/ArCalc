#include "pch.h"

#include <../../ArCalc/Source/Parser.h>
#include "Util/Str.h"
#include <Util/IO.h>
#include <Util/Random.h>

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
	ASSERT_DOUBLE_EQ(15.0, par.GetLast());
}

PARSER_TEST(Set_statement) {
	auto par{GenerateTestingInstance()};

	par.ParseLine("_Set x 5;");
	ASSERT_TRUE(par.IsLineEndsWithSemiColon());
	ASSERT_TRUE(par.IsVisible("x"));
	ASSERT_DOUBLE_EQ(5.0, par.Deref("x"));
}

PARSER_TEST(Defining_a_function) {
	constexpr std::string_view Lines[]{
		"_Func Add a b\n",
		"_Set sum a b +\n",
		"_Return sum\n",
	};

	Parser par{GenerateTestingInstance()};
	ASSERT_FALSE(par.IsParsingFunction());
	for (auto const line : Lines | view::take(std::size(Lines) - 1)) {
		ASSERT_NO_THROW(par.ParseLine(line));
		ASSERT_TRUE(par.IsParsingFunction());
	}

	ASSERT_NO_THROW(par.ParseLine(Lines[std::size(Lines) - 1]));
	ASSERT_FALSE(par.IsParsingFunction());
	ASSERT_TRUE(par.IsFunction("Add"));
}

PARSER_TEST(Executing_a_function) {
	constexpr std::string_view FuncName{"Sub"};
	
	std::string DefLines[]{
		std::format("_Func {} a b\n", FuncName),
		"_Set sum a b -\n",
		"_Return sum\n",
	};

	Parser par{GenerateTestingInstance()};
	for (auto const& line : DefLines) {
		par.ParseLine(line); // Already tested above.
	}
	
	for (auto const lhs : view::iota(-10, 10)) {
		auto const rhs{Random::Double(-10.0, 10.0)};

		// Here we are just regular numbers as arguments.
		ASSERT_NO_THROW(par.ParseLine(std::format("{} {} {}", lhs, rhs, FuncName)));
		ASSERT_DOUBLE_EQ(lhs - rhs, par.GetLast());

		// Here we are using variables as arguments
		ASSERT_NO_THROW(par.ParseLine(std::format("_Set lhs {}", lhs)));
		ASSERT_NO_THROW(par.ParseLine(std::format("_Set rhs {}", rhs)));
		ASSERT_NO_THROW(par.ParseLine(std::format("{} {} {}", lhs, rhs, FuncName)));
		ASSERT_DOUBLE_EQ(lhs - rhs, par.GetLast());
	}
}

PARSER_TEST(Selection_statements) {
	auto const stroll = [&](auto caseName, auto code) {
		Parser par{GenerateTestingInstance()};
		
		// Define the function.
		for (auto const lines{Str::SplitOn(code, "\n")}; auto const& line : lines) {
			par.ParseLine(line);
		}

		for (auto const lhs : view::iota(-10, 10)) {
			for (auto const rhs : view::iota(-10, 10)) {
				ASSERT_NO_THROW(par.ParseLine(std::format("{} {} {}", lhs, rhs, "Max"))) << caseName;
				ASSERT_DOUBLE_EQ(std::max(lhs, rhs), par.GetLast()) << caseName;
			}
		}
	};

	stroll("One branch - same line",
		"_Func Max a b\n"
		"_Set ret a;\n"
		"_If b a > _Set ret b;\n"
		"_Return ret;\n"
	);

	stroll("One branch - same line",
		"_Func Max a b\n"
		"_Set ret a;\n"
		"_If b a >\n"
		"    _Set ret b;\n"
		"_Return ret;\n"
	);

	stroll("If else - same same",
		"_Func Max a b\n"
		"_If a b > _Set ret a;\n"
		"_Else     _Set ret b;\n"
		"_Return ret;\n"
	);

	stroll("If else - same next",
		"_Func Max a b\n"
		"_If a b > _Set ret a;\n"
		"_Else\n"
		"    _Set ret b;\n"
		"_Return ret;\n"
	);

	stroll("If else - next same",
		"_Func Max a b\n"
		"_If a b >\n"
		"    _Set ret a;\n"
		"_Else _Set ret b;\n"
		"_Return ret;\n"
	);

	stroll("If else - next next",
		"_Func Max a b\n"
		"_If a b >\n"
		"    _Set ret a;\n"
		"_Else\n"
		"    _Set ret b;\n"
		"_Return ret;\n"
	);
}