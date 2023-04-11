#include "PostfixMathEvaluator.h"
#include "Util/MathOperator.h"
#include "Util/MathConstant.h"
#include "Util/Str.h"
#include "Exception/ArCalcException.h"
#include "Parser.h"

namespace ArCalc {
	enum class PostfixMathEvaluator::St : std::size_t {
		WhiteSpace,
		FoundMinusSign,
		HandledMinusSign,

		ParsingIdentifier,

		ParsingNumber,
		ParsingOperator,
	};

	PostfixMathEvaluator::PostfixMathEvaluator(LiteralManager& litMan, FunctionManager& funMan) 
		: m_LitMan{litMan}, m_FunMan{funMan}
	{
	}

	std::optional<double> PostfixMathEvaluator::Eval(std::string_view exprString) {
		if (exprString.empty()) {
			throw ExprEvalError{"Evaluating empty expression"};
		}
		
		for (auto const c : exprString) {
			DoIteration(c); // To allow for recursive behaviour.
		}
		DoIteration(' '); // Cut any unfinished tokens.

		if (m_Values.Size() > 1) { 
			throw ExprEvalError{
				"Incomplete eval: {{ {}}}", [this] {
					for (auto stackStr = std::string{};;) {
						stackStr = std::to_string(*m_Values.Pop()) + ' ' + stackStr; 
						if (m_Values.IsEmpty()) {
							return stackStr;
						}
					}
				}(/*)(*/)
			};
		} else {
			auto const res = m_Values.IsEmpty() ? std::optional<double>{} : *m_Values.Pop();
			Reset();
			return res;
		}
	}

	void PostfixMathEvaluator::Reset() {
		ResetString();
		m_Values.Clear();
		m_CurrState = St::WhiteSpace;
		m_NumPar.Reset();
	}

	void PostfixMathEvaluator::DoIteration(char c) {
		switch (GetState()) {
		case St::WhiteSpace:        ParseWhiteSpace(c); break;
		case St::ParsingIdentifier: ParseIdentifier(c); break;
		case St::ParsingOperator:   ParseSymbolicOperator(c); break;
		case St::ParsingNumber:     ParseNumber(c); break;
		case St::FoundMinusSign:    ParseMinusSign(c); break;
		}
	}

	void PostfixMathEvaluator::ParseWhiteSpace(char c) {
		if (std::isspace(c)) {
			return;
		} else if (std::isalpha(c) || c == '_') {
			SetState(St::ParsingIdentifier);
			ParseIdentifier(c);
		} else if (std::isdigit(c) || c == '.') {  
			// The second condition allows ".5" instead of the long-winded "0.5".
			SetState(St::ParsingNumber);
			ParseNumber(c);
		} else {
			SetState(St::ParsingOperator);
			ParseSymbolicOperator(c);
		}
	}

	void PostfixMathEvaluator::ParseIdentifier(char c) {
		if (IsCharValidForIdent(c)) { // Parsing...
			AddChar(c);
			return;
		}

		/* **** Order of checks ****
		 * 1) Literals (and the Last keyword).
		 * 2) Functions.
		 * 3) Constants and operators.
		 * 
		 * Due to conventions, constant and operator names will never overlap, 
		 * and thus the order of thier checks will not make a difference.
		 */

		// Finished parsing, evaluating...
		std::string identifier{GetString()};

		auto const bMinus{identifier.front() == '-'};
		if (bMinus) { // Strip the negative sign for lookup.
			identifier = identifier.substr(1);
		}

		if (m_LitMan.IsVisible(identifier)) {
			if (bMinus) { // Minus sign turns it into an rvalue.
				m_Values.PushRValue(*m_LitMan.Get(identifier) * -1.0);
			} else {
				m_Values.PushLValue(&m_LitMan.Get(identifier));
			}
		} else if (identifier == Keyword::ToString(KeywordType::Last)) {
			// Is is always treated as an rvalue, the user can not pass it by reference.
			m_Values.PushRValue(*m_LitMan.Get(identifier) * (bMinus ? -1.0 : 1.0));
		} else if (m_FunMan.IsDefined(identifier)) {
			if (bMinus) {
				throw ExprEvalError{"Found function name [{}] preceeded by a minus sign"};
			} else {
				EvalFunction();
			}
		} else if (MathConstant::IsValid(identifier)) {
			m_Values.PushRValue(MathConstant::ValueOf(identifier) * (bMinus ? -1.0 : 1.0));
		} else if (MathOperator::IsValid(identifier)) { 
			if (bMinus) {
				throw ExprEvalError{"Found operator name [{}] preceeded by a minus sign"};
			} else {
				EvalOperator();
			}
		} else if (Keyword::IsValid(identifier)) {
			// Only valid keyword in this context is _Last, which was already handled above.
			throw SyntaxError{
				"Found keyword [{}] in invalid context (in the middle of an expression)",
				identifier
			};
		} else {
			throw ExprEvalError{"Used of invalid name [{}]", identifier};
		} 

		ResetString();
		ResetState(c);
	}

	void PostfixMathEvaluator::ParseNumber(char c) {
		if (auto const res{m_NumPar.Parse(c)}; res.IsDone) {
			// When the string is not empty, a minus sign is assumed to be there.
			m_Values.PushRValue(GetString().empty() ? res.Value : -res.Value);
			ResetString();
			ResetState(c);
		}
	}

