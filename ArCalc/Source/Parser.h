#pragma once

#include "Core.h"
#include "Util/Keyword.h"
#include "Util/FunctionManager.h"

namespace ArCalc {
	/*
		_Set myVar 5 10 *
		mVar = 15

		_Set myOtherVar 2 2 +
		myOtherVar = 4

		myVar
		15

		_List
		myVar = 15
		myOtherVar = 4

		_List myV
		myVar = 15

		myVar myOtherVar *
		60

		_Set thirdVar = myVar myOtherVar 1 2 + + /

		myVar * thirdVar
		Error (6): Found unary operator [*] but only one operand [myVar].

		thirdVar fourthVar *
		Error (17): Unknow identifier [fourthVar].

		_Func AddThree lhs mhs rhs
			lhs mhs rhs + +
			_Return _Last

		_Set fourthVar 5 10 15 Define
		fourthVar = 30

		1 2 AddThree
		Error (6): Insufficient number of arguments supplied
			Expected (3) but, only found (2).

		_Func AddStore &where lhs mhs rhs
			_Set where lhs rhs mhs AddThree

		_Func AddMany param0 params...
			_If (!params) _Return a
			_Else		  _Return params... AddMany param0 +

		// 1 2 3 4 5 AddMany
		// 1 2 3 4   AddMany 5 +
		// 1 2 3     AddMany 4 + 5 +
		// 1 2       AddMany 3 + 4 + 5 +
		// 1         AddMany 2 + 3 + 4 + 5 +
		// 1 2 + 3 + 4 + 5 +
		// 3 3 + 4 + 5 +
		// 6 4 + 5 +
		// 10 5 +
		// 15
	*/

	class Parser {
	private:
		enum class St : size_t;

	private:
		struct ConditionAndStatement {
			std::string_view Condition;
			std::string_view Statement;
		};

	public:
		Parser();
		Parser(std::vector<ParamData> const& paramData, bool bValidation);

		static void ParseFile(fs::path const& filePath);
		static void ParseFile(fs::path const& filePath, fs::path const& outFilePath);
		static void ParseIStream(std::istream& is);
		static void ParseIStream(std::istream& is, std::ostream& resultOStream);
		
		std::optional<double> CallFunction(std::string_view funcName, std::vector<double> const& args);

		void ParseLine(std::string_view line);
		void ListVarNames(std::string_view prefix);

		void SetOutStream(std::ostream& toWhat);

		bool IsVisible(std::string_view varName) const;
		double Deref(std::string_view varName) const;
		double GetLast() const;

		bool IsFunction(std::string_view funcName) const;
		constexpr bool IsParsingFunction() const 
			{ return m_bInFunction; }
		
		constexpr size_t GetLineNumber() const 
			{ return m_LineNumber; }
		constexpr bool IsLineEndsWithSemiColon() const 
			{ return m_bSemiColon; }

	private:
		void HandleFirstToken();
		void SetVar(std::string_view name, double value);
		double GetVar(std::string_view name) const;
		double Eval(std::string_view exprString) const;
		constexpr void IncrementLineNumber(size_t inc = 1U) 
			{ m_LineNumber += inc; }
		
		void HandleSetKeyword();
		void HandleNormalExpression();
		void HandleListKeyword();

		void HandleFuncKeyword();
		void HandleReturnKeyword();
		void AddFunctionLine();

		void HandleSelectionKeyword();
		void HandleConditionalBody(bool bExecute);
		ConditionAndStatement ParseConditionalHeader(std::string_view header);

		void AssertKeyword(std::string_view glyph, KeywordType what);
		void AssertIdentifier(std::string_view what);

		constexpr void SetState(St newState) 
			{ m_CurrState = newState; }
		constexpr St GetState() const 
			{ return m_CurrState; }
		
	private:
		FunctionManager m_FunMan{*this};

		std::string_view m_CurrentLine{};
		std::unordered_map<std::string, double> m_VarMap{};
		St m_CurrState;
		size_t m_LineNumber{};

		bool m_bSemiColon{};
		bool m_bInFunction{};
		bool m_bConditionalBlockExecuted{};

		std::optional<bool> m_bConditionRegister{};
		std::optional<double> m_ReturnValueRegister{};

		std::unique_ptr<Parser> m_pValidationSubParser{};

		std::ostream* m_pOutStream{&std::cout};
	};
}