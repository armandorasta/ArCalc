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

	PostfixMathEvaluator::PostfixMathEvaluator(VarMap const& literals, FunctionManager& funMan) 
		: m_Literals{literals}, m_FunMan{funMan}
	{
		// Validate the input map.
		for (auto const& [name, _value] : m_Literals) {
			// First character may not be a number.
			ARCALC_ASSERT(IsCharValidForIdent(name[0]) && !Str::IsNum(name[0]),
				"Found Invalid literal name [{}] in literal map (starts with a number)", name);

			for (auto const c : name | view::drop(1U)) {
				ARCALC_ASSERT(IsCharValidForIdent(c), 
					"Found Invalid literal name [{}] in literal map", name);
			}
		}

		if (!MathOperator::IsInitialized()) {
			MathOperator::Initialize();
		}
	}

	double PostfixMathEvaluator::Eval(std::string_view exprString) {
		ARCALC_ASSERT(!exprString.empty(), "Evaluating empty expression");
		
		for (auto const c : exprString) {
			DoIteration(c); // To allow for recurive behaviour.
		}
		DoIteration(' '); // Cut any unfinished tokens.

		// Incomplete evaluation
		ARCALC_EXPECT(m_Values.Size() < 2, "Expression was not parsed completely"
			"; too many values are still on the stack");

		// Should be impossible.
		ARCALC_ASSERT(m_Values.Size() > 0, "Invalid state at end of expression"
			"; stack was empty at the end of evaluation");
		return m_Values.Pop();
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
		} else if (MathConstant::IsValid(literalName)) {
			m_Values.Push(MathConstant::ValueOf(literalName));
		} else if (m_FunMan.IsDefined(literalName)) {
			EvalFunction();
		} else if (m_Literals.contains(literalName)) {
			PushLiteralValue();
		} else if (Keyword::IsValid(literalName) && literalName != "_Last") {
			// Only valid keyword in this context is _Last
			ARCALC_ERROR("Found keyword [{}] in invalid context (in the middle of an expression)", 
				literalName);
		} 
		else ARCALC_THROW(ArCalcException, "Used of invalid name [{}]", literalName);

		ResetString();
		ResetState(c);
	}

	void PostfixMathEvaluator::ParseNumber(char c) {
		// Even something like '5PI' instead of '5 * PI' is not allowed for now.
		ARCALC_ASSERT(!std::isalpha(c) && c != '_',
			"Found alphabetic character [{}] while parsing number", c
		);

		if (GetState() == St::FoundSingleQuote) {
			switch (c) {
			case '.':
				ARCALC_THROW(ArCalcException,
					"Found a ' just before a floating point while parsing a number");
			case '\'':
				ARCALC_THROW(ArCalcException, "Found two ' in a row while parsing a number");
			default: // One of the rare reachable ones.
				SetState(St::ParsingNumber);
				break; // Advised by K & R them selves.
			}
		}
		else SetState(St::ParsingNumber);

		if (std::isdigit(c)) {
			AddChar(c);
		} else if (c == '.') {
			if (range::distance(GetString() | view::filter(Util::Eq('.'))) > 0U) {
				ARCALC_THROW(ArCalcException, "Found a number with multiple floating points");
			}
			AddChar(c);
		} else if (c == '\'') { // Single quotes are for readability 10000000 => 10'000'000.
			if (auto const acc{GetString()}; !acc.empty() && acc.back() == '.') {
				ARCALC_THROW(ArCalcException, 
					"Found a ' right after a floating point while parsing a number");
			}

			SetState(St::FoundSingleQuote); // To check for some errors later ^^^
			return; // Ignore it.
		} else {
			// Finished parsing, evaluating...
			m_Values.Push(std::atof(GetString().c_str()));
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
		} else {
			SetState(St::ParsingIdentifier);
			ParseIdentifier(c);
		}
	}

	void PostfixMathEvaluator::PushLiteralValue() {
		auto literalName{GetString()};

		auto const bMinusSign{literalName.starts_with('-')};
		if (bMinusSign) {
			literalName = literalName.substr(1);
		}

		m_Values.Push(Deref(literalName) * (bMinusSign ? -1.0 : 1.0));
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

	bool PostfixMathEvaluator::IsValidLiteral(std::string const& glyph) const {
		return m_Literals.contains(glyph);
	}

	double PostfixMathEvaluator::Deref(std::string const& glyph) const {
		if (auto const it{m_Literals.find(glyph)}; it != m_Literals.end()) {
			return it->second;
		}
		else ARCALC_ERROR("Use of undefined literal [{}]", glyph);
	}

	void PostfixMathEvaluator::EvalOperator() {
		auto const& glyph{GetString()};
		ARCALC_EXPECT(MathOperator::IsValid(glyph), "Invalid operator [{}]", glyph);

		if (MathOperator::IsBinary(glyph)) {
			ARCALC_EXPECT(m_Values.Size() > 0, "Found binary operator [{}] with no operands", glyph);
			ARCALC_EXPECT(m_Values.Size() > 1, "Found binary operator [{}] with 1 operand with value [{}]",
				glyph, m_Values.Top());

			auto const rhs{m_Values.Pop()};
			auto const lhs{m_Values.Top()};
			m_Values.Top() = MathOperator::EvalBinary(glyph, lhs, rhs);
		} else {
			ARCALC_EXPECT(m_Values.Size() > 0, "Found unary operator [{}] with no operands", glyph);
			auto const operand{m_Values.Top()};
			m_Values.Top() = MathOperator::EvalUnary(glyph, operand);
		}
	}

	void PostfixMathEvaluator::EvalFunction() {
		auto const funcName{GetString()};
		std::vector<double> args{};
		auto const func{m_FunMan.Get(funcName)};
		auto const paramCount{func.Params.size()};
		if (m_Values.Size() >= paramCount) {
			args.resize(paramCount);
			for (size_t i : view::iota(0U, paramCount) | view::reverse) {
				args[i] = m_Values.Pop();
			}
		} else {
			// the Function is class is responsible for handling the case where the number of values on the stack 
			// are less than the number of arguments required.
			args.resize(m_Values.Size());
		}

		try {
			if (auto const returnValue{m_FunMan.CallFunction(funcName, args)}; returnValue) {
				m_Values.Push(*returnValue);
			}
		} catch (ArCalcException& err) {
			err.SetLineNumber(err.GetLineNumber() + func.HeaderLineNumber);
			err.LockNumberLine();
			throw;
		}
	}

	bool PostfixMathEvaluator::IsCharValidForIdent(char c) {
		return Str::IsAlNum(c) || c == '_';
	}

	bool PostfixMathEvaluator::IsCharValidForNumber(char c) {
		return Str::IsNum(c) || c == '\'' || c == '.';
	}
}
