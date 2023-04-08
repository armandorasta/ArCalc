#pragma once

#include "IEvaluator.h"
#include "ValueStack.h"
#include "Util/LiteralManager.h"
#include "Util/FunctionManager.h"
#include "Util/NumberParser.h"

namespace ArCalc {
	class PostfixMathEvaluator : public IEvaluator {
	private:
		enum class St : std::size_t;

	public:
		PostfixMathEvaluator(LiteralManager& litMan, FunctionManager& funMan);

		std::optional<double> Eval(std::string_view exprString);
		void Reset();

	private:
		void DoIteration(char c);

		void ParseWhiteSpace(char c);
		void ParseIdentifier(char c);
		void ParseSymbolicOperator(char op);

		void ParseNumber(char c);

		void ParseMinusSign(char c);
		void PushLiteralValue();

	private:
		void SetState(St newState);
		void ResetState(char c);
		St GetState() const noexcept;

		void AddChar(char c);
		std::string const& GetString() const;
		void ResetString();

		bool IsCharValidForIdent(char c);
		bool IsCharValidForNumber(char c);

		void EvalOperator();
		void EvalFunction();

		constexpr void SetLineNumber(size_t toWhat) 
			{ m_LineNumber = toWhat; }
		constexpr size_t GetLineNumber() const 
			{ return m_LineNumber; }

	private:
		std::string m_CurrStringAcc{};
		St m_CurrState{};
		
		ValueStack m_Values{};
		
		size_t m_LineNumber{};

		LiteralManager& m_LitMan;
		FunctionManager& m_FunMan;
		NumberParser m_NumPar{};
	};
}