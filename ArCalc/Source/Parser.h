#pragma once

#include "Core.h"
#include "Util/Keyword.h"
#include "Util/FunctionManager.h"
#include "Util/LiteralManager.h"

/* Minimum amount of features to start working on the console interface:
	* Add the _Clear keyword (clears the damn console). (probably not gonna happen)
	* Add hexadecimal, binary and octal number parsing.
	* Add Current session serialization {
		* Maybe by not passing anything to the _Save keyword, it will serialize the current
		  session to disk?
		* Maybe by not passed anything to the _Load keyword, it will load the last session
		  if it exists, other wise it will be a syntax error?
	}

	* Niche {
		* Making the window always on top.
		* Changing the color of the console.
		* Syntax highlighting in the console.
		* Better error messages.
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
		static bool IsExecSt(St st);
		static bool IsValSt(St st);
		static bool IsSelSt(St st);

	private:
		struct ConditionAndStatement {
			std::string_view Condition;
			std::string_view Statement;
		};

		class ConditionInfo {
		public:
			ConditionInfo()                                = default;
			ConditionInfo(ConditionInfo const&)            = default;
			ConditionInfo(ConditionInfo&&)                 = default;
			ConditionInfo& operator=(ConditionInfo const&) = default;
			ConditionInfo& operator=(ConditionInfo&&)      = default;

		public:
			void Unreset();
			bool Get() const;

			constexpr void Reset() {
				m_bAvail = false;
			}

			constexpr bool IsAvail() const {
				return m_bAvail;
			}

			constexpr void Set(bool toWhat) {
				m_bCond = toWhat;
				m_bAvail = true;
			}

			constexpr bool ValueOr(bool what) const {
				return IsAvail() ? m_bCond : what;
			}

		private:
			bool m_bCond{};
			bool m_bAvail{};
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
		
		constexpr size_t GetLineNumber()         const { return m_LineNumber; }
		constexpr void ResetLineNumber()               { m_LineNumber = 0; }
		constexpr bool IsLineEndsWithSemiColon() const { return m_bSemiColon; }

		constexpr bool IsCurrentStatementReturning() const { return m_bJustHitReturn; }
		std::optional<double> GetReturnValue(FuncReturnType retype);

		LiteralManager const& GetLitMan()  const { return m_LitMan; }
		FunctionManager const& GetFunMan() const { return m_FunMan; }

		void ExceptionReset();

		void ToggleOutput();
		constexpr bool IsOutputEnabled() const { return !m_bSuppressOutput; }

	private:
		void HandleFirstToken();
		std::optional<double> Eval(std::string_view exprString);
		constexpr void IncrementLineNumber(size_t inc = 1U) { m_LineNumber += inc; }
		
		void HandleSetKeyword();
		void HandleNormalExpression();
		void HandleListKeyword();

		void HandleFuncKeyword();
		void HandleReturnKeyword();
		void AddFunctionLine();

		void HandleSelectionKeyword();
		void HandleConditionalBody(KeywordType selKW, bool bExecute);
		ConditionAndStatement ParseConditionalHeader(KeywordType selKW, std::string_view header);

		void HandleSaveKeyword();
		void HandleLoadKeyword();

		void HandleUnscopeKeyword();

		void ExpectKeyword(std::string_view glyph, KeywordType what);
		void KeywordDebugDoubleCheck(std::string_view glyph, KeywordType what);
		void ExpectIdentifier(std::string_view what);

		constexpr St GetState()              const { return m_CurrState; }
		constexpr void SetState(St newState)       { m_CurrState = newState; }

		template <class... FormatArgs>
		void Print(std::string_view fmtString, FormatArgs&&... fmtArgs) {
			if (IsOutputEnabled() && !IsLineEndsWithSemiColon()) {
				GetOStream() << std::vformat(
					fmtString, 
					std::make_format_args(std::forward<FormatArgs>(fmtArgs)...)
				);
			}
		}

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

		// Used to indicate that all already parsed branches contain return statements, 
		// so that if the current statement is an else, and it also contains a return,
		// the function must exist immediately.
		bool m_bAllOtherBranchesReturned{};
		bool m_bFuncMustExist{};
		
		ConditionInfo m_bConditionRegister{};

		bool m_bJustHitReturn{}; // Used by FunctionManager to know when to exist the function.
		std::optional<double> m_ReturnValueRegister{};        // Value: for execution.
		std::optional<FuncReturnType> m_ReturnTypeRegister{}; // Type : for validation.

		// So the validator does not output anything while the function is being defined.
		std::stringstream m_ValidationStringStream{}; 
		std::unique_ptr<Parser> m_pValSubParser{};

		bool m_bSuppressOutput{};
		std::ostream* m_pOutStream{};
	};
}