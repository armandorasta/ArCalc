#include "Parser.h"
#include "KeywordType.h"
#include "Util/Util.h"
#include "PostfixMathEvaluator.h"
#include "Util/FunctionManager.h"
#include "Util/Str.h"
#include "Util/IO.h"
#include "Util/MathConstant.h"
#include "Util/MathOperator.h"

namespace ArCalc {
	// Used during validation of function bodies.
	constexpr size_t ValidationBit{1U << 31}; 

	// Used during NOT validation of function bodies
	constexpr size_t ExecutionBit {1U << 30}; 

	// Used for selection statements. 
	// There are 4 of these, but they should have been 6.
	constexpr size_t SelectionBit {1U << 29}; 

	enum class Parser::St : size_t {
		// Rest.
		Default = 0U,

		// Function validation.
		Val_LineCollection = ValidationBit | 1,
		Val_SubParser      = ValidationBit | 2,
		Val_IfElifNewLine  = ValidationBit | SelectionBit | 1,
		Val_ElseNewLine    = ValidationBit | SelectionBit | 2,

		// Selections statements.
		IfElifNewLine      = ExecutionBit | SelectionBit | 1,
		ElseNewLine        = ExecutionBit | SelectionBit | 2,
	};

	bool Parser::IsExecSt(St st) {
		return static_cast<std::underlying_type_t<St>>(st) & ExecutionBit;
	}

	bool Parser::IsValSt(St st) {
		return static_cast<std::underlying_type_t<St>>(st) & ValidationBit;
	}

	bool Parser::IsSelSt(St st) {
		return static_cast<std::underlying_type_t<St>>(st) & SelectionBit;
	}

	Parser::Parser(std::ostream& os) 
		: m_CurrState{St::Default}, m_pOutStream{&os}, m_FunMan{os}, m_LitMan{os}
	{
		m_LitMan.SetLast(0.0);
	}

	Parser::Parser(std::ostream& os, std::vector<ParamData>& paramData, bool bValidation) : Parser{os} {
		m_bInFunction = true;

#ifndef NDEBUG
		for (auto const& param : paramData) {
			ExpectIdentifier(param.GetName());
		}
#endif // ^^^^ Debug mode only.

		if (bValidation) {
			// Validation will make all parameter packs have size 1 for now, because the sub-parser 
			// will evaluate all branches anyway.

			// The evaluator, however, is still unable to deal with parameter packs...
			// TODO: fix that.
			for (auto const& param : paramData) {
				if (param.IsPassedByRef()) {
					m_LitMan.Add(param.GetName(), param.GetRef());
				} 
				else m_LitMan.Add(param.GetName(), 0.f);
			}
		} else for (auto const& param : paramData) {
			if (param.IsPassedByRef()) {
				m_LitMan.Add(param.GetName(), param.GetRef());
			} else {
				m_LitMan.Add(param.GetName(), param.GetValue());

				if (param.IsParameterPack()) {
					ARCALC_NOT_IMPLEMENTED("Parameter packs");
					// Avoid doublicating the first argument in the pack.
					//                            vvvv
					// for (auto const i : view::iota(1U, param.Values.size())) {
					// 	m_LitMan.SetLiteralByValue(Str::Mangle(param.Name, i), param.Values[i]);
					// }
				}
			}
		}
	}

	void Parser::ParseFile(fs::path const& filePath) {
		if (std::ifstream file{filePath}; file.is_open()) {
			ParseIStream(file);
		}
		else throw ParseError{"{} on Invalid file [{}]", Util::FuncName(), filePath.string()};
	}

