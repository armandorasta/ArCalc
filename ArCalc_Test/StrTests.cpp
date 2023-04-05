#include "pch.h"

#include <Util/Util.h>
#include <Util/Str.h>
#include <Util/Random.h>

using namespace ArCalc;

#define STR_TEST(_testName) TEST_F(StrTests, _testName)

class StrTests : public testing::Test {
};

STR_TEST(TrimLeft) {
	std::string const str{"  \t\t  \t Hello"};
	ASSERT_EQ(Str::TrimLeft(str), "Hello");
}

STR_TEST(TrimRight) {
	std::string const str{"Hello   \t\t  \t "};
	ASSERT_EQ(Str::TrimRight(str), "Hello");
}

STR_TEST(Trim) {
	std::string const str{"   \t\t  \t Hello   \t\t  \t "};
	ASSERT_EQ(Str::Trim(str), "Hello");
	ASSERT_EQ(Str::Trim(str), Str::TrimLeft(Str::TrimRight(str)));
	ASSERT_EQ(Str::Trim(str), Str::TrimRight(Str::TrimLeft(str)));
}

STR_TEST(SplitOn_with_chaining) {
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

STR_TEST(SplitOn_no_chaining) {
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

STR_TEST(GetFirstTokenIndices) {
	using Str::Secret::GetFirstTokenIndices;

	auto const str{"    hello, this is my string"};
	auto const [startIndex, endIndex] {GetFirstTokenIndices(str)};
	ASSERT_EQ(4U, startIndex) << "Invalid start index";
	// End index is 1 past the index of the last character character in the token.
	ASSERT_EQ(10U, endIndex) << "Invalid end index";
}

STR_TEST(Name_mangling) {
	constexpr auto paramName{"my_param"};

	for (auto const i : view::iota(0U, 10U)) {
		auto const res{Str::Demangle(Str::Mangle(paramName, i))};
		ASSERT_EQ(i, res.Index) << "Index was fucked up";
		ASSERT_EQ(paramName, res.PackName) << "Paramter pack name was fucked up";
	}
}

STR_TEST(String_to_int) {
	for (auto const n : view::iota(-10, 10)) {
		auto res{std::to_string(n)};

		for (auto const i : view::iota(0U, 5U)) {
			if (i % 2 == 0) {
				switch (Random::Int(2)) {
				case 1: res.push_back(' ' ); break;
				case 2: res.push_back('\t'); break;
				default: /*Aka 0*/           break;
				}
			} else {
				switch (Random::Int(3)) {
				case 1: res = ' '  + res;    break;
				case 2: res = '\t' + res;    break;
				default: /*Aka 0*/           break;
				}
			}

			ASSERT_EQ(n, Str::StringToInt<decltype(n)>(res));
			if (n < 0) { // Parsing negative number using unsigned type
				ASSERT_ANY_THROW(Str::StringToInt<std::make_unsigned_t<decltype(n)>>(res));
			}

			// Invalid characters.
			//auto copy{res};
			//copy[copy.size() / 2] = 'g';
			//EXPECT_ANY_THROW(Str::StringToInt<decltype(n)>(copy));

			//copy[copy.size() / 2] = '#';
			//ASSERT_ANY_THROW(Str::StringToInt<decltype(n)>(copy));
		}
	}
}

STR_TEST(String_to_float) {
	for (auto const n : view::iota(-10, 10)) {
		auto resFloat{std::to_string(n / 2.f)};
		auto resInt{std::to_string(n / 2)};

		for (auto const i : view::iota(0U, 5U)) {
			if (i % 2 == 0) {
				switch (Random::Int(2)) {
				case 1: resFloat.push_back(' '); break;
				case 2: resFloat.push_back('\t'); break;
				default: /*Aka 0*/                break;
				}
			}
			else {
				switch (Random::Int(3)) {
				case 1: resFloat = ' ' + resFloat;    break;
				case 2: resFloat = '\t' + resFloat;    break;
				default: /*Aka 0*/                     break;
				}
			}

			ASSERT_FLOAT_EQ(n / 2.f, Str::StringToFloat<float>(resFloat));
			ASSERT_FLOAT_EQ(static_cast<float>(n / 2), Str::StringToFloat<float>(resInt));

			// Invalid characters.
			/*auto fCopy{resFloat};
			fCopy[fCopy.size() / 2] = 'g';
			EXPECT_ANY_THROW(Str::StringToInt<decltype(n)>(fCopy));

			fCopy[fCopy.size() / 2] = '#';
			ASSERT_ANY_THROW(Str::StringToInt<decltype(n)>(fCopy));

			auto iCopy{resInt};
			iCopy[iCopy.size() / 2] = 'g';
			EXPECT_ANY_THROW(Str::StringToInt<decltype(n)>(iCopy));

			iCopy[iCopy.size() / 2] = '#';
			ASSERT_ANY_THROW(Str::StringToInt<decltype(n)>(iCopy));*/
		}
	}
}