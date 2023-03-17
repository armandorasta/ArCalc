#include "pch.h"

#include <../../ArCalc/Source/Parser.h>
#include "Util/Str.h"
#include <Util/IO.h>

#define PARSER_TEST(_testName) TEST_F(ParserTests, _testName)

using namespace ArCalc;

class ParserTests : public testing::Test {
};

PARSER_TEST(Semicolon_at_end_of_line) {
	constexpr auto code = R"(5 10 +
							 _List
							 _Func Max a b
							 _Set ret a
							 _If a b <
							 _Set ret b
							 _Return ret)";
	
	auto const lines{Str::SplitOn(code, "\n")};
	std::stringstream bullshitStream{};
	std::optional<Parser> par{Parser{}};
	par->SetOutStream(bullshitStream);
	for (auto const& line : lines) {
		ASSERT_NO_THROW(par->ParseLine(line));
		ASSERT_FALSE(par->IsLineEndsWithSemiColon());
	}

	par.emplace(Parser{});             // "Reset"
	par->SetOutStream(bullshitStream); // ^^^^^^

	for (auto const& line : lines) {
		ASSERT_NO_THROW(par->ParseLine(line + ";"));
		ASSERT_TRUE(par->IsLineEndsWithSemiColon());
	}
}

PARSER_TEST(Normal_expression_and_Last_keyword) {
	Parser par{};
	std::stringstream ss{};
	par.SetOutStream(ss);

	par.ParseLine("5 10 +");
	ASSERT_DOUBLE_EQ(15.0, par.GetLast());
}

PARSER_TEST(Set_statement) {
	Parser par{};
	std::stringstream ss{};
	par.SetOutStream(ss);

	par.ParseLine("_Set x 5;");
	ASSERT_TRUE(par.IsLineEndsWithSemiColon());
	ASSERT_TRUE(par.IsVisible("x"));
	ASSERT_DOUBLE_EQ(5.0, par.Deref("x"));
}