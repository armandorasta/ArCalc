#pragma once

#include "IEvaluator.h"
#include "ValueStack.h"

namespace ArCalc {
	class PostfixMathEvaluator : public IEvaluator {
	private:
		enum class State : std::size_t;

	public:
		PostfixMathEvaluator(std::unordered_map<std::string, double> const& literals);

		double Eval(std::string const& exprString);

	private:
		void DoIteration(char c);

		void ParseWhiteSpace(char c);
		void ParseIdentifier(char c);
		void ParseNumber(char c);
		void ParseSymbolicOperator(char op);

		void ParseMinusSign(char c);
		void HandleAddingLiteral();

	private:
		void SetState(State newState);
		void ResetState(char c);
		State GetState() const noexcept;

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

		constexpr void IncrementCharIndex(size_t amount = 1U) { m_CharIndex += amount; }
		constexpr size_t GetCharIndex() const { return m_CharIndex; }

		constexpr void SetLineNumber(size_t toWhat) { m_LineNumber = toWhat; }
		constexpr size_t GetLineNumber() const { return m_LineNumber; }

	private:
		std::string m_CurrStringAcc{};
		State m_CurrState{};
		std::unordered_map<std::string, double> const& m_Literals{};
		ValueStack m_Values{};
		size_t m_CharIndex{};
		size_t m_LineNumber{};
	};
}