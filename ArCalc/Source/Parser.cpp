#include "Parser.h"
#include "Util/Util.h"
#include "PostfixMathEvaluator.h"
#include "Util/Function.h"
#include "Util/Str.h"
#include "Util/IO.h"
#include "Util/MathConstant.h"

/*
#define ARCALC_PARSE_ASSERT(_cond, _message, ...) \
	ARCALC_ASSERT(_cond, "Error ({}): " + std::string{_message}, GetLineNumber(), __VA_ARGS__)
#define ARCALC_PARSE_ERROR(_message, ...) \
	ARCALC_PARSE_ASSERT(false, _message, __VA_ARGS__)

#define ARCALC_PARSE_EXPECT(_cond, _message, ...) \
	ARCALC_EXPECT(_cond, "Error ({}): " + std::string{_message}, GetLineNumber(), __VA_ARGS__)
#define ARCALC_PARSE_THROW(_type, _cond, _message, ...) \
	ARCALC_THROW(_type, _message, __VA_ARGS__)
*/

namespace ArCalc {
	enum class Parser::State : size_t {
		Default = 0U,
		CollectingLinesForFunction,
		SubParser,
		SelectionStatementNewLine,
	};


	Parser::Parser() : m_CurrState{State::Default} {
		SetVar("_Last", 0.0);
	}

	Parser::Parser(std::vector<ParamData> const& paramData, bool bValidation) : Parser{} {
		m_bInFunction = true;

		if (bValidation) {
			// Validation will make all parameter packs have size 1 for now, because the sub-parser 
			// will evaluate all branches anyway.

			// The evaluator, however, is still unable to deal with parameter packs...
			// TODO: fix that.
			for (auto const& param : paramData) {
				m_VarMap.emplace(param.Name, 0.f);
			}
		} else for (auto const& param : paramData) {
			m_VarMap.emplace(param.Name, param.Values.front());

			if (param.IsParameterPack) {
				// Avoid doublicating the first argument in the pack.
				//                            vvvv
				for (auto const i : view::iota(1U, param.Values.size())) {
					m_VarMap.emplace(Str::Mangle(param.Name, i), param.Values[i]);
				}
			} 
		}
	}

	void Parser::ParseFile(fs::path filePath) {
		auto subParser = Parser{};
		auto const fileBytes{Str::FileToString(filePath)};
		auto const lines{Str::SplitOn<std::string_view>(fileBytes, "\n", false)};
		for (auto const& line : lines) {
			subParser.ParseLine(line);
		}
	}

	void Parser::ParseLine(std::string_view line) {
		IncrementLineNumber();

		m_CurrentLine = Str::Trim<std::string_view>(line);
		m_bSemiColon = !m_CurrentLine.empty() && (m_CurrentLine.back() == ';');

		switch (GetState()) {
		case State::SelectionStatementNewLine: {
			if (m_CurrentLine.empty()) { 
				break; 
			} else if (m_bSemiColon) {
				m_CurrentLine = m_CurrentLine.substr(0, m_CurrentLine.size() - 1);
			}
			HandleConditionalBody(*m_bConditionRegister);
			break;
		}
		case State::CollectingLinesForFunction: {
			try {
				m_pSubParser->ParseLine(line);
			} catch (ArCalcException&) {
				__debugbreak();
			} 
			AddFunctionLine();
			break;
		}
		case State::SubParser:
		default: {
			if (!m_CurrentLine.empty()) {
				if (m_bSemiColon) {
					m_CurrentLine = m_CurrentLine.substr(0U, m_CurrentLine.size() - 1);
				}
				HandleFirstToken();
			}
			break;
		}
		}
	}

	void Parser::ListVarNames(std::string_view prefix = "") {
		constexpr auto Tab = "    ";

		IO::OutputStd("{\n");
		for (auto const& [name, value] : m_VarMap) {
			if (name != "_Last" && name.starts_with(prefix)) {
				std::cout << std::format("{}{} = {}\n", Tab, name, value);
			}
		}
		IO::OutputStd("}");
	}

