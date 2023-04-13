#include "pch.h"

#include <Util/FunctionManager.h>
#include <Util/IO.h>
#include <Exception/ArCalcException.h>

#define FUNMAN_TEST(_testName) TEST_F(FunctionManagerTests, _testName)

using namespace ArCalc;

class FunctionManagerTests : public testing::Test {
public:
	FunctionManager GenerateTestingInstance() {
		auto funMan = FunctionManager{std::cout};
		funMan.ToggleOutput();
		return funMan;
	}

protected:
	constexpr static auto sc_FuncName{"Shit"};
	constexpr static size_t sc_ParamCount{3};
	constexpr static size_t sc_LineNumber{35};

	static_assert(sizeof sc_FuncName);
	static_assert(sc_ParamCount > 0U);
	static_assert(sc_LineNumber > 0U);
};

FUNMAN_TEST(Defining_a_function) {
	auto funMan{GenerateTestingInstance()};
	ASSERT_FALSE(funMan.IsDefinationInProgress());

	ASSERT_NO_THROW(funMan.BeginDefination(sc_FuncName, sc_LineNumber));
	ASSERT_TRUE(funMan.IsDefinationInProgress());

	for (constexpr auto ParamPerfix{"param"}; auto i : view::iota(0U, sc_ParamCount)) {
		if (i % 2) {
			ASSERT_NO_THROW(funMan.AddParam(std::format("{}{}", ParamPerfix, i)));
		} else {
			ASSERT_NO_THROW(funMan.AddRefParam(std::format("{}{}", ParamPerfix, i)));
		}
		ASSERT_TRUE(funMan.IsDefinationInProgress());

		// Doublicate parameter name, must throw before perfoming action!
		ASSERT_ANY_THROW(funMan.AddParam(std::format("{}{}", ParamPerfix, i))); 
	}

	// ASSERT_FALSE(funMan.IsCurrFuncVariadic());
	// ASSERT_NO_THROW(funMan.AddVariadicParam(std::format("param{}", sc_ParamCount)));
	// ASSERT_TRUE(funMan.IsCurrFuncVariadic());

	// Empty functions don't make any sense here and are not allowed.
	// EndDefination is expected to throw before performing any action.
	ASSERT_ANY_THROW(funMan.EndDefination());
	ASSERT_NO_THROW(funMan.AddCodeLine("a 2 + 3 *"));
	ASSERT_TRUE(funMan.IsDefinationInProgress());

	ASSERT_NO_THROW(funMan.SetReturnType(FuncReturnType::Number));
	ASSERT_TRUE(funMan.IsDefinationInProgress());
	ASSERT_EQ(funMan.CurrReturnType(), FuncReturnType::Number);

	ASSERT_NO_THROW(funMan.SetReturnType(FuncReturnType::None)); // Changing the return type must be allowed.
	ASSERT_TRUE(funMan.IsDefinationInProgress());
	ASSERT_EQ(funMan.CurrReturnType(), FuncReturnType::None);

	// The defination requires at least 1 parameter and 1 line of code.
	ASSERT_NO_THROW(funMan.EndDefination());
	ASSERT_FALSE(funMan.IsDefinationInProgress());

	ASSERT_TRUE(funMan.IsDefined(sc_FuncName));

	auto const func{funMan.Get(sc_FuncName)};
	ASSERT_EQ(func.Params.size(), sc_ParamCount /*+ 1*/); // Paramter pack counts as one argument.
	ASSERT_EQ(func.HeaderLineNumber, sc_LineNumber);
	ASSERT_EQ(func.ReturnType, FuncReturnType::None);
}

FUNMAN_TEST(Double_defination) {
	auto funMan{GenerateTestingInstance()};

	funMan.BeginDefination(sc_FuncName, sc_LineNumber);
	funMan.AddParam("a");
	funMan.AddCodeLine("a 2 *");
	funMan.EndDefination();

	ASSERT_ANY_THROW(funMan.BeginDefination(sc_FuncName, sc_LineNumber + 3));
}

FUNMAN_TEST(Function_not_variadic_and_returns_number) {
	auto funMan{GenerateTestingInstance()};

	funMan.BeginDefination(sc_FuncName, sc_LineNumber);
	funMan.SetReturnType(FuncReturnType::Number);
	for (auto i : view::iota(0U, sc_ParamCount)) {
		funMan.AddParam("param" + std::to_string(i));
	}
	funMan.AddCodeLine("param0 param1 param2 + +");
	funMan.EndDefination();

	auto const func{funMan.Get(sc_FuncName)};
	ASSERT_TRUE(funMan.IsDefined(sc_FuncName));
	ASSERT_EQ(func.Params.size(), sc_ParamCount);
	ASSERT_FALSE(func.IsVariadic);
	ASSERT_EQ(func.ReturnType, FuncReturnType::Number);
	ASSERT_EQ(func.HeaderLineNumber, sc_LineNumber);
}

FUNMAN_TEST(Function_non_variadic_and_returns_none) {
	auto funMan{GenerateTestingInstance()};

	funMan.BeginDefination(sc_FuncName, sc_LineNumber);
	funMan.SetReturnType(FuncReturnType::None);
	for (auto i : view::iota(0U, sc_ParamCount)) {
		funMan.AddParam(std::format("param{}", i));
	}
	funMan.AddCodeLine("param0 param1 param2 + +");
	funMan.EndDefination();

	auto const func{funMan.Get(sc_FuncName)};
	ASSERT_TRUE(funMan.IsDefined(sc_FuncName));
	ASSERT_EQ(func.Params.size(), sc_ParamCount);
	// ASSERT_TRUE(func.IsVariadic);
	ASSERT_EQ(func.ReturnType, FuncReturnType::None);
	ASSERT_EQ(func.HeaderLineNumber, sc_LineNumber);
}

FUNMAN_TEST(Passing_by_reference) {
	auto funMan{GenerateTestingInstance()};

	funMan.BeginDefination(sc_FuncName, sc_LineNumber);
	funMan.AddParam("value");
	funMan.AddRefParam("var0");
	funMan.AddRefParam("var1");
	funMan.AddCodeLine("_Set var0 value;");
	funMan.AddCodeLine("_Set var1 value;");
	funMan.AddCodeLine("_Return;");
	funMan.SetReturnType(FuncReturnType::None);
	funMan.EndDefination();

	auto& func{funMan.Get(sc_FuncName)};

	double var0{};
	double var1{};
	constexpr auto ExpectedValue{5.0};
	func.Params[0].PushValue(ExpectedValue);
	func.Params[1].SetRef(&var0);
	func.Params[2].SetRef(&var1);

	ASSERT_NO_THROW(funMan.CallFunction(sc_FuncName));
	ASSERT_EQ(ExpectedValue, var0);
	ASSERT_EQ(ExpectedValue, var1);
}