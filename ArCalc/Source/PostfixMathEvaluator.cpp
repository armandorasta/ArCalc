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
		} else if (m_Values.IsEmpty()) {
			return {};
		} 
		else return *m_Values.Pop();
	}

	void PostfixMathEvaluator::Reset() {
		ResetString();
		m_Values.Clear();
		m_CurrState = St::WhiteSpace;
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

		// Finished parsing, evaluating...
		std::string literalName{GetString()};
		if (MathOperator::IsValid(literalName)) {
			EvalOperator();
		} else if (m_FunMan.IsDefined(literalName)) {
			EvalFunction();
		} else {
			// Strip the negative sign for lookup.
			auto const bMinus{literalName.front() == '-'};
			literalName = literalName.substr(bMinus ? 1U : 0U);
			
			if (m_LitMan.IsVisible(literalName)) {
				if (bMinus) { // Minus sign turns it into an rvalue.
					m_Values.PushRValue(*m_LitMan.Get(literalName) * -1.0);
				} else {
					m_Values.PushLValue(&m_LitMan.Get(literalName));
				}
			} else if (literalName == Keyword::ToString(KeywordType::Last)) {
				// Is is always treated as an rvalue, the user can not pass it by reference.
				m_Values.PushRValue(*m_LitMan.Get(literalName) * (bMinus ? -1.0 : 1.0));
			} else if (MathConstant::IsValid(literalName)) {
				m_Values.PushRValue(MathConstant::ValueOf(literalName) * (bMinus ? -1.0 : 1.0));
			} else if (Keyword::IsValid(literalName)) {
				// Only valid keyword in this context is _Last
				ARCALC_DE("Found keyword [{}] in invalid context (in the middle of an expression)",
					literalName);
			}
			else throw ExprEvalError{"Used of invalid name [{}]", literalName};
		}

		ResetString();
		ResetState(c);
	}

	void PostfixMathEvaluator::ParseNumber(char c) {
#ifndef NDEBUG
		auto const state{GetState()};
#endif // ^^^^ Variable is used in debug mode only.
		ARCALC_NOT_POSSIBLE(state != St::ParsingNumber);

		if (std::isdigit(c) || 
			c == 'e' ||  // For exponential notation.
			c == '.' ||  // Floating point.
			c == '\''||  // Single quotes are for readability 10000000 => 10'000'000.
			c == '-')    // For negative exponents.
		{
			AddChar(c);
		} else if (std::isalpha(c)) {
			throw ParseError{
				"Found invalid character [{}] while parsing number [{}]",
				c, GetString(),
			};
		} else {
			// Finished parsing, evaluating...
			auto const cleanNumberString{ValidateAndFixParsedNumber()};
			m_Values.PushRValue(std::atof(cleanNumberString.c_str()));
			ResetString();
			ResetState(c);
		}
	}

	std::string PostfixMathEvaluator::ValidateAndFixParsedNumber() {
		auto numberString = std::string_view{GetString()}; 

		auto res = std::string{};
		if (numberString.front() == '-') {
			numberString = numberString.substr(1);
			res.push_back('-');
		}

		bool bFoundE{};
		bool bFoundFloatingPoint{};

		for (auto it{numberString.begin()}; it < numberString.end(); ++it) {
			if (auto const ch{*it}; std::isdigit(ch)) {
				res.push_back(ch);
			} else switch (ch) {
			case 'e': {
				if (bFoundE) {
					throw ParseError{
						"Found more than one `e` while parsing number [{}]",
						numberString,
					};
				} else {
					bFoundE = true;
				}

				if (std::next(it) == numberString.end()) {
					throw ParseError{
						"Found `e` but nothing after while parsing number [{}]",
						numberString,
					};
				} else if (*std::next(it) == '\'') {
					throw ParseError{
						"Found `e` just before `'` while parsing number [{}]", 
						numberString
					};
				}

				res.push_back(ch);
				break;
			}
			case '.': {
				if (bFoundE) {
					throw ParseError{
						"Found floating point in exponent while parsing number [{}]",
						numberString,
					};				
				}

				if (bFoundFloatingPoint) {
					throw ParseError{
						"Found more than one floating point while parsing number [{}]",
						numberString,
					};
				} else {
					bFoundFloatingPoint = true;
				}
				
				if (std::next(it) != numberString.end() && *std::next(it) == '\'') {
					throw ParseError{
						"Found a `'` right after a floating point while parsing a number"
					};
				}

				res.push_back(ch);
				break;
			}
			case '\'': {
				if (auto const next{std::next(it)}; next == numberString.end()) {
					throw ParseError{
						"Found a `'` at the end of number [{}]",
						numberString,
					};
				} else switch (*next) {
				case '\'':
					throw ParseError{"Found two `'` in a row while parsing number [{}]", numberString};
				case '.': 
				case 'e':
					throw ParseError{
						"Found {} right after `'` while parsing number [{}]", 
						*next == '.' ? "a floating point" : "`e`",
						numberString,
					};
				} 

				break;
			}
			default: ARCALC_UNREACHABLE_CODE();
			}
		}

		return res;
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
		} else {
			if (m_Values.Size() == 0) {
				throw ExprEvalError{"Found unary operator [{}] with no operands", glyph};
			}
			
			auto const operand{*m_Values.Pop()};
			// Must pop here ^^^, explained in the other branch.
			m_Values.PushRValue(MathOperator::EvalUnary(glyph, operand));
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
