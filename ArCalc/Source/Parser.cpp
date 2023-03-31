#include "Parser.h"
#include "KeywordType.h"
#include "Util/Util.h"
#include "PostfixMathEvaluator.h"
#include "Util/FunctionManager.h"
#include "Util/Str.h"
#include "Util/IO.h"
#include "Util/MathConstant.h"

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
		} 
		else for (auto const& param : paramData) {
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
		std::ifstream file{filePath};
		if (!file.is_open()) {
			throw ParseError{"{} on Invalid file [{}]", Util::FuncName(), filePath.string()};
		}

		ParseIStream(file);
	}

	void Parser::ParseFile(fs::path const& filePath, fs::path const& outFilePath) {
		std::ifstream file{filePath};
		if (!file.is_open()) {
			throw ParseError{"{} on Invalid file [{}]", Util::FuncName(), filePath.string()};
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
		case St::SelectionNewLine:
		case St::Val_SelectionNewLine: {
			if (m_CurrentLine.empty()) {
				break;
			}
			
			HandleConditionalBody(state == St::Val_SelectionNewLine 
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
		return GetState() == St::Val_SelectionNewLine; 
	}

	std::optional<double> Parser::GetReturnValue([[maybe_unused]] FuncReturnType retype) {
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
		ARCALC_NOT_POSSIBLE(!(state == St::Default || state == St::Val_SubParser));

		KeywordDebugDoubleCheck(Str::ChopFirstToken<std::string_view>(m_CurrentLine), KeywordType::Set);

		auto const name{Str::ChopFirstToken<std::string_view>(m_CurrentLine)};
		ExpectIdentifier(name);

		auto const value = [&] {
			if (auto const opt{Eval(m_CurrentLine)}; opt.has_value()) {
				return *opt;
			} else throw ParseError{
				"Setting literal [{}] to an expression returns none.\n"
				"The expression was most probably a function returns None",
				name
			};
		}();

		if (m_LitMan.IsVisible(name)) { *m_LitMan.Get(name) = value; } 
		else                          { m_LitMan.Add(name, value); }

		if (state == St::Default && !IsLineEndsWithSemiColon()) {
			IO::Print(GetOStream(), "{} = {}\n", name, value);
		}
	}

	void Parser::HandleListKeyword() {
		if (GetState() == St::Val_SubParser) {
			return;
		}

		auto const tokens{Str::SplitOnSpaces(m_CurrentLine)};
		if (tokens.size() < 2) { m_LitMan.List(); }
		else                   { m_LitMan.List(tokens[1]); }
	}

	void Parser::HandleFuncKeyword() {
		if (GetState() == St::Val_SubParser) {
			throw SyntaxError{
				"Found keyword [{}] in an invalid context (inside a function)", 
				KeywordType::Func
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
		m_FunMan.BeginDefination(funcName, GetLineNumber());

		if (tokens.size() == 2) {
			throw ParseError{
				"Expected at least one parameter after the name of the function, but found none"
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
		m_pValidationSubParser = std::make_unique<Parser>(GetOStream(), m_FunMan.CurrParamData(), true);
		m_pValidationSubParser->SetState(St::Val_SubParser);
		SetState(St::Val_LineCollection);
	}

	void Parser::HandleReturnKeyword() {
		switch (GetState()) {
		case St::Default: {
			if (!IsExecutingFunction()) {
				throw SyntaxError{"Found {} in global scope", KeywordType::Return};
			}

			KeywordDebugDoubleCheck(Str::ChopFirstToken(m_CurrentLine), KeywordType::Return);

			if (m_CurrentLine.empty()) { m_ReturnValueRegister = {}; } 
			else                       { m_ReturnValueRegister = Eval(m_CurrentLine); }

			break;
		}
		case St::Val_SubParser: {
			ExpectKeyword(Str::ChopFirstToken(m_CurrentLine), KeywordType::Return);

			if (!m_CurrentLine.empty()) {
				Eval(m_CurrentLine); // Just make sure no exception is thrown.
			}
			
			break;
		}
		default: ARCALC_UNREACHABLE_CODE();
		}
	}

	void Parser::AddFunctionLine() {
		// TODO: Handle end of function more generally.
		ARCALC_NOT_POSSIBLE(GetState() != St::Val_LineCollection);

		if (IsLineEndsWithSemiColon()) {
			m_FunMan.AddCodeLine(std::string{m_CurrentLine} + ";");
		} else {
			m_FunMan.AddCodeLine(m_CurrentLine);
		}
		if (auto const retKW{Keyword::ToString(KeywordType::Return)}; m_CurrentLine.starts_with(retKW)) {
			// This is a hack, it depends on AddFunctionLine being called after the line is trimmed.
			m_FunMan.SetReturnType(m_CurrentLine.size() > retKW.size() ? FuncReturnType::Number 
				                                                       : FuncReturnType::None);
			m_FunMan.EndDefination();
			SetState(St::Default);
			m_pValidationSubParser.reset();
		}
	}

	void Parser::HandleSelectionKeyword() {
		if (!IsExecutingFunction()) {
			// The flag must be turned on when creating the sub-parser for validation as well!!!
			throw ParseError{"Found selection statement in global scope"};
		}
		auto const state{GetState()};

		switch (auto const keyword{*Keyword::FromString(Str::ChopFirstToken(m_CurrentLine))}; keyword) {
		case KeywordType::Elif:
			if (!m_bConditionRegister.has_value()) {
				throw ParseError{"Found a hanging [{}] keyword", keyword};
			}
			[[fallthrough]];
		case KeywordType::If: {
			ARCALC_NOT_POSSIBLE(!(state == St::Default || state == St::Val_SubParser));

			auto const [cond, statement] {ParseConditionalHeader(m_CurrentLine)};
			if (cond.empty()) {
				throw ParseError{
					"Expected a condition after keyword [{}], but found nothing", 
					keyword
				};
			}

			if (auto const opt{Eval(cond)}; opt.has_value()) {
				m_bConditionRegister = std::abs(*opt - 0.0) > 0.000001;
			}
			else throw ParseError{"Found expression returns none in condition"};

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
			ARCALC_NOT_POSSIBLE(!(state == St::Default || state == St::Val_SubParser));
			if (!m_bConditionRegister.has_value()) {
				throw ParseError{"Found a hanging [{}] keyword", keyword};
			}
			m_bConditionRegister.reset(); // This disallows any elif's after this branch.

			if (m_CurrentLine.empty()) {
				SetState(state == St::Default ? St::SelectionNewLine 
											  : St::Val_SelectionNewLine);
			}
			else HandleConditionalBody(state == St::Val_SubParser || !m_bSelectionBlockExecuted);
			break;
		}
		default: ARCALC_UNREACHABLE_CODE();
		}
	}

	void Parser::HandleConditionalBody(bool bExecute) {
		auto const state{GetState()}; // This cache is necessary!!!
		ARCALC_NOT_POSSIBLE(!( state == St::Default       || state == St::SelectionNewLine
							|| state == St::Val_SubParser || state == St::Val_SelectionNewLine));

		if (bExecute) {
			m_bSelectionBlockExecuted = true;
			//SetState(St::Default); // Reset the state so the statement executes without any problems.
			HandleFirstToken();
		}

		SetState([&] {
			switch (state) {
			case St::SelectionNewLine:     return St::Default;
			case St::Val_SelectionNewLine: return St::Val_SubParser;
			default:                       return state;
			}
		}(/*)(*/));

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
		//       for else blocks, I mean, just look at this bullshit.
		for (static auto const sc_AllKeywordTypes{Keyword::GetAllKeywordTypes()}; 
			auto const kw : sc_AllKeywordTypes) 
		{
			if (auto condEndIndex{header.find(Keyword::ToString(kw))}; 
				condEndIndex != std::string::npos) 
			{
				if (kw != KeywordType::Set && kw != KeywordType::List) {
					throw ParseError{
						"Found keyword [{}] in an invalid context (inside a condition)", kw
					};
				}

				return {
					.Condition{header.substr(0, condEndIndex)},
					.Statement{header.substr(condEndIndex)}
				};
			}
		}

		return {.Condition{header}, .Statement{}};
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
		ARCALC_NOT_POSSIBLE(!(state == St::Default || state == St::Val_SubParser));

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

	void Parser::KeywordDebugDoubleCheck([[maybe_unused]] std::string_view glyph, 
		[[maybe_unused]] KeywordType what) 
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