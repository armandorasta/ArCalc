#include "pch.h"

#include <Util/FunctionManager.h>

#define FUNMAN_TEST(_testName) TEST_F(FunctionManagerTests, _testName)

using namespace ArCalc;

class FunctionManagerTests : public testing::Test {
protected:
	constexpr static auto sc_FuncName{"Shit"};
	constexpr static size_t sc_ParamCount{3};
	constexpr static size_t sc_LineNumber{35};

	static_assert(sizeof sc_FuncName);
	static_assert(sc_ParamCount > 0U);
	static_assert(sc_LineNumber > 0U);
};

FUNMAN_TEST(Defining_a_function) {
	FunctionManager funMan{};
	ASSERT_FALSE(funMan.IsDefinationInProgress());

	funMan.BeginDefination(sc_FuncName, sc_LineNumber);
	ASSERT_TRUE(funMan.IsDefinationInProgress());

	for (constexpr auto ParamPerfix{"param"}; auto i : view::iota(0U, sc_ParamCount)) {
		funMan.AddParam(std::format("{}{}", ParamPerfix, i));
		ASSERT_TRUE(funMan.IsDefinationInProgress());

		// Doublicate parameter name, must throw before perfoming action!
		ASSERT_ANY_THROW(funMan.AddParam(std::format("{}{}", ParamPerfix, i))); 
	}

	ASSERT_FALSE(funMan.IsCurrFuncVariadic());
	funMan.AddVariadicParam(std::format("param{}", sc_ParamCount));
	ASSERT_TRUE(funMan.IsCurrFuncVariadic());

	// Empty functions don't make any sense here and are not allowed.
	// EndDefination is expected to throw before performing any action.
	ASSERT_ANY_THROW(funMan.EndDefination());
	funMan.AddCodeLine("a 2 + 3 *");
	ASSERT_TRUE(funMan.IsDefinationInProgress());

	funMan.SetReturnType(FuncReturnType::Number);
	ASSERT_TRUE(funMan.IsDefinationInProgress());
	ASSERT_EQ(funMan.GetCurrentReturnType(), FuncReturnType::Number);

	funMan.SetReturnType(FuncReturnType::None); // Changing the return type must be allowed.
	ASSERT_TRUE(funMan.IsDefinationInProgress());
	ASSERT_EQ(funMan.GetCurrentReturnType(), FuncReturnType::None);

	// The defination requires at least 1 parameter and 1 line of code.
	funMan.EndDefination();
	ASSERT_FALSE(funMan.IsDefinationInProgress());

	ASSERT_TRUE(funMan.IsDefined(sc_FuncName));

	auto const func{funMan.Get(sc_FuncName)};
	ASSERT_EQ(func.Params.size(), sc_ParamCount + 1); // Paramter pack counts as one argument.
	ASSERT_EQ(func.HeaderLineNumber, sc_LineNumber);
	ASSERT_EQ(func.ReturnType, FuncReturnType::None);
}

FUNMAN_TEST(Double_defination) {
	FunctionManager funMan{};

	funMan.BeginDefination(sc_FuncName, sc_LineNumber);
	funMan.AddParam("a");
	funMan.AddCodeLine("a 2 *");
	funMan.EndDefination();

	ASSERT_ANY_THROW(funMan.BeginDefination(sc_FuncName, sc_LineNumber + 3));
}

FUNMAN_TEST(Function_not_variadic_and_returns_number) {
	FunctionManager funMan{};

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

FUNMAN_TEST(Function_variadic_and_returns_none) {
	FunctionManager funMan{};

	funMan.BeginDefination(sc_FuncName, sc_LineNumber);
	funMan.SetReturnType(FuncReturnType::None);
	for (auto i : view::iota(0U, sc_ParamCount)) {
		funMan.AddParam(std::format("param{}", i));
	}
	funMan.AddVariadicParam(std::format("param{}", sc_ParamCount));
	funMan.AddCodeLine("param0 param1 param2 + +");
	funMan.EndDefination();

	auto const func{funMan.Get(sc_FuncName)};
	ASSERT_TRUE(funMan.IsDefined(sc_FuncName));
	ASSERT_EQ(func.Params.size(), sc_ParamCount + 1); // Paramter pack counts as one argument.
	ASSERT_TRUE(func.IsVariadic);
	ASSERT_EQ(func.ReturnType, FuncReturnType::None);
	ASSERT_EQ(func.HeaderLineNumber, sc_LineNumber);
}