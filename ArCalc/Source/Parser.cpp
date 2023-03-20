#include "Parser.h"
#include "Util/Util.h"
#include "PostfixMathEvaluator.h"
#include "Util/FunctionManager.h"
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
	enum class Parser::St : size_t {
		// Rest.
		Default = 0U,

		// Function validation.
		Val_LineCollection,
		Val_SubParser,
		Val_SelectionNewLine,

		// Selections statements.
		SelectionNewLine,
	};


	Parser::Parser(std::ostream& os) : m_CurrState{St::Default}, m_pOutStream{&os}, m_FunMan{os} {
		SetVar("_Last", 0.0);
	}

	Parser::Parser(std::ostream& os, std::vector<ParamData> const& paramData, bool bValidation) 
		: Parser{os} 
	{
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

	void Parser::ParseFile(fs::path const& filePath) {
		std::ifstream file{filePath};
		if (!file.is_open()) {
			ARCALC_THROW(ArCalcException, "{} on Invalid file [{}]", Util::FuncName(), filePath.string());
		}

		ParseIStream(file);
	}

	void Parser::ParseFile(fs::path const& filePath, fs::path const& outFilePath) {
		std::ifstream file{filePath};
		if (!file.is_open()) {
			ARCALC_THROW(ArCalcException, "{} on Invalid file [{}]", Util::FuncName(), filePath.string());
		}

		std::ofstream outFile{outFilePath};
		ParseIStream(file, outFile);
	}

	void Parser::ParseIStream(std::istream& is) {
		ParseIStream(is, std::cout);
	}

	void Parser::ParseIStream(std::istream& is, std::ostream& resultOStream) {
		auto const streamBytes{IO::IStreamToString(is)};

		auto subParser = Parser{resultOStream};
		for (auto const& line : Str::SplitOn<std::string_view>(streamBytes, "\n", false)) {
			subParser.ParseLine(line);
		}
	}

	void Parser::ParseLine(std::string_view line) {
		IncrementLineNumber();

		m_CurrentLine = Str::Trim<std::string_view>(line);
		m_bSemiColon = !m_CurrentLine.empty() && (m_CurrentLine.back() == ';');

		if (m_bSemiColon) {
			m_CurrentLine = m_CurrentLine.substr(0, m_CurrentLine.size() - 1);
		}

		switch (auto const state{GetState()}; state) {
		case St::SelectionNewLine:
		case St::Val_SelectionNewLine: {
			if (m_CurrentLine.empty()) {
				break;
			}

			HandleConditionalBody(state == St::Val_SelectionNewLine || *m_bConditionRegister);
			break;
		}
		case St::Val_LineCollection: {
			try { m_pValidationSubParser->ParseLine(line); } 
			catch (ArCalcException& err) {
				err.SetLineNumber(m_pValidationSubParser->GetLineNumber() 
					              + m_FunMan.CurrHeaderLineNumber());
				throw;
			} 
			AddFunctionLine();
			break;
		}
		case St::Val_SubParser:
		default: {
			if (m_CurrentLine.empty()) {
				break;
			}

			HandleFirstToken();
			break;
		}
		}
	}

	void Parser::ListVarNames(std::string_view prefix = "") {
		constexpr auto Tab = "    ";

		auto& os{GetOStream()};
		IO::Output(os, "{\n");
		for (auto const& [name, value] : m_VarMap) {
			if (name != "_Last" && name.starts_with(prefix)) {
				IO::Print(os, "{}{} = {}\n", Tab, name, value);
			}
		}
		IO::Output(os, "}");
	}

	void Parser::SetOStream(std::ostream& toWhat) {
		m_pOutStream = &toWhat;
	}

	std::ostream& Parser::GetOStream() {
		return *m_pOutStream;
	}

	bool Parser::IsVisible(std::string_view varName) const {
		return m_VarMap.contains(std::string{varName});
	}

	double Parser::Deref(std::string_view varName) const {
		if (varName == "_Last") {
			ARCALC_THROW(ArCalcException, "The value of _Last may only be accessed throw GetLast");
		} else if (auto const it{m_VarMap.find(std::string{varName})}; it != m_VarMap.end()) {
			return it->second;
		}
		else ARCALC_THROW(ArCalcException, "Dereferencing invisible or invalid variable [{}]", varName);
	}

	double Parser::GetLast() const {
		return m_VarMap.at("_Last");
	}

	bool Parser::IsFunction(std::string_view funcName) const {
		return m_FunMan.IsDefined(funcName);
	}

	bool Parser::IsParsingFunction() const { 
		return GetState() == St::Val_LineCollection; 
	}

	double Parser::GetReturnValue() {
		ARCALC_ASSERT(m_ReturnValueRegister.has_value(), "Tried to get non-existant return value");
		auto const returnValue{m_ReturnValueRegister.value()};
		m_ReturnValueRegister.reset();

		return returnValue;
	}


	void Parser::HandleFirstToken() {
		auto const firstToken{Str::GetFirstToken(m_CurrentLine)};
		if (auto const keyword{Keyword::FromString(firstToken)}; !keyword) {
			// Assume it's a normal expression and show its result in the next line.
			HandleNormalExpression();
		} else switch (*keyword) { 
		using KT = KeywordType;
		case KT::Set:    HandleSetKeyword(); break;
		case KT::Last:   HandleNormalExpression(); break;
		case KT::List:   HandleListKeyword(); break;
		case KT::Func:   HandleFuncKeyword(); break;
		case KT::Return: HandleReturnKeyword(); break;
		case KT::If:
		case KT::Else:
		case KT::Elif:   HandleSelectionKeyword(); break;
		default:         ARCALC_UNREACHABLE_CODE();
		}
	}

	void Parser::SetVar(std::string_view name, double value) {
		m_VarMap.insert_or_assign(std::string{name}, value);
	}

	double Parser::GetVar(std::string_view name) const {
		if (auto const it{m_VarMap.find(std::string{name})}; it != m_VarMap.end()) {
			return it->second;
		}
		else ARCALC_ERROR("Use of unset variable {}", name);
	}

	double Parser::Eval(std::string_view exprString) {
		try { return PostfixMathEvaluator{m_VarMap, m_FunMan}.Eval(exprString); } 
		catch (ArCalcException& err) {
			err.SetLineNumber(GetLineNumber());
			throw;
		}
	}

	void Parser::HandleSetKeyword() {
		auto const state{GetState()}; 
		if (state != St::Default && state != St::Val_SubParser) {
			ARCALC_UNREACHABLE_CODE();
		} 

		AssertKeyword(Str::ChopFirstToken<std::string_view>(m_CurrentLine), KeywordType::Set);
		auto const name{Str::ChopFirstToken<std::string_view, std::string_view>(m_CurrentLine)};
		AssertIdentifier(name);

		auto const value{Eval(m_CurrentLine)};
		SetVar(name, value);

		if (state == St::Default && !IsLineEndsWithSemiColon()) {
			IO::Print(GetOStream(), "{} = {}\n", name, value);
		}
	}

	void Parser::HandleListKeyword() {
		if (GetState() == St::Val_SubParser)
			return;

		auto const tokens{Str::SplitOnSpaces(m_CurrentLine)};
		(tokens.size() < 2) ? ListVarNames() : ListVarNames(tokens[1]);
	}

	void Parser::HandleFuncKeyword() {
		if (GetState() == St::Val_SubParser) {
			ARCALC_ERROR("Found keyword [{}] in an invalid context (inside a function)", KeywordType::Func);
		}

		auto const tokens{Str::SplitOnSpaces(m_CurrentLine)};
		AssertKeyword(tokens[0], KeywordType::Func);
		
		if (tokens.size() == 1) {
			ARCALC_ERROR("Expected Function name, but found nothing");
		} 

		// Function name
		auto funcName = std::string_view{tokens[1]};
		AssertIdentifier(funcName);
		m_FunMan.BeginDefination(funcName, GetLineNumber());

		if (tokens.size() == 2) {
			ARCALC_ERROR("Expected at least one paramter, but found none");
		}
		
		// Function parameters
		for (auto const i : view::iota(2U, tokens.size())) {
			auto const& paramName{tokens[i]};
			if constexpr (false && paramName.ends_with("...")) {
				ARCALC_ASSERT(i == tokens.size() - 1, "Found '...' in the middle of the parameter list");
				ARCALC_ASSERT(paramName.size() > 3, "Found '...' but no variable name");
				
				auto const currParamName{std::string_view{paramName}.substr(0, paramName.size() - 3)};
				AssertIdentifier(currParamName);
				m_FunMan.AddVariadicParam(currParamName);
				break; // Not needed because of the asserts above
			} else {
				AssertIdentifier(paramName);
				m_FunMan.AddParam(paramName);
			}
		}

		// Check function body for syntax errors
		m_pValidationSubParser = std::make_unique<Parser>(GetOStream(), m_FunMan.CurrParamData(), true);
		m_pValidationSubParser->SetState(St::Val_SubParser);
		SetState(St::Val_LineCollection);
	}

	void Parser::HandleReturnKeyword() {
		switch (GetState()) {
		case St::Default: {
			if (!IsExecutingFunction()) {
				ARCALC_THROW(ArCalcException, "Found {} in global scope", KeywordType::Return);
			}

			AssertKeyword(Str::ChopFirstToken(m_CurrentLine), KeywordType::Return);
			if (m_CurrentLine.empty()) {
				m_ReturnValueRegister = {};
			} else {
				m_ReturnValueRegister = Eval(m_CurrentLine);
			}

			break;
		}
		case St::Val_SubParser: {
			AssertKeyword(Str::ChopFirstToken(m_CurrentLine), KeywordType::Return);
			if (!m_CurrentLine.empty()) {
				Eval(m_CurrentLine); // Make sure no exception is thrown
			}
			
			break;
		}
		default: ARCALC_UNREACHABLE_CODE();
		}
	}

	void Parser::AddFunctionLine() {
		// TODO: Handle end of function more generally.

		if (GetState() != St::Val_LineCollection) {
			ARCALC_UNREACHABLE_CODE();
		}

		m_FunMan.AddCodeLine(m_CurrentLine);
		if (m_CurrentLine.starts_with(Keyword::ToString(KeywordType::Return))) {
			m_FunMan.EndDefination();
			SetState(St::Default);
			m_pValidationSubParser.reset();
		}
	}

	void Parser::HandleSelectionKeyword() {
		// The flag must be turned on when creating the sub-parser for validation as well!!!
		ARCALC_ASSERT(IsExecutingFunction(), "Found selection statement in global scope");

		auto const state{GetState()};
		
		switch (auto const keyword{*Keyword::FromString(Str::ChopFirstToken(m_CurrentLine))}; keyword) {
		case KeywordType::Elif:
			ARCALC_EXPECT(m_bConditionRegister.has_value(), "Found a hanging [{}] keyword", keyword);
			[[fallthrough]];
		case KeywordType::If: {
			auto const [cond, statement] {ParseConditionalHeader(m_CurrentLine)};
			if (cond.empty()) {
				ARCALC_ERROR("Expected a condition after keyword [{}], but found nothing", keyword);
			}

			if (state == St::Default || state == St::Val_SubParser) {
				m_bConditionRegister = !m_bConditionalBlockExecuted && std::abs(Eval(cond) - 0.0) > 0.000001;
			} 
			else ARCALC_UNREACHABLE_CODE();

			if (statement.empty()) {
				SetState(state == St::Default ? St::SelectionNewLine 
					                          : St::Val_SelectionNewLine);
			} else {
				m_CurrentLine = statement;
				HandleConditionalBody(state == St::Val_SubParser || *m_bConditionRegister); 
			}

			break;
		}
		case KeywordType::Else: {
			ARCALC_EXPECT(m_bConditionRegister.has_value(), "Found a hanging [{}] keyword", keyword);
			if (state == St::Default || state == St::Val_SubParser) {
				m_bConditionRegister.value() = !m_bConditionalBlockExecuted;
			} 
			else ARCALC_UNREACHABLE_CODE();

			if (m_CurrentLine.empty()) {
				SetState(state == St::Default ? St::SelectionNewLine 
					                          : St::Val_SelectionNewLine);
			}
			else HandleConditionalBody(state == St::Val_SubParser || *m_bConditionRegister);
			break;
		}
		default: ARCALC_UNREACHABLE_CODE();
		}
	}

	void Parser::HandleConditionalBody(bool bExecute) {
		auto const state{GetState()}; // This cache is necessary!!!
		if (!( state == St::Default                // For same line statements.
			|| state == St::SelectionNewLine       // For next line statements.
			|| state == St::Val_SubParser          // For validation of same line statements.
			|| state == St::Val_SelectionNewLine)) // For validation of next line statements.
		{
			ARCALC_UNREACHABLE_CODE();
		}

		if (bExecute) {
			m_bConditionalBlockExecuted = true;
			SetState(St::Default); // Reset the state so the statement executes without any problems.
			HandleFirstToken();
		}

		SetState(state == St::SelectionNewLine ? St::Default         // Finish the block.
			                                   : St::Val_SubParser); // Continue the validation process.
		/*

			TODO: Add return keyword.

		*/
	}

	Parser::ConditionAndStatement Parser::ParseConditionalHeader(std::string_view header) {
		// The condition ends when a keyword not valid in the middle of the header is found, 
		// however, some keywords are not valid inside an if statement in general, the 
		// condition stops when it finds these keywords as well and puts out a helpful error 
		// message.

		// TODO: add the python colon after an if and elif condition, make it optional 
		//       for else blocks.
		static std::vector<std::pair<std::string_view, bool>> const sc_PossibleKeywords{
			{Keyword::ToString(KeywordType::Set),    true},
			{Keyword::ToString(KeywordType::List),   true},
			{Keyword::ToString(KeywordType::If),     false},
			{Keyword::ToString(KeywordType::Elif),   false},
			{Keyword::ToString(KeywordType::Else),   false},
			{Keyword::ToString(KeywordType::Func),   false},
			{Keyword::ToString(KeywordType::Return), false},
		};

		for (auto const& [kw, bValid] : sc_PossibleKeywords) {
			if (auto condEndIndex{header.find(kw)}; condEndIndex != std::string::npos) {
				if (!bValid) {
					ARCALC_ERROR("Found keyword [{}] in an invalid context (inside a condition)", kw);
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
		if (state != St::Default && state != St::Val_SubParser) {
			ARCALC_UNREACHABLE_CODE();
		}

		auto const res{Eval(m_CurrentLine)};
		if (state == St::Default) {
			IO::Print(GetOStream(), "{}\n", res);
			SetVar("_Last", res);
		} 
	}

	void Parser::AssertKeyword(std::string_view glyph, KeywordType type) {
		auto const tokenOpt{Keyword::FromString(glyph)};
		ARCALC_ASSERT(tokenOpt && *tokenOpt == type, "Expected keyword [{}]", type);
	}

	void Parser::AssertIdentifier(std::string_view what) {
		ARCALC_ASSERT(!std::isdigit(what[0]), "Invalid identifier ({}); found digit [{}]", 
			what, what[0]);
		ARCALC_ASSERT(!Keyword::IsValid(what), "Expected identifier, but found keyword ({})", 
			what);
		ARCALC_ASSERT(!MathConstant::IsValid(what), 
			"Expected identifier, but found math constant ({})", what);

		for (auto const c : what) {
			if (!Str::IsAlNum(c) && c != '_') {
				ARCALC_ERROR("Found invalid character [{}] in indentifier [{}]", c, what);
			}
		}
	}
}