	void Parser::HandleFirstToken() {
		auto const firstToken{Str::GetFirstToken(m_CurrentLine)};
		if (auto const keyword{Keyword::FromString(firstToken)}; !keyword) {
			// Assume it's a normal expression and show its result in the next line.
			HandleNormalExpression();
		}
		else switch (*keyword) {
			using KT = KeywordType;
		case KT::Set:
			HandleSetKeyword();
			break;
		case KT::Last:
			HandleNormalExpression();
			break;
		case KT::List:
			HandleListKeyword();
			break;
		case KT::Func:
			HandleFuncKeyword();
			break;
		case KT::Return:
			HandleReturnKeyword();
			break;
		case KT::If:
		case KT::Else:
		case KT::Elif:
			HandleIfKeyword();
			break;
		default:
			ARCALC_UNREACHABLE_CODE();
		}
	}

	bool Parser::IsLineEndsWithSemiColon() const {
		return m_bSemiColon;
	}

	void Parser::SetVar(std::string_view name, double value) {
		m_VarMap.insert_or_assign(std::string{name}, value);
	}

	double Parser::GetVar(std::string_view name) const {
		if (auto const it{m_VarMap.find(std::string{name})}; it != m_VarMap.end()) {
			return it->second;
		}
		else ARCALC_PARSE_ERROR("Error: Use of unset variable {}", name);
	}

	double Parser::Eval(std::string_view exprString) const {
		try { 
			return PostfixMathEvaluator{m_VarMap}.Eval(std::string{exprString}); 
		} catch (ArCalcException& err) {
			err.SetLineNumber(GetLineNumber());
			throw;
		}
	}

	void Parser::HandleSetKeyword() {
		auto const state{GetState()}; 
		if (state != State::Default || state != State::SubParser) {
			ARCALC_UNREACHABLE_CODE();
		} 

		AssertKeyword(Str::ChopFirstToken<std::string_view>(m_CurrentLine), KeywordType::Set);
		auto const name{Str::ChopFirstToken<std::string_view, std::string_view>(m_CurrentLine)};
		AssertIdentifier(name);

		auto const value{Eval(m_CurrentLine)};
		SetVar(name, value);

		if (state == State::Default && !IsLineEndsWithSemiColon()) {
			IO::PrintStd("{} = {}\n", name, value);
		}
	}

	void Parser::HandleListKeyword() {
		if (GetState() == State::SubParser)
			return;

		auto const tokens{Str::SplitOnSpaces(m_CurrentLine)};
		(tokens.size() < 2) ? ListVarNames() : ListVarNames(tokens[1]);
	}

	void Parser::HandleFuncKeyword() {
		if (GetState() == State::SubParser) {
			ARCALC_PARSE_ERROR("Found keyword [{}] in an invalid context (inside a function)", KeywordType::Func);
		}

		auto const tokens{Str::SplitOnSpaces(m_CurrentLine)};
		AssertKeyword(tokens[0], KeywordType::Func);
		
		if (tokens.size() == 1) {
			ARCALC_PARSE_ERROR("Expected Function name, but found nothing");
		} 

		// Function name
		auto funcName = std::string_view{tokens[1]};
		AssertIdentifier(funcName);
		Function::BeginDefination(funcName, GetLineNumber());

		if (tokens.size() == 2) {
			ARCALC_PARSE_ERROR("Expected at least one paramter, but found none");
		}
		
		// Function parameters
		for (auto const i : view::iota(2U, tokens.size())) {
			auto const& paramName{tokens[i]};
			if constexpr (false && paramName.ends_with("...")) {
				ARCALC_PARSE_ASSERT(i == tokens.size() - 1, "Found '...' in the middle of the parameter list");
				ARCALC_PARSE_ASSERT(paramName.size() > 3, "Found '...' but no variable name");
				
				auto const currParamName{std::string_view{paramName}.substr(0, paramName.size() - 3)};
				AssertIdentifier(currParamName);
				Function::AddVariadicParam(currParamName);
				break; // Not needed because of the asserts above
			} else {
				AssertIdentifier(paramName);
				Function::AddParam(paramName);
			}
		}

		// Check function body for syntax errors
		m_pSubParser = std::make_unique<Parser>(Function::CurrParamData(), true);
		m_pSubParser->SetState(State::SubParser);
	}

