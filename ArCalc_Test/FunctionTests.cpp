#include "pch.h"

#include <Util/Function.h>

using namespace ArCalc;

class FunctionTests : public testing::Test {
public:
	void SetUp() override {
	}

	void TearDown() override {
		Function::Reset();
	}

protected:
	constexpr static auto sc_FuncName{"Shit"};
	constexpr static size_t sc_ParamCount{3};
	constexpr static size_t sc_LineNumber{35};

	static_assert(sizeof sc_FuncName);
	static_assert(sc_ParamCount > 0U);
	static_assert(sc_LineNumber > 0U);
};

TEST_F(FunctionTests, Defining_a_function) {
	ASSERT_FALSE(Function::IsDefinationInProgress());
	
	Function::BeginDefination(sc_FuncName, sc_LineNumber);
	ASSERT_TRUE(Function::IsDefinationInProgress());
	
	for (constexpr auto ParamPerfix{"param"}; auto i : view::iota(0U, sc_ParamCount)) {
		Function::AddParam(std::format("{}{}", ParamPerfix, i));
		ASSERT_TRUE(Function::IsDefinationInProgress());

		// Doublicate parameter name, must throw before perfoming action!
		ASSERT_ANY_THROW(Function::AddParam(std::format("{}{}", ParamPerfix, i))); 
	}

	ASSERT_FALSE(Function::IsCurrFuncVariadic());
	Function::AddVariadicParam(std::format("param{}", sc_ParamCount));
	ASSERT_TRUE(Function::IsCurrFuncVariadic());

	// Empty functions don't make any sense here and are not allowed.
	// EndDefination is expected to throw before performing any action.
	ASSERT_ANY_THROW(Function::EndDefination());
	Function::AddCodeLine("a 2 + 3 *");
	ASSERT_TRUE(Function::IsDefinationInProgress());

	Function::SetReturnType(FuncReturnType::Number);
	ASSERT_TRUE(Function::IsDefinationInProgress());
	ASSERT_EQ(Function::GetCurrentReturnType(), FuncReturnType::Number);
	
	Function::SetReturnType(FuncReturnType::None); // Changing the return type must be allowed.
	ASSERT_TRUE(Function::IsDefinationInProgress());
	ASSERT_EQ(Function::GetCurrentReturnType(), FuncReturnType::None);

	// The defination requires at least 1 parameter and 1 line of code.
	Function::EndDefination();
	ASSERT_FALSE(Function::IsDefinationInProgress());

	ASSERT_TRUE(Function::IsDefined(sc_FuncName));
	ASSERT_EQ(Function::ParamCountOf(sc_FuncName), sc_ParamCount + 1); // Paramter pack counts as one argument.
	ASSERT_EQ(Function::HeaderLineNumberOf(sc_FuncName), sc_LineNumber);
	ASSERT_EQ(Function::ReturnTypeOf(sc_FuncName), FuncReturnType::None);
}

TEST_F(FunctionTests, Double_defination) {
	Function::BeginDefination(sc_FuncName, sc_LineNumber);
	Function::AddParam("a");
	Function::AddCodeLine("a 2 *");
	Function::EndDefination();

	ASSERT_ANY_THROW(Function::BeginDefination(sc_FuncName, sc_LineNumber + 3));
}

TEST_F(FunctionTests, Function_not_variadic_and_returns_number) {
	Function::BeginDefination(sc_FuncName, sc_LineNumber);
	Function::SetReturnType(FuncReturnType::Number);
	for (auto i : view::iota(0U, sc_ParamCount)) {
		Function::AddParam("param" + std::to_string(i));
	}
	Function::AddCodeLine("param0 param1 param2 + +");
	Function::EndDefination();

	ASSERT_TRUE(Function::IsDefined(sc_FuncName));
	ASSERT_EQ(Function::ParamCountOf(sc_FuncName), sc_ParamCount);
	ASSERT_FALSE(Function::IsVariadic(sc_FuncName));
	ASSERT_EQ(Function::ReturnTypeOf(sc_FuncName), FuncReturnType::Number);
	ASSERT_EQ(Function::HeaderLineNumberOf(sc_FuncName), sc_LineNumber);
}

TEST_F(FunctionTests, Function_variadic_and_returns_none) {
	Function::BeginDefination(sc_FuncName, sc_LineNumber);
	Function::SetReturnType(FuncReturnType::None);
	for (auto i : view::iota(0U, sc_ParamCount)) {
		Function::AddParam(std::format("param{}", i));
	}
	Function::AddVariadicParam(std::format("param{}", sc_ParamCount));
	Function::AddCodeLine("param0 param1 param2 + +");
	Function::EndDefination();

	ASSERT_TRUE(Function::IsDefined(sc_FuncName));
	ASSERT_EQ(Function::ParamCountOf(sc_FuncName), sc_ParamCount + 1); // Paramter pack counts as one argument.
	ASSERT_TRUE(Function::IsVariadic(sc_FuncName));
	ASSERT_EQ(Function::ReturnTypeOf(sc_FuncName), FuncReturnType::None);
	ASSERT_EQ(Function::HeaderLineNumberOf(sc_FuncName), sc_LineNumber);
}