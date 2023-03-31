#pragma once

#include "Core.h"
#include "Util/Keyword.h"
#include "Util/FunctionManager.h"
#include "Util/LiteralManager.h"

/* Minimum amount of features to start working on the console interface:
	* Variables (done) {
		* Global variables, which are only usable in the global scope.
		* Local variables, which are only usable in their respective scope.
	}
	
	* Functions {
		* Passing m_By value. (done)
		* Passing m_By reference. (done)
		* Branching (done) {
			* If.
			* Else.
			* Elif chaining.
		}
		* Return and not return a value. (done)
		* Handling end of function better. (optional)
	}

	* Serialization {
		* Built in functions. (done)
		* Built in constants. (done)
		* Some of the user functions {
			* They should be sortable each in their own category.
			* They must not need to be loaded all at once.
		}
	}

	* Niche {
		* Making the window always on top.
		* Changing the color of the console.
		* Syntax highlighting in the console.
		* Better error messages.
		* Make it faster.
	}
*/

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
		Parser(Parser const&)         = default;
		Parser(Parser&&)              = default;
		auto operator=(Parser const&) = delete;
		auto operator=(Parser&&)      = delete;

		Parser(std::ostream& os);
		Parser(std::ostream& os, std::vector<ParamData>& paramData, bool bValidation);

		static void ParseFile(fs::path const& filePath);
		static void ParseFile(fs::path const& filePath, fs::path const& outFilePath);
		static void ParseIStream(std::istream& is);
		static void ParseIStream(std::istream& is, std::ostream& resultOStream);
		
		void ParseLine(std::string_view line);

		void SetOStream(std::ostream& toWhat);
		std::ostream& GetOStream();

		constexpr auto IsExecutingFunction() const 
			{ return m_bInFunction; }
		bool IsParsingFunction() const;

		bool IsParsingSelectionStatement() const;
		
		constexpr auto GetLineNumber() const 
			{ return m_LineNumber; }
		constexpr auto IsLineEndsWithSemiColon() const 
			{ return m_bSemiColon; }

		std::optional<double> GetReturnValue(FuncReturnType retype);

		LiteralManager const& GetLitMan() const
			{ return m_LitMan; }
		FunctionManager const& GetFunMan() const 
			{ return m_FunMan; }

		void ExceptionReset();

	private:
		void HandleFirstToken();
		std::optional<double> Eval(std::string_view exprString);
		constexpr auto IncrementLineNumber(size_t inc = 1U) 
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

		void HandleSaveKeyword();
		void HandleLoadKeyword();

		void ExpectKeyword(std::string_view glyph, KeywordType what);
		void KeywordDebugDoubleCheck(std::string_view glyph, KeywordType what);
		void ExpectIdentifier(std::string_view what);

		constexpr void SetState(St newState) 
			{ m_CurrState = newState; }
		constexpr auto GetState() const 
			{ return m_CurrState; }

		void Reset();
		
	private:
		FunctionManager m_FunMan;
		LiteralManager m_LitMan;

		std::string_view m_CurrentLine{};
		St m_CurrState;
		size_t m_LineNumber{};

		bool m_bSemiColon{};
		bool m_bInFunction{};
		bool m_bSelectionBlockExecuted{};

		std::optional<bool> m_bConditionRegister{};
		std::optional<double> m_ReturnValueRegister{};

		std::unique_ptr<Parser> m_pValidationSubParser{};

		std::ostream* m_pOutStream{};
	};
}