	void Parser::HandleReturnKeyword() {
		switch (GetState()) {
		case State::Default: {
			ARCALC_PARSE_ASSERT(IsParsingFunction(), "Found {} in global scope", KeywordType::Return);
			AssertKeyword(Str::ChopFirstToken(m_CurrentLine), KeywordType::Return);
			if (m_CurrentLine.empty()) {
				m_ReturnValueRegister = {};
			} else {
				m_ReturnValueRegister = Eval(m_CurrentLine);
			}

			break;
		}
		case State::SubParser: {
			AssertKeyword(Str::ChopFirstToken(m_CurrentLine), KeywordType::Return);
			if (!m_CurrentLine.empty()) {
				Eval(m_CurrentLine); // Make sure no exception is thrown
			}
			
			break;
		}
		default: ARCALC_UNREACHABLE_CODE();
		}
	}

	std::optional<double> Parser::CallFunction(std::string_view funcName, std::vector<double> const& args) {
		ARCALC_ASSERT(Function::IsDefined(funcName), "Call of undefined function [{}]", funcName);
		
		auto const bVariadic{Function::IsVariadic(funcName)};
		if (bVariadic) {
			ARCALC_EXPECT(args.size() >= Function::ParamCountOf(funcName),
				"Variadic function [{}] expects at least [{}] arguments but only [{}] were passed",
				funcName, Function::ParamCountOf(funcName), args.size());
		} else {
			ARCALC_EXPECT(args.size() == Function::ParamCountOf(funcName),
				"Invalid number of arguments passed to [{}], expected {} but {} were passed",
				funcName, Function::ParamCountOf(funcName), args.size());
		}

		// Perparing function parameter values
		auto params{Function::ParamDataOf(funcName)};
		for (auto const i : view::iota(0U, args.size())) {
			params[i].Values.push_back(args[i]);
		}

		if (bVariadic) { /*Are there any left-overs in the arguments?*/ 
			auto& parameterPack{params.back().Values};
			for (auto const arg : args | view::drop(params.size())) {
				parameterPack.push_back(arg);
			}
		}
		
		auto subParser = Parser{params, false};
		for (auto const codeLines{Function::CodeLinesOf(funcName)}; auto const& codeLine : codeLines) {
			subParser.ParseLine(codeLine);
		}
		return subParser.m_ReturnValueRegister;
	}

	void Parser::AddFunctionLine() {
		// TODO: Handle end of function more generally.

		if (GetState() != State::CollectingLinesForFunction) {
			ARCALC_UNREACHABLE_CODE();
		}

		Function::AddCodeLine(m_CurrentLine);
		if (m_CurrentLine.starts_with(Keyword::ToString(KeywordType::Return))) {
			Function::EndDefination();
			SetState(State::Default);
			m_pSubParser.reset();
		}
	}

	void Parser::HandleIfKeyword() {
		// The flag must be turned on when creating the sub-parser for validation as well!!!
		ARCALC_PARSE_ASSERT(IsParsingFunction(), "Found selection statement in global scope");

		switch (auto const keyword{*Keyword::FromString(Str::ChopFirstToken(m_CurrentLine))}; keyword) {
		case KeywordType::Elif:
			ARCALC_PARSE_EXPECT(m_bConditionRegister.has_value(), "Found a hanging [{}] keyword", keyword);
			[[fallthrough]];
		case KeywordType::If: {
			auto const [cond, statement] {ParseConditionalHeader(m_CurrentLine)};
			if (cond.empty()) {
				ARCALC_PARSE_ERROR("Expected a condition after keyword [{}], but found nothing", keyword);
			}

			if (auto const state{GetState()}; state == State::Default) {
				m_bConditionRegister = !m_bConditionalBlockExecuted && std::abs(Eval(cond) - 0.0) > 0.000001;
			} else if (state == State::SubParser) {
				Eval(cond); // Sub-parser does not care about the condition.
			} 
			else ARCALC_UNREACHABLE_CODE();

			if (statement.empty()) {
				SetState(State::SelectionStatementNewLine);
			} else {
				m_CurrentLine = statement;
				// Sub-parser ignores: vvvvvvvvvvvvvvvvvvvv
				HandleConditionalBody(*m_bConditionRegister); 
			}
			break;
		}
		case KeywordType::Else: {
			ARCALC_PARSE_EXPECT(m_bConditionRegister.has_value(), "Found a hanging [{}] keyword", keyword);
			
			if (auto const state{GetState()}; state == State::Default) {
				m_bConditionRegister.value() = !m_bConditionalBlockExecuted;
			} else if (state != State::SubParser) {
				ARCALC_UNREACHABLE_CODE();
			}

			if (m_CurrentLine.empty()) {
				SetState(State::SelectionStatementNewLine);
			}
			else HandleConditionalBody(*m_bConditionRegister);
			break;
		}
		default: ARCALC_UNREACHABLE_CODE();
		}
	}

