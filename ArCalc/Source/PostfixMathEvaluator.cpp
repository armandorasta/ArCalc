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
		FoundSingleQuote, 

		ParsingOperator,
	};

	PostfixMathEvaluator::PostfixMathEvaluator(LiteralManager& litMan, FunctionManager& funMan) 
		: m_LitMan{litMan}, m_FunMan{funMan}
	{
		if (!MathOperator::IsInitialized()) {
			MathOperator::Initialize();
		}
	}

	std::optional<double> PostfixMathEvaluator::Eval(std::string_view exprString) {
		ARCALC_DA(!exprString.empty(), "Evaluating empty expression");
		
		for (auto const c : exprString) {
			DoIteration(c); // To allow for recursive behaviour.
		}
		DoIteration(' '); // Cut any unfinished tokens.

		// Incomplete evaluation
		ARCALC_EXPECT(m_Values.Size() < 2, "Expression was not parsed completely"
			"; too many values are still on the stack");

		if (m_Values.IsEmpty()) {
			return {};
		} 
		else return m_Values.Pop().Deref();
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
		case St::ParsingNumber:     
		case St::FoundSingleQuote:  ParseNumber(c); break;
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
				if (bMinus) {
					// Minus sign turns it into an rvalue.
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
			else throw ArCalcException{"Used of invalid name [{}]", literalName};
		}

		ResetString();
		ResetState(c);
	}

	void PostfixMathEvaluator::ParseNumber(char c) {
		// Even something like '5PI' instead of '5 * PI' is not allowed for now.
		ARCALC_DA(!std::isalpha(c) && c != '_', "Found alphabetic character [{}] while parsing number",
			c);

		auto const state{GetState()};
		ARCALC_NOT_POSSIBLE(!(state == St::ParsingNumber || state == St::FoundSingleQuote));

		if (state == St::FoundSingleQuote) switch (c) {
		case '.':
			throw ArCalcException{"Found a ' just before a floating point while parsing a number"};
		case '\'':
			throw ArCalcException{ "Found two ' in a row while parsing a number"};
		default: // One of the rare reachable ones.
			SetState(St::ParsingNumber);
			break; // Advised m_By K & R themselves.
		}

		if (std::isdigit(c)) {
			AddChar(c);
		} else if (c == '.') {
			if (range::distance(GetString() | view::filter(Util::Eq('.'))) > 0U) {
				throw ArCalcException{"Found a number with multiple floating points"};
			}
			AddChar(c);
		} else if (c == '\'') { // Single quotes are for readability 10000000 => 10'000'000.
			if (auto const acc{GetString()}; !acc.empty() && acc.back() == '.') {
				throw ArCalcException{"Found a ' right after a floating point while parsing a number"};
			}

			SetState(St::FoundSingleQuote); // To check for some errors later ^^^
			return; // Ignore it.
		} else {
			// Finished parsing, evaluating...
			m_Values.PushRValue(std::atof(GetString().c_str()));
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
		ARCALC_EXPECT(MathOperator::IsValid(glyph), "Invalid operator [{}]", glyph);

		if (MathOperator::IsBinary(glyph)) {
			ARCALC_EXPECT(m_Values.Size() > 0, "Found binary operator [{}] with no operands", glyph);
			ARCALC_EXPECT(m_Values.Size() > 1, "Found binary operator [{}] with 1 operand with value [{}]",
				glyph, m_Values.Top().Deref());

			auto const rhs{m_Values.Pop().Deref()};
			auto const lhs{m_Values.Pop().Deref()}; 
			// Must pop here ^^^ because, lhs might be an lvalue, and the expression result 
			// must be an rvalue.
			m_Values.PushRValue(MathOperator::EvalBinary(glyph, lhs, rhs));
		} else {
			ARCALC_EXPECT(m_Values.Size() > 0, "Found unary operator [{}] with no operands", glyph);
			auto const operand{m_Values.Pop().Deref()};
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
					auto const top{m_Values.Pop()};
					ARCALC_EXPECT(top.bLValue, "Passing rvalue [{}] by reference", top.Deref());
					param.SetRef(top.Ptr);
				} else {
					param.PushValue(m_Values.Pop().Deref());
				}
			}
		} else {
			throw ArCalcException{
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

		for (auto& param : func.Params | view::filter([](auto& p) { return !p.IsPassedByRef(); })) {
			param.ClearValues();
		}
	}

	bool PostfixMathEvaluator::IsCharValidForIdent(char c) {
		return Str::IsAlNum(c) || c == '_';
	}

	bool PostfixMathEvaluator::IsCharValidForNumber(char c) {
		return Str::IsDigit(c) || c == '\'' || c == '.';
	}
}
