#include "pch.h"

#include <Util/IO.h>
#include <Util/LiteralManager.h>
#include <Util/FunctionManager.h>

#define SER_TEST(_testName) TEST_F(SerializationTests, _testName)

using namespace ArCalc;

class SerializationTests : public testing::Test {
};

SER_TEST(Serializing_a_math_constant) {
	constexpr auto Name{"_Hello"};
	constexpr auto Value{413.0};
	constexpr auto RepCount{2U};

	std::stringstream outputSS{};
	LiteralManager litMan{outputSS};
	litMan.Add(Name, Value);

	std::stringstream ss{};
	for (auto const nSer : view::iota(0U, RepCount)) {
		ASSERT_NO_THROW(litMan.Serialize(Name, ss)) << nSer;
	}

	for (auto const nSer : view::iota(0U, RepCount)) {
		(void) IO::Input<char>(ss); // The C will not be consumed by the deserializer.
		litMan.Deserialize(ss);

		ASSERT_TRUE(litMan.IsVisible(Name)) << nSer;
		ASSERT_EQ(Value, *litMan.Get(Name)) << nSer;
	}
}

SER_TEST(Serializing_a_function) {
	constexpr auto RepCount{2U};
	constexpr auto Name{"_MyFunc"};
	
	std::stringstream outputSS{};
	auto funMan = FunctionManager{outputSS};
	funMan.BeginDefination(Name, 0U);
	funMan.AddParam("a");
	funMan.AddRefParam("c");
	funMan.AddCodeLine("_Set c a;");
	funMan.AddCodeLine("_Return;");
	funMan.EndDefination();
	auto const func{funMan.Get(Name)};

	std::stringstream serializationSS{};
	for (auto const nSer : view::iota(0U, RepCount)) {
		ASSERT_NO_THROW(funMan.Serialize(Name, serializationSS)) << nSer;
	}
	
	for (auto const nSer : view::iota(0U, RepCount)) { 
		// The F will not be consumed by the deserializer.
		ASSERT_NO_THROW((void) IO::Input<char>(serializationSS)); 
		ASSERT_NO_THROW(funMan.Deserialize(serializationSS));


		ASSERT_NO_THROW(funMan.Get(Name)); // Check
		auto const deserializedVer{funMan.Get(Name)};
		ASSERT_EQ(func.ReturnType, deserializedVer.ReturnType) << nSer;

		// Parameters
		for (auto const nParam : view::iota(0U, func.Params.size())) {
			auto const& original{func.Params[nParam]};
			auto const& deserialized{deserializedVer.Params[nParam]};

			auto const errMsg{std::format("iteration: {}, parameter index: {}", nSer, nParam)};
			ASSERT_EQ(original.GetName(), deserialized.GetName()) << errMsg;
			ASSERT_EQ(original.IsPassedByRef(), deserialized.IsPassedByRef()) << errMsg;
			// ASSERT_EQ(original.IsParameterPack(), deserialized.IsParameterPack()) << errMsg;
		}

		// Lines. I know I could just use the std::vector == operator, but this gives me 
		// more information.
		ASSERT_EQ(func.CodeLines.size(), deserializedVer.CodeLines.size());
		for (auto const nCodeLine : view::iota(0U, func.CodeLines.size())) {
			ASSERT_EQ(func.CodeLines[nCodeLine], deserializedVer.CodeLines[nCodeLine])
				<< std::format("iteration: {}, line index: {}", nSer, nCodeLine);
		}
	}
}