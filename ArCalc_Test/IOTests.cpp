#include "pch.h"

#include <Util/IO.h>

#define IO_TEST(_testName) TEST_F(IOTests, _testName)

using namespace ArCalc;

class IOTests : public testing::Test {
};

IO_TEST(Output) {
	constexpr auto OutputStr{"yay!"};

	std::stringstream ss{};
	IO::Output(ss, OutputStr);
	ASSERT_EQ(OutputStr, ss.str());
}


IO_TEST(Input_using_return_value) {
	constexpr auto InputStr{"yay!"};
	std::stringstream ss{InputStr};
	
	auto const testString{IO::Input<std::string>(ss)};
	ASSERT_EQ(InputStr, testString);
}

IO_TEST(Input_using_out_ptr) {
	constexpr auto InputStr{"yay!"};

	std::stringstream ss{InputStr};
	std::string testString{};
	IO::Input(ss, &testString);
	ASSERT_EQ(InputStr, testString);
}

IO_TEST(Input_return_and_out_ptr_equivalance) {
	constexpr auto InputStr{"yay! yay!"};

	std::string strUsingOutPtr{};
	{
		std::stringstream ss{InputStr};
		IO::Input(ss, &strUsingOutPtr);
	}

	std::string strUsingReturn{};
	{
		std::stringstream ss{InputStr};
		strUsingReturn = IO::Input<std::string>(ss);
	}

	ASSERT_EQ(strUsingReturn, strUsingOutPtr);
}

IO_TEST(GetLine) {
	constexpr auto LinePrefix{"Line #"};
	constexpr auto LineCount{5U};

	std::stringstream ss{};
	for (auto const i : view::iota(0U, LineCount)) {
		IO::Output(ss, LinePrefix + std::to_string(i) + '\n');
	}

	auto const wholeThing{ss.str()};

	for (auto const i : view::iota(0U, LineCount)) {
		ASSERT_EQ(LinePrefix + std::to_string(i), IO::GetLine(ss));
	}
}

IO_TEST(Print) {
	std::stringstream ss{};
	ASSERT_ANY_THROW(IO::Print(ss, "{} {}", "Frist argument, no second argument"));

	ss.sync();
	ss.clear();
	auto const resStr{std::format("{} {} {} {} {}", "Hello", ',', 123, 1.23, 12.3f)};
	ASSERT_NO_THROW(IO::Print(ss, "{} {} {} {} {}", "Hello", ',', 123, 1.23, 12.3f));
	ASSERT_EQ(resStr, ss.str());
}

IO_TEST(Stream_to_string_keeping_trailing_nulls) {
	constexpr auto MyString{"Hello, baby!\nthis is supposed to be a long\n\t\t\tstring."};
	std::stringstream ss{MyString};
	ASSERT_EQ(MyString, IO::IStreamToString(ss, true));
}

IO_TEST(Stream_to_string_trimming_trailing_nulls) {
	constexpr auto MyString{"Hello, baby!"};
	constexpr auto Nulls{"\0\0\0\0\0\0\0\0\0"};
	std::stringstream ss{MyString + std::string{Nulls}};
	ASSERT_EQ(MyString, IO::IStreamToString(ss));
}

IO_TEST(IStream_size) {
	constexpr auto MyString{"Hello, baby!"};
	std::stringstream ss{MyString};
	ASSERT_EQ(std::string_view{MyString}.size(), IO::IStreamSize(ss));
}