	void Parser::ParseFile(fs::path const& filePath, fs::path const& outFilePath) {
		if (std::ifstream file{filePath}; !file.is_open()) {
			std::ofstream outFile{outFilePath};
			ParseIStream(file, outFile);
		}
		else throw ParseError{"{} on Invalid file [{}]", Util::FuncName(), filePath.string()};
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
		m_bJustHitReturn = {}; // Reset the return statement flag.

		m_CurrentLine = Str::Trim<std::string_view>(line);
		m_bSemiColon = !m_CurrentLine.empty() && (m_CurrentLine.back() == ';');

		if (m_bSemiColon) {
			m_CurrentLine = m_CurrentLine.substr(0, m_CurrentLine.size() - 1);
		}

		switch (auto const state{GetState()}; state) {
		case St::Val_LineCollection: {
			try { m_pValidationSubParser->ParseLine(line); } 
			catch (ArCalcException& err) {
				err.SetLineNumber(m_pValidationSubParser->GetLineNumber() + 
					m_FunMan.CurrHeaderLineNumber());
				throw;
			} 
			AddFunctionLine();
			break;
		}
		case St::IfElifNewLine:
		case St::Val_IfElifNewLine: {
			if (m_CurrentLine.empty()) {
				break;
			}

			HandleConditionalBody(KeywordType::If, state == St::Val_IfElifNewLine 
				|| !m_bSelectionBlockExecuted 
				   && m_bConditionRegister.value_or(true)); // _Else == _Elif 1 (not exactly tho)
			break;
		}
		case St::ElseNewLine:
		case St::Val_ElseNewLine: {
			if (m_CurrentLine.empty()) {
				break;
			}

			HandleConditionalBody(KeywordType::Else, state == St::Val_IfElifNewLine 
				|| !m_bSelectionBlockExecuted 
				   && m_bConditionRegister.value_or(true)); // _Else == _Elif 1 (not exactly tho)
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

	void Parser::SetOStream(std::ostream& toWhat) {
		m_pOutStream = &toWhat;
	}

	std::ostream& Parser::GetOStream() {
		return *m_pOutStream;
	}

	bool Parser::IsParsingFunction() const { 
		return GetState() == St::Val_LineCollection; 
	}

	bool Parser::IsParsingSelectionStatement() const {
		return m_pValidationSubParser // Validating the function body while parsing?
			&& IsSelSt(m_pValidationSubParser->GetState());
	}

#ifdef NDEBUG
	std::optional<double> Parser::GetReturnValue([[maybe_unused]] FuncReturnType retype) 
#else // ^^^^ Release, vvvv Debug.
	std::optional<double> Parser::GetReturnValue(FuncReturnType retype) 
#endif
	{
#ifndef NDEBUG // retype is not checked in Release.
		if (retype == FuncReturnType::None && m_ReturnValueRegister.has_value()) {
			throw ParseError{
				"Invalid function return type: expected {} but found {}",
				FuncReturnType::Number, retype
			};
		} else if (retype == FuncReturnType::Number && !m_ReturnValueRegister.has_value()) {
			throw ParseError{
				"Invalid function return type: expected {} but found {}",
				FuncReturnType::None, retype
			};
		}
#endif // ^^^^ Debug mode only.
		return std::exchange(m_ReturnValueRegister, {});
	}

	void Parser::ExceptionReset() {
		// m_LineNumber += 1;
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
		case KT::Save:   HandleSaveKeyword(); break;
		case KT::Load:   HandleLoadKeyword(); break;
		default:         ARCALC_UNREACHABLE_CODE();
		}
	}

	std::optional<double> Parser::Eval(std::string_view exprString) {
		try { return PostfixMathEvaluator{m_LitMan, m_FunMan}.Eval(exprString); } 
		catch (ArCalcException& err) {
			err.SetLineNumber(GetLineNumber());
			throw;
		}
	}

	void Parser::HandleSetKeyword() {
		auto const state{GetState()}; 
		ARCALC_NOT_POSSIBLE(state != St::Default && state != St::Val_SubParser && !IsSelSt(state));
		KeywordDebugDoubleCheck(Str::ChopFirstToken<std::string_view>(m_CurrentLine), KeywordType::Set);

		auto const litName{Str::ChopFirstToken<std::string_view>(m_CurrentLine)};
		ExpectIdentifier(litName);

		if (MathConstant::IsValid(litName) ||
			MathOperator::IsValid(litName) ||
			m_FunMan.IsDefined(litName)) 
		{
			throw SyntaxError{
				"Can not define a literal with the name {}, "
				"because {} with that name already exists.",
				MathConstant::IsValid(litName) ? "a math constant" : 
				MathOperator::IsValid(litName) ? "an operator" : 
				                                 "you have already defined a function",
				litName,
			};
		} 

		auto const value = [&] {
			if (auto const opt{Eval(m_CurrentLine)}; opt.has_value()) {
				return *opt;
			} else throw ParseError{
				"Setting literal [{}] to an expression returns none.\n"
				"The expression was most probably a function returns None",
				litName
			};
		}();

		if (m_LitMan.IsVisible(litName)) { *m_LitMan.Get(litName) = value; } 
		else                             { m_LitMan.Add(litName, value); }

		if (!IsLineEndsWithSemiColon() && (state == St::Default || IsSelSt(state))) {
			IO::Print(GetOStream(), "{} = {}\n", litName, value);
		}
	}

	void Parser::HandleListKeyword() {
		if (auto const state{GetState()}; 
			state == St::Val_SubParser || IsSelSt(state)) 
		{
			return;
		}

		auto const tokens{Str::SplitOnSpaces(m_CurrentLine)};
		IO::Output(GetOStream(), "{");
		if (tokens.size() < 2) { 
			m_LitMan.List();
			m_FunMan.List();
		} else { 
			m_LitMan.List(tokens[1]); 
			m_FunMan.List(tokens[1]);
		}
		IO::Output(GetOStream(), "\n}\n");
	}

	void Parser::HandleFuncKeyword() {
		if (GetState() == St::Val_SubParser) {
			throw SyntaxError{
				"Found keyword [{}] in an invalid context (inside a function)", 
				KeywordType::Func,
			};
		}

		auto const tokens{Str::SplitOnSpaces<std::string_view>(m_CurrentLine)};
		KeywordDebugDoubleCheck(tokens.front(), KeywordType::Func);
		
		if (tokens.size() == 1) {
			throw ParseError{
				"Expected Function name after {0} keyword, but found nothing.\n"
				"{0} [function name] [parameter list]",
				KeywordType::Func
			};
		} 

		// Function name
		auto& funcName = tokens[1];
		ExpectIdentifier(funcName);

		if (MathConstant::IsValid(funcName) ||
			MathOperator::IsValid(funcName) ||
			m_LitMan.IsVisible(funcName)) 
		{
			throw SyntaxError{
				"Can not define a function with the name {}, "
				"because {} with that name already exists.",
				MathConstant::IsValid(funcName) ? "a math constant" : 
				MathOperator::IsValid(funcName) ? "an operator" : 
				                                  "you have already defined a variable",
				funcName,
			};
		} 

		m_FunMan.BeginDefination(funcName, GetLineNumber());

		if (tokens.size() == 2) {
			throw ParseError{
				"Expected at least one parameter after the name of the function, but found none.\n"
				"{} [function name] [parameter list]",
				KeywordType::Func
			};
		}
		
		// Function parameters
		for (auto const i : view::iota(2U, tokens.size())) {
			auto const& paramName{tokens[i]};
			if (paramName.front() == '&') {
				if (paramName.size() < 2) { // More helpful error message.
					throw SyntaxError{
						"There may not be any space between the & and the name of a by-reference parameter"
					};
				}

				auto const currParamName{std::string_view{paramName}.substr(1)};
				ExpectIdentifier(currParamName);
				m_FunMan.AddRefParam(currParamName);
			} else if (paramName.ends_with("...")) {
				ARCALC_NOT_IMPLEMENTED("Parameter packs ");
				// ARCALC_DA(i == tokens.size() - 1, "Found '...' in the middle of the parameter list");
				// ARCALC_DA(paramName.size() > 3, "Found '...' but no variable name");
				// 
				// auto const currParamName{std::string_view{paramName}.substr(0, paramName.size() - 3)};
				// AssertIdentifier(currParamName);
				// m_FunMan.AddVariadicParam(currParamName);
				// break; // Not needed because of the asserts above
			} else {
				ExpectIdentifier(paramName);
				m_FunMan.AddParam(paramName);
			}
		}

		// Check function body for syntax errors.
		m_pValidationSubParser = std::make_unique<Parser>(m_ValidationStringStream, m_FunMan.CurrParamData(), true);
		m_pValidationSubParser->SetState(St::Val_SubParser);
		SetState(St::Val_LineCollection);
	}

	void Parser::HandleReturnKeyword() {
		auto currLine{m_CurrentLine}; // The actual view need not be touched.
		KeywordDebugDoubleCheck(Str::ChopFirstToken<std::string_view>(currLine), KeywordType::Return);
		m_bJustHitReturn = true;

		auto const state{GetState()}; 
		m_bFuncMustExist = (state == St::Val_SubParser);

		if (IsValSt(state)) {
			auto const currReturnType = 
				currLine.empty() ? FuncReturnType::None : FuncReturnType::Number;

			if (!m_ReturnTypeRegister) {
				m_ReturnTypeRegister = currReturnType;
			} else if (currReturnType != m_ReturnTypeRegister) {
				throw SyntaxError{
					"Different return types; expected {} but found {}",
					*m_ReturnTypeRegister, currReturnType
				};
			}
		}

		switch (state) {
		case St::Default: {
			if (!IsExecutingFunction()) {
				throw SyntaxError{"Found {} in global scope", KeywordType::Return};
			}

			if (currLine.empty()) { m_ReturnValueRegister = {}; } 
			else                  { m_ReturnValueRegister = Eval(currLine); }

			break;
		}
		case St::Val_SubParser: {
			if (!currLine.empty()) {
				Eval(currLine); // Just make sure no exception is thrown.
			}
			
			break;
		}
		default: ARCALC_UNREACHABLE_CODE();
		}
	}

	void Parser::AddFunctionLine() {
		ARCALC_NOT_POSSIBLE(GetState() != St::Val_LineCollection);

		m_FunMan.AddCodeLine(std::string{m_CurrentLine} + (IsLineEndsWithSemiColon() ? ";" : ""));

		if (auto& subParser{*m_pValidationSubParser}; subParser.m_bFuncMustExist) {
			ARCALC_NOT_POSSIBLE(!subParser.m_ReturnTypeRegister);
			m_FunMan.SetReturnType(*subParser.m_ReturnTypeRegister);
			subParser.m_ReturnTypeRegister = {};

			m_FunMan.EndDefination();
			SetState(St::Default);
			m_pValidationSubParser.reset();
		}
	}

	void Parser::HandleSelectionKeyword() {
		if (!IsExecutingFunction()) {
			// The flag must be turned on when creating the sub-parser for validation as well!!!
			throw SyntaxError{"Found selection statement in global scope"};
		}

		auto const state{GetState()};
		auto const keyword{*Keyword::FromString(Str::ChopFirstToken(m_CurrentLine))};
		if (IsSelSt(state)) {
			throw SyntaxError{
				"Found selection keyword [{}] inside a selection statement.\n"
				"Selection statements may not be nested inside one another",
				keyword,
			};
		}

		switch (keyword) {
		case KeywordType::Elif:
			if (!m_bConditionRegister.has_value()) {
				throw SyntaxError{"Found a hanging [{}] keyword", keyword};
			}
			[[fallthrough]];
		case KeywordType::If: {
			ARCALC_NOT_POSSIBLE(!(state == St::Default || state == St::Val_SubParser));

			auto const [cond, statement] {ParseConditionalHeader(keyword, m_CurrentLine)};
			if (cond.empty()) {
				throw ParseError{
					"Expected a condition after keyword [{}], but found nothing", 
					keyword
				};
			}

			if (auto const opt{Eval(cond)}; opt) {
				m_bConditionRegister = std::abs(*opt) > 0.000001;
			}
			else throw SyntaxError{"Found expression returns none in condition"};

			if (statement.empty()) {
				SetState(state == St::Default ? St::IfElifNewLine 
				                              : St::Val_IfElifNewLine);
			} else {
				m_CurrentLine = statement;
				HandleConditionalBody(keyword, state == St::Val_SubParser || *m_bConditionRegister); 
			}

			break;
		}
		case KeywordType::Else: {
			ARCALC_NOT_POSSIBLE(!(state == St::Default || state == St::Val_SubParser));
			auto const [cond, statement] {ParseConditionalHeader(keyword, m_CurrentLine)};
			
			if (!m_bConditionRegister.has_value()) {
				throw SyntaxError{"Found a hanging [{}] keyword", keyword};
			}
			m_bConditionRegister.reset(); // This disallows any elif's after this branch.

			if (m_CurrentLine.empty()) {
				SetState(state == St::Default ? St::ElseNewLine 
											  : St::Val_ElseNewLine);
			} else {
				m_CurrentLine = statement;
				HandleConditionalBody(keyword, state == St::Val_SubParser || !m_bSelectionBlockExecuted);
			}
			break;
		}
		default: ARCALC_UNREACHABLE_CODE();
		}
	}

	void Parser::HandleConditionalBody(KeywordType selKW, bool bExecute) {
		auto const state{GetState()}; // This cache is necessary!!!
		ARCALC_NOT_POSSIBLE(!(state == St::Default 
		                   || state == St::Val_SubParser 
		                   || IsSelSt(state)));
		
		// indicates that the current statement is a return statement.
		auto const bReturn{m_CurrentLine.starts_with(Keyword::ToString(KeywordType::Return))};
		if (selKW == KeywordType::If) {
			m_bAllOtherBranchesReturned = bReturn; // Starts the chain.
		} else {
			m_bAllOtherBranchesReturned &= bReturn;
		}

		if (bExecute) {
			m_bSelectionBlockExecuted = true;
			HandleFirstToken();
		}

		m_bFuncMustExist = (state == St::Val_SubParser || IsSelSt(state)) 
		                 && m_bAllOtherBranchesReturned 
		                 && selKW == KeywordType::Else;

		SetState([&] {
			switch (state) {
			case St::IfElifNewLine:
			case St::ElseNewLine: 
				return St::Default;
			case St::Val_IfElifNewLine:
			case St::Val_ElseNewLine:
				return St::Val_SubParser;
			default:
				return state;
			}
		}(/*)(*/));
	}

	Parser::ConditionAndStatement Parser::ParseConditionalHeader(KeywordType selKW, std::string_view header) {
		switch (selKW) {
		case KeywordType::If:
		case KeywordType::Elif: {
			if (auto const colonIndex{header.find(':')}; colonIndex == std::string::npos) {
				throw SyntaxError{
					"Expected a `:` terminating condition.\n"
					"[_if / _Elif] [condition]: [body]",
				};
			} else if (colonIndex + 1 == header.size()) { // Statement is next line.
				return {
					.Condition{header.substr(0, colonIndex)},
					.Statement{},
				};
			} else return { // Inline statement.
				.Condition{header.substr(0, colonIndex)},
				.Statement{Str::TrimLeft<std::string_view>(header.substr(colonIndex + 1))},
			};

			break;
		}
		case KeywordType::Else: {
			return { // Inline statement.
				.Condition{},
				.Statement{Str::TrimLeft<std::string_view>(header)},
			};
		}
		default: 
			ARCALC_UNREACHABLE_CODE();
			return {};
		}
	}

	void Parser::HandleSaveKeyword() {
		auto const tokens{Str::SplitOnSpaces<std::string_view>(m_CurrentLine)};
		KeywordDebugDoubleCheck(tokens.front(), KeywordType::Save);

		if (tokens.size() > 3) {
			throw ParseError{
				"Too many tokens in line, expected only the target name, and the category name"
			};
		} else if (tokens.size() < 2) {
			throw ParseError{"Expected name of target to be saved, but found nothing"};
		}

		auto const& targetName{tokens[1]};
		ExpectIdentifier(targetName);
		
		if (tokens.size() < 3) {
			throw ParseError{"Expected name of category, but found nothing"};
		}

		auto const& categoryName{tokens[2]};
		ExpectIdentifier(categoryName);

		auto const saveDir{IO::GetSerializationPath()}; 
		fs::create_directory(saveDir);
		std::ofstream file{
			saveDir / std::string{categoryName}.append(".txt"),
			std::ios::app,
		};

		if (m_LitMan.IsVisible(targetName)) {
			m_LitMan.Serialize(targetName, file);
		} else if (m_FunMan.IsDefined(targetName)) {
			m_FunMan.Serialize(targetName, file);
		} 
		else throw ParseError{"Tried to save '{}' which does not refer to anything", targetName};
	}

	void Parser::HandleLoadKeyword() {
		auto const tokens{Str::SplitOnSpaces<std::string_view>(m_CurrentLine)};
		KeywordDebugDoubleCheck(tokens.front(), KeywordType::Load);

		if (tokens.size() > 2) {
			throw ParseError{
				"Too many tokens in line, expected only the category name"
			};
		} else if (tokens.size() < 2) {
			throw ParseError{"Expected name of category, but found nothing"};
		}
		auto const& categoryName{tokens[1]};
		auto const categoryFullPath{IO::GetSerializationPath() / std::string{categoryName}.append(".txt")};
		if (!fs::exists(categoryFullPath)) {
			throw ParseError{"Loading non-existant category [{}]", categoryName};
		}

		std::ifstream file{categoryFullPath, std::ios::app};
		if (!file.is_open()) {
			throw ParseError{"Loading non-existant category [{}]", categoryName};
		}

		for (bool bQuit{}; !(bQuit || file.eof());) switch (IO::Input<char>(file)) {
			case 'C':   m_LitMan.Deserialize(file); break;
			case 'F':   m_FunMan.Deserialize(file); break;
			case '\n':  break;
			case '\0':  bQuit = true; break;
			default: {
				throw ParseError{
					"File deserialization failed; expected either C or F at the begining of the line"
				};
			}
		}
	}

	void Parser::HandleNormalExpression() {
		auto const state{GetState()};
		ARCALC_NOT_POSSIBLE(!(state == St::Default 
		                   || state == St::Val_SubParser 
		                   || IsSelSt(state)));

		auto const opt{Eval(m_CurrentLine)};
		if (state == St::Default) {
			if (opt.has_value()) {
				auto const res{*opt};
				IO::Print(GetOStream(), "{}\n", res);
				m_LitMan.SetLast(res);
			} 
			else IO::Print(GetOStream(), "None.\n");
		} 
	}

	void Parser::ExpectKeyword(std::string_view glyph, KeywordType type) {
		if (auto const tokenOpt{Keyword::FromString(glyph)}; !tokenOpt || *tokenOpt != type) {
			throw ParseError{"Expected keyword [{}]", type};
		}
	}

#ifdef NDEBUG
	void Parser::KeywordDebugDoubleCheck([[maybe_unused]] std::string_view glyph, 
		[[maybe_unused]] KeywordType what) 
#else // ^^^^ Release, vvvv Debug.
	void Parser::KeywordDebugDoubleCheck(std::string_view glyph, KeywordType what) 
#endif
	{
#ifndef NDEBUG
		return ExpectKeyword(glyph, what);
#endif // ^^^^ Debug mode only.
	}

	void Parser::ExpectIdentifier(std::string_view what) {
		if (std::isdigit(what[0])) {
			throw SyntaxError{
				"Invalid identifier ({}); found digit [{}]", what, what[0]
			};
		} else if (Keyword::IsValid(what)) {
			throw SyntaxError{"Expected identifier, but found keyword ({})", what};
		} else if (MathConstant::IsValid(what)) {
			throw SyntaxError{
				"Expected identifier, but found math constant ({})", what
			};
		}

		for (auto const c : what) {
			if (!Str::IsAlNum(c) && c != '_') {
				throw SyntaxError{"Found invalid character [{}] in indentifier [{}]", c, what};
			}
		}
	}

	void Parser::Reset() {
		m_FunMan.Reset();
		m_LitMan.Reset();
		m_CurrentLine = {};
		m_CurrState = St::Default;
		m_LineNumber = {};
		m_bSemiColon = {};
		m_bSelectionBlockExecuted = {};
		m_bConditionRegister.reset();
		m_ReturnValueRegister.reset();
		m_pValidationSubParser.reset();
	}
}