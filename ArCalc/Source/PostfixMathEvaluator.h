#pragma once

#include "IEvaluator.h"
#include "ValueStack.h"
#include "Util/FunctionManager.h"

namespace ArCalc {
	class PostfixMathEvaluator : public IEvaluator {
	public:
		using VarMap = std::unordered_map<std::string, double>;

	private:
		enum class St : std::size_t;

	public:
		PostfixMathEvaluator(VarMap const& literals, FunctionManager& funMan);

		double Eval(std::string_view exprString);
		void Reset();

	private:
		void DoIteration(char c);

		void ParseWhiteSpace(char c);
		void ParseIdentifier(char c);
		void ParseNumber(char c);
		void ParseSymbolicOperator(char op);

		void ParseMinusSign(char c);
		void PushLiteralValue();

	private:
		void SetState(St newState);
		void ResetState(char c);
		St GetState() const noexcept;

		void AddChar(char c);
		std::string const& GetString() const;
		void ResetString();

		bool IsValidLiteral(std::string const& glyph) const;
		double Deref(std::string const& glyph) const;
		bool IsCharValidForIdent(char c);
		bool IsCharValidForNumber(char c);

		void EvalOperator();
		void EvalFunction();
		std::optional<double> CallFunction(std::string_view funcName, std::vector<double> const& args);

		constexpr void SetLineNumber(size_t toWhat) 
			{ m_LineNumber = toWhat; }
		constexpr size_t GetLineNumber() const 
			{ return m_LineNumber; }

	private:
		std::string m_CurrStringAcc{};
		St m_CurrState{};
		
		VarMap m_Literals{};
		ValueStack m_Values{};
		
		size_t m_LineNumber{};

		FunctionManager& m_FunMan;
	};
}