	void PostfixMathEvaluator::ParseSymbolicOperator(char op) {
		if (std::isspace(op) || std::isalnum(op)) { // Operator token fully accumulated?
			EvalOperator();
			ResetString();
			ResetState(op);
		} else {
			if (op == '-') { // Could be a negative number.
				SetState(St::FoundMinusSign);
			}
			AddChar(op);
		}
	}

	void PostfixMathEvaluator::ParseMinusSign(char c) {
		if (std::isspace(c)) {
			EvalOperator();
			ResetString();
			ResetState(c);
		} else if (std::isdigit(c) || c == '.') {
			SetState(St::ParsingNumber);
			ParseNumber(c);
		} else if (IsCharValidForIdent(c)) {
			SetState(St::ParsingIdentifier);
			ParseIdentifier(c);
		} 
		else ARCALC_UNREACHABLE_CODE();
	}

	void PostfixMathEvaluator::PushLiteralValue() {
		auto literalName{GetString()};

		auto const bMinusSign{literalName.starts_with('-')};
		if (bMinusSign) {
			m_Values.PushRValue(*m_LitMan.Get(literalName.substr(1)) * -1.0);
		} else {
			m_Values.PushLValue(&m_LitMan.Get(literalName));
		}
	}

	void PostfixMathEvaluator::SetState(St newState) {
		m_CurrState = newState;
	}

	void PostfixMathEvaluator::ResetState(char c) {
		SetState(St::WhiteSpace);
		if (!std::isspace(c)) {
			DoIteration(c);
		}
	}

	PostfixMathEvaluator::St PostfixMathEvaluator::GetState() const noexcept {
		return m_CurrState;
	}

	void PostfixMathEvaluator::AddChar(char c) {
		m_CurrStringAcc.push_back(c);
	}

	std::string const& PostfixMathEvaluator::GetString() const {
		return m_CurrStringAcc;
	}

	void PostfixMathEvaluator::ResetString() {
		m_CurrStringAcc.clear();
	}

	void PostfixMathEvaluator::EvalOperator() {
		auto const& glyph{GetString()};
		if (!MathOperator::IsValid(glyph)) {
			throw ExprEvalError{"Invalid operator [{}]", glyph};
		}

		if (MathOperator::IsBinary(glyph)) {
			if (m_Values.Size() == 0) {
				throw ExprEvalError{"Found binary operator [{}] with no operands", glyph};
			} else if (m_Values.Size() == 1) {
				throw ExprEvalError{
					"Found binary operator [{}] with 1 operand with value [{}]", 
					glyph, *m_Values.Top()
				};
			}

			auto const rhs{*m_Values.Pop()};
			auto const lhs{*m_Values.Pop()}; 
			// Must pop here ^^^ because, lhs might be an lvalue, and the expression result 
			// must be an rvalue.
			m_Values.PushRValue(MathOperator::EvalBinary(glyph, lhs, rhs));
		} else if (MathOperator::IsUnary(glyph)) {
			if (m_Values.Size() == 0) {
				throw ExprEvalError{"Found unary operator [{}] with no operands", glyph};
			}
			
			auto const operand{*m_Values.Pop()};
			// Must pop here ^^^, explained in the other branch.
			m_Values.PushRValue(MathOperator::EvalUnary(glyph, operand));
		} else if (MathOperator::IsVariadic(glyph)) {
			if (m_Values.Size() == 0) {
				throw ExprEvalError{"Found variadic operator [{}] with no operands", glyph};
			}

			auto const operands = [&] {
				std::vector<double> res{};
				res.reserve(m_Values.Size());
				while (!m_Values.IsEmpty()) {
					res.push_back(*m_Values.Pop());
				}

				return res;
			}(/*)(*/);

			// Must pop here ^^^, explained in the other branch.
			m_Values.PushRValue(MathOperator::EvalVariadic(glyph, operands));
		} else {
			ARCALC_UNREACHABLE_CODE();
		}
	}

	void PostfixMathEvaluator::EvalFunction() {
		auto const funcName{GetString()};
		auto& func{m_FunMan.Get(funcName)};

		if (auto& params{func.Params}; m_Values.Size() >= params.size()) {
			for (auto const i : view::iota(0U, params.size()) | view::reverse) {
				auto& param{params[i]};
				if (param.IsPassedByRef()) {
					if (auto const top{m_Values.Pop()}; top.bLValue) {
						param.SetRef(top.Ptr);
					} else {
						throw ExprEvalError{"Passing rvalue [{}] by reference", *top};
					}
				} else {
					param.PushValue(*m_Values.Pop());
				}
			}
		} else {
			throw ExprEvalError{
				"Function [{}] Expects [{}] arguments, but only [{}] are available in the stack",
				funcName, params.size(), m_Values.Size()
			};
		}

		try {
			if (auto const returnValue{m_FunMan.CallFunction(funcName)}; returnValue.has_value()) {
				m_Values.PushRValue(*returnValue);
			}
		} catch (ArCalcException& err) {
			err.SetLineNumber(err.GetLineNumber() + func.HeaderLineNumber);

			// So if we are deep in the stack, the functions above do not modify
			// the line number to the line where the call of this function occurred.
			err.LockNumberLine(); 
			throw;
		}

		// for (auto& param : func.Params | view::filter([](auto& p) { return !p.IsPassedByRef(); })) {
		// 	param.ClearValues();
		// }
	}

	bool PostfixMathEvaluator::IsCharValidForIdent(char c) {
		return Str::IsAlNum(c) || c == '_';
	}

	bool PostfixMathEvaluator::IsCharValidForNumber(char c) {
		return Str::IsDigit(c) || c == '\'' || c == '.';
	}
}