	void Parser::HandleConditionalBody(bool bExecute) {
		auto const state{GetState()};
		if (state != State::Default && state != State::SubParser) {
			ARCALC_UNREACHABLE_CODE();
		}

		if (bExecute) {
			m_bConditionalBlockExecuted = true;
			HandleFirstToken();
			SetState(State::Default);
		} else if (state == State::SubParser) {
			HandleFirstToken();
		} 

		/*

			TODO: Add return keyword.

		*/
		
	}

	Parser::ConditionAndStatement Parser::ParseConditionalHeader(std::string_view header) {
		// The condition ends when a keyword not valid in the middle of an expression 
		// is found, however, some keywords are not valid inside an if statement, 
		// the condition stops when it finds these keywords as well and puts out
		// a helpful error message.

		// TODO: add the python colon after an if and elif condition, make it optional 
		//       for for else blocks.
		std::array<std::pair<std::string_view, bool>, 8U> PossibleValidKeywords{{
			{Keyword::ToString(KeywordType::Set),    true},
			{Keyword::ToString(KeywordType::List),   true},
			{Keyword::ToString(KeywordType::If),     false},
			{Keyword::ToString(KeywordType::Elif),   false},
			{Keyword::ToString(KeywordType::Else),   false},
			{Keyword::ToString(KeywordType::Func),   false},
			{Keyword::ToString(KeywordType::Return), false},
		}};

		for (auto const& [kw, bValid] : PossibleValidKeywords) {
			if (auto condEndIndex{header.find(kw)}; condEndIndex != std::string::npos) {
				if (!bValid) {
					ARCALC_PARSE_ERROR("Found keyword [{}] in an invalid context (inside a condition)", kw);
				}

				return {
					.Condition{header.substr(0, condEndIndex)},
					.Statement{header.substr(condEndIndex)}
				};
			}
		}

		return {.Condition{header}, .Statement{}};
	}


	void Parser::HandleNormalExpression() {
		auto const state{GetState()};
		if (state != State::Default && state != State::SubParser) {
			ARCALC_UNREACHABLE_CODE();
		}

		auto const res{Eval(m_CurrentLine)};
		if (state == State::Default) {
			IO::PrintStd("{}\n", res);
			SetVar("_Last", res);
		} 
	}

	void Parser::AssertKeyword(std::string_view glyph, KeywordType type) {
		auto const tokenOpt{Keyword::FromString(glyph)};
		ARCALC_PARSE_ASSERT(tokenOpt && *tokenOpt == type, "Expected keyword [{}]", type);
	}

	void Parser::AssertIdentifier(std::string_view what) {
		ARCALC_PARSE_ASSERT(!std::isdigit(what[0]), "Invalid identifier ({}); found digit [{}]", 
			what, what[0]);
		ARCALC_PARSE_ASSERT(!Keyword::IsValid(what), "Expected identifier, but found keyword ({})", 
			what);
		ARCALC_PARSE_ASSERT(!MathConstant::IsValid(what), 
			"Expected identifier, but found math constant ({})", what);

		for (auto const c : what) {
			if (!std::isalnum(c) && c != '_') {
				ARCALC_PARSE_ERROR("Found invalid character [{}] in indentifier [{}]", c, what);
			}
		}
	}
}