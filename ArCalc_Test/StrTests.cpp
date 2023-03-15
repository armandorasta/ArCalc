#include "pch.h"

#include <Util/Util.h>
#include <Util/Str.h>
#include <Util/Random.h>

using namespace ArCalc;

class StrTests : public testing::Test {
};

TEST_F(StrTests, TrimLeft) {
	std::string const str{"  \t\t  \t Hello"};
	ASSERT_EQ(Str::TrimLeft(str), "Hello");
}

TEST_F(StrTests, TrimRight) {
	std::string const str{"Hello   \t\t  \t "};
	ASSERT_EQ(Str::TrimRight(str), "Hello");
}

TEST_F(StrTests, Trim) {
	std::string const str{"   \t\t  \t Hello   \t\t  \t "};
	ASSERT_EQ(Str::Trim(str), "Hello");
	ASSERT_EQ(Str::Trim(str), Str::TrimLeft(Str::TrimRight(str)));
	ASSERT_EQ(Str::Trim(str), Str::TrimRight(Str::TrimLeft(str)));
}

TEST_F(StrTests, SplitOn_with_chaining) {
	constexpr std::size_t MaxDelimCount{10};

	std::vector<std::string> const saperatedString{"hello", "this", "is", "the", "resulting", "string"};
	std::vector<std::string> delims{", +-", ".*/*-", "+646\t54", "*-+*\n564", "\n \t"};

	std::vector<std::string> strings{};
	strings.reserve(delims.size());
	for (auto& delim : delims) {
		std::string joinedString{};
		for (auto const& str : saperatedString) {
			joinedString += str;

			// Randomly skip adding the delimeter at the end of the string.
			if (&str == &saperatedString.back() && Random::Bool()) {
				continue;
			}

			for (size_t i{}; i < MaxDelimCount; ++i) {
				range::shuffle(delim, Random::Engine());
				joinedString += delim;
			}
		}
		strings.push_back(std::exchange(joinedString, {}));
	}

	for (size_t i : view::iota(0U, delims.size())) {
		// Empty strings may not be touched.
		ASSERT_EQ(Str::SplitOn("", delims[i]), std::vector<std::string>{""}); 
		ASSERT_EQ(Str::SplitOn(strings[i], delims[i]), saperatedString);
	}
}

TEST_F(StrTests, SplitOn_no_chaining) {
	std::vector<std::pair<std::string, std::vector<std::string>>> strings{
		{"Hello*baby!", {"Hello", "baby!"}},
		{"Hello++*baby!", {"Hello", "", "", "baby!"}},
		{"Hello+*+*baby!+", {"Hello", "", "", "", "baby!"}},
		{"Hello+*+*baby!+*+*", {"Hello", "", "", "", "baby!", "", "", ""}},
	};
	std::string delim{"+*"};
	for (auto const& [str, vec] : strings) {
		range::shuffle(delim, Random::Engine());
		ASSERT_EQ(Str::SplitOn(str, delim, false), vec);
	}
}

TEST_F(StrTests, GetFirstTokenIndices) {
	using Str::Secret::GetFirstTokenIndices;

	auto const str{"    hello, this is my string"};
	auto const [startIndex, endIndex] {GetFirstTokenIndices(str)};
	ASSERT_EQ(4U, startIndex) << "Invalid start index";
	// End index is 1 past the index of the last character character in the token.
	ASSERT_EQ(10U, endIndex) << "Invalid end index";
}

TEST_F(StrTests, Name_mangling) {
	constexpr auto paramName{"my_param"};

	for (auto const i : view::iota(0U, 10U)) {
		auto const res{Str::Demangle(Str::Mangle(paramName, i))};
		ASSERT_EQ(i, res.Index) << "Index was fucked up";
		ASSERT_EQ(paramName, res.PackName) << "Paramter pack name was fucked up";
	}
}
