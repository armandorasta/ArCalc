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
		Val_LineCollection        = ValidationBit | 1,
		Val_SubParser             = ValidationBit | 2,
		Val_UnscopeFunc           = ValidationBit | 3,
		Val_UnscopeIfElifSameLine = ValidationBit | 4,
		Val_UnscopeElseSameLine   = ValidationBit | 5,
		Val_UnscopeIfElifNewLine  = ValidationBit | 6,
		Val_UnscopeElseNewLine    = ValidationBit | 7,
		Val_IfNewLine             = ValidationBit | SelectionBit | 1,
		Val_ElifNewLine           = ValidationBit | SelectionBit | 2,
		Val_ElseNewLine           = ValidationBit | SelectionBit | 3,
		Val_IfSameLine            = ValidationBit | SelectionBit | 4,
		Val_ElifSameLine          = ValidationBit | SelectionBit | 5,
		Val_ElseSameLine          = ValidationBit | SelectionBit | 6,

		// Selections statements.
		IfNewLine      = ExecutionBit | SelectionBit | 1,
		ElifNewLine    = ExecutionBit | SelectionBit | 2,
		ElseNewLine    = ExecutionBit | SelectionBit | 3,
		IfSameLine     = ExecutionBit | SelectionBit | 4,
		ElifSameLine   = ExecutionBit | SelectionBit | 5,
		ElseSameLine   = ExecutionBit | SelectionBit | 6,
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

	void Parser::ConditionInfo::Unreset() {
		ARCALC_DA(!m_bAvail, "Unreset may only be called on reset condition");
		m_bAvail = true;
	}

	bool Parser::ConditionInfo::Get() const {
		ARCALC_DA(IsAvail(), "Tried to get value of condition was already reset");
		return m_bCond;
	}

	Parser::Parser(std::ostream& os) 
		: m_CurrState{St::Default}, m_pOutStream{&os}, m_FunMan{os}, m_LitMan{os}
	{
		m_LitMan.SetLast(0.0);
	}

	Parser::Parser(std::ostream& os, FunctionManager const& funMan, 
		LiteralManager::LiteralMap const& litMap) : Parser{os} 
	{
		m_bInFunction = true;
		m_FunMan.CopyMapFrom(funMan);
		m_LitMan.SetMap(litMap);
	}

	void Parser::ParseFile(fs::path const& filePath) {
		if (std::ifstream file{filePath}; file.is_open()) {
			ParseIStream(file);
		} else {
			throw ParseError{"Parser::ParseFile on Invalid file [{}]", filePath.string()};
		}
	}

	void Parser::ParseFile(fs::path const& filePath, fs::path const& outFilePath) {
		if (std::ifstream file{filePath}; file.is_open()) {
			std::ofstream outFile{outFilePath};
			ParseIStream(file, outFile);
		} else {
			throw ParseError{"Parser::ParseFile on Invalid file [{}]", filePath.string()};
		}
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
		m_bJustHitReturn = {}; // Reset the return statement flag.

		m_CurrentLine = Str::Trim<std::string_view>(line);
		m_bSemiColon = !m_CurrentLine.empty() && (m_CurrentLine.back() == ';');

		if (m_bSemiColon) {
			m_CurrentLine = m_CurrentLine.substr(0, m_CurrentLine.size() - 1);
		}

		switch (auto const state{GetState()}; state) {
		case St::Val_LineCollection:
			try { 
				m_pValSubParser->ParseLine(line); 
				switch (m_pValSubParser->GetState()) {
				case St::Val_UnscopeFunc:
					SetState(St::Default);
					m_pValSubParser.reset();
					m_FunMan.ResetCurrFunc();

					break;
				case St::Val_UnscopeIfElifNewLine:
					m_FunMan.RemoveLastLine();
					[[fallthrough]];
				case St::Val_UnscopeIfElifSameLine: // Do not add the current line.
					m_pValSubParser->SetState(St::Val_SubParser);
					break;
				case St::Val_UnscopeElseNewLine:
					m_FunMan.RemoveLastLine();
					[[fallthrough]];
				case St::Val_UnscopeElseSameLine: // Do not add the current line.
					if (!m_pValSubParser->m_bConditionRegister.IsAvail()) {
						m_pValSubParser->m_bConditionRegister.Unreset();
					}
					m_pValSubParser->SetState(St::Val_SubParser);
					break;
				default:
					AddFunctionLine();
				}
			} catch (ArCalcException& err) {
				err.SetLineNumber(m_pValSubParser->GetLineNumber() + 
					m_FunMan.CurrHeaderLineNumber());
				throw;
			} 

			break;
		case St::IfNewLine:
		case St::ElifNewLine:
		case St::ElseNewLine: 
		case St::Val_IfNewLine: 
		case St::Val_ElifNewLine: 
		case St::Val_ElseNewLine:
			if (m_CurrentLine.empty()) {
				break;
			}

			HandleConditionalBody(
				[=] {
					switch (state) {
					case St::IfNewLine:   
					case St::Val_IfNewLine:
						return KeywordType::If;
					case St::ElifNewLine: 
					case St::Val_ElifNewLine: 
						return KeywordType::Elif;
					case St::ElseNewLine: 
					case St::Val_ElseNewLine: 
						return KeywordType::Else;
					default: 
						ARCALC_UNREACHABLE_CODE();
					}
				}(/*)(*/),
				IsValSt(state) || !m_bSelectionBlockExecuted && m_bConditionRegister.ValueOr(true)
			);

			break;
		case St::Val_SubParser:
		default:
			if (m_CurrentLine.empty()) {
				break;
			}

			HandleFirstToken();
			break;
		}

		IncrementLineNumber();
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
		return m_pValSubParser // Validating the function body while parsing?
			&& IsSelSt(m_pValSubParser->GetState());
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
		IncrementLineNumber();
	}

	void Parser::ToggleOutput() {
		m_bSuppressOutput ^= 1;
		m_LitMan.ToggleOutput();
		m_FunMan.ToggleOutput();
	}

	void Parser::HandleFirstToken() {
		auto const firstToken{Str::GetFirstToken(m_CurrentLine)};
		if (auto const keyword{Keyword::FromString(firstToken)}; !keyword) {
			// Assume it's a normal expression and show its result in the next line.
			HandleNormalExpression();
		} else switch (*keyword) { 
		using KT = KeywordType;
		case KT::Set:     HandleSetKeyword(); break;
		case KT::Last:    HandleNormalExpression(); break;
		case KT::List:    HandleListKeyword(); break;
		case KT::Func:    HandleFuncKeyword(); break;
		case KT::Return:  HandleReturnKeyword(); break;
		case KT::If:
		case KT::Else:
		case KT::Elif:    HandleSelectionKeyword(); break;
		case KT::Save:    HandleSaveKeyword(); break;
		case KT::Load:    HandleLoadKeyword(); break;
		case KT::Unscope: HandleUnscopeKeyword(); break;
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
				                                 "a function",
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
		}(/*)(*/);

		if (m_LitMan.IsVisible(litName)) { *m_LitMan.Get(litName) = value; } 
		else                             { m_LitMan.Add(litName, value); }

		if (state == St::Default || IsSelSt(state)) {
			Print("{} = {}\n", litName, value);
		}
	}

	void Parser::HandleListKeyword() {
		if (auto const state{GetState()}; 
			state == St::Val_SubParser || IsSelSt(state)) 
		{
			return;
		}

		auto const tokens{Str::SplitOnSpaces(m_CurrentLine)};
		Print("{");
		if (tokens.size() < 2) { 
			m_LitMan.List();
			m_FunMan.List();
		} else { 
			m_LitMan.List(tokens[1]); 
			m_FunMan.List(tokens[1]);
		}
		Print("\n}\n");
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
				                                  "a literal",
				funcName,
			};
		} 

		if (tokens.size() == 2) {
			throw ParseError{
				"Expected at least one parameter after the name of the function, but found none.\n"
				"{} [function name] [parameter list]",
				KeywordType::Func
			};
		}

		// Defination should begin after all header validation is done.
		m_FunMan.BeginDefination(funcName, GetLineNumber());
		
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

		// Quite unfortunate this function has to exist, I am just too lazy to work out 
		// any other way -_-
		m_FunMan.TerminateAddingParams();

		auto const paramMap = [&] {
			// Validation will make all parameter packs have size 1 for now, because the sub-parser 
			// will evaluate all branches anyway.

			// The evaluator, however, is still unable to deal with parameter packs...
			// TODO: fix that.
			auto res = LiteralManager::LiteralMap{};
			for (auto const& param : m_FunMan.CurrParamData()) {
				if (param.IsPassedByRef()) {
					res.emplace(param.GetName(), LiteralData::MakeRef(param.GetRef()));
				} else {
					res.emplace(param.GetName(), LiteralData::Make(0.f));
				}
			}

			return res;
		}(/*)(*/);

		// Check function body for syntax errors.
		m_pValSubParser = std::make_unique<Parser>(std::cout, m_FunMan, paramMap);
		m_pValSubParser->ToggleOutput();
		m_pValSubParser->SetState(St::Val_SubParser);
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
		case St::Default: 
		case St::IfSameLine: 
		case St::ElifSameLine: 
		case St::ElseSameLine: {
			if (!IsExecutingFunction()) {
				throw SyntaxError{"Found {} in invalid context (global scope)", KeywordType::Return};
			}

			if (currLine.empty()) { m_ReturnValueRegister = {}; } 
			else                  { m_ReturnValueRegister = Eval(currLine); }

			break;
		}
		case St::Val_SubParser: 
		case St::Val_IfSameLine:
		case St::Val_ElifSameLine:
		case St::Val_ElseSameLine: {
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

		if (auto& subParser{*m_pValSubParser}; subParser.m_bFuncMustExist) {
			ARCALC_NOT_POSSIBLE(!subParser.m_ReturnTypeRegister);
			m_FunMan.SetReturnType(*subParser.m_ReturnTypeRegister);
			subParser.m_ReturnTypeRegister = {};

			m_FunMan.EndDefination();
			SetState(St::Default);
			m_pValSubParser.reset();
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
			if (!m_bConditionRegister.IsAvail()) {
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
				m_bConditionRegister.Set(std::abs(*opt) > 0.000001);
			}
			else throw SyntaxError{"Found expression returns none in condition"};

			if (statement.empty()) {
				SetState([=] {
					switch (state) {
					case St::Default:
						if (keyword == KeywordType::If) { return St::IfNewLine; }
						else                            { return St::ElifNewLine; }
					case St::Val_SubParser:
						if (keyword == KeywordType::If) { return St::Val_IfNewLine; }
						else                            { return St::Val_ElifNewLine; }
					default:
						ARCALC_UNREACHABLE_CODE();
					}
				}(/*)(*/));
			} else {
				m_CurrentLine = statement;
				SetState([=] {
					switch (state) {
					case St::Default:
						if (keyword == KeywordType::If) { return St::IfSameLine; }
						else                            { return St::ElifSameLine; }
					case St::Val_SubParser:
						if (keyword == KeywordType::If) { return St::Val_IfSameLine; }
						else                            { return St::Val_ElifSameLine; }
					default:
						ARCALC_UNREACHABLE_CODE();
					}
				}(/*)(*/));
				HandleConditionalBody(keyword, state == St::Val_SubParser || m_bConditionRegister.Get()); 
			}

			break;
		}
		case KeywordType::Else: {
			ARCALC_NOT_POSSIBLE(!(state == St::Default || state == St::Val_SubParser));
			auto const [cond, statement] {ParseConditionalHeader(keyword, m_CurrentLine)};
			
			if (!m_bConditionRegister.IsAvail()) {
				throw SyntaxError{"Found a hanging [{}] keyword", keyword};
			}
			m_bConditionRegister.Reset(); // This disallows any elif's after this branch.

			if (m_CurrentLine.empty()) {
				SetState(state == St::Default ? St::ElseNewLine : St::Val_ElseNewLine);
			} else {
				m_CurrentLine = statement;
				SetState(state == St::Default ? St::ElseSameLine : St::Val_ElseSameLine);
				HandleConditionalBody(keyword, state == St::Val_SubParser || !m_bSelectionBlockExecuted);
			}
			break;
		}
		default: ARCALC_UNREACHABLE_CODE();
		}
	}

	void Parser::HandleConditionalBody(KeywordType selKW, bool bExecute) {
		auto const state{GetState()}; // This cache is necessary!!!
		ARCALC_NOT_POSSIBLE(!IsSelSt(state));

		if (m_CurrentLine.starts_with(Keyword::ToString(KeywordType::Unscope))) {
			SetState([&] {
				switch (state) {
				case St::Val_IfNewLine:
					m_bConditionRegister.Reset();
					[[fallthrough]];
				case St::Val_ElifNewLine:
					return St::Val_UnscopeIfElifNewLine;
				case St::Val_ElseNewLine:
					return St::Val_UnscopeElseNewLine;
				case St::Val_IfSameLine:
					m_bConditionRegister.Reset();
					[[fallthrough]];
				case St::Val_ElifSameLine:
					return St::Val_UnscopeIfElifSameLine;
				case St::Val_ElseSameLine: 
					return St::Val_UnscopeElseSameLine;
				default:
					ARCALC_UNREACHABLE_CODE();
				}
			}(/*)(*/));

			return;
		}
		
		// indicates that the current statement is a return statement.
		auto const bReturn{m_CurrentLine.starts_with(Keyword::ToString(KeywordType::Return))};
		m_bAllOtherBranchesReturned = {
			bReturn && (selKW == KeywordType::If || m_bAllOtherBranchesReturned)
		};

		if (bExecute) {
			m_bSelectionBlockExecuted = true;
			HandleFirstToken();
		}

		m_bFuncMustExist = IsSelSt(state) && m_bAllOtherBranchesReturned && selKW == KeywordType::Else;
		SetState(IsValSt(state) ? St::Val_SubParser : St::Default);
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

	void Parser::HandleUnscopeKeyword() {
		auto const tokens{Str::SplitOnSpaces<std::string_view>(m_CurrentLine)};
		KeywordDebugDoubleCheck(tokens.front(), KeywordType::Unscope);

		switch (GetState()) { 
		case St::Default: {
			switch (tokens.size()) { // This is silly xd.
			case 1:
				// Do nothing.
				break;
			case 2: {
				auto const& name{tokens[1]};
				if (m_LitMan.IsVisible(name)) {
					m_LitMan.Delete(name);
					Print("Deleted literal: [{}].\n", name);
				} else if (m_FunMan.IsDefined(name)) {
					m_FunMan.Delete(name);
					Print("Deleted function: [{}].\n", name);
				} else throw SyntaxError{
					"Tried to delete invalid name [{}]",
					name,
				};

				break;
			}
			case 3: {
				auto const& oldName{tokens[1]};
				auto const& newName{tokens[2]};
				if (m_LitMan.IsVisible(oldName)) { // Better error messages.
					throw SyntaxError{
						"Tried to rename literal [{}], only functions may be renamed",
						oldName,
					};
				} else if (m_FunMan.IsDefined(oldName)) {
					m_FunMan.Rename(oldName, newName);
					Print("Function [{}] is now [{}].\n", oldName, newName);
				} else throw SyntaxError{ // Better error messages.
					"Tried to rename invalid name [{}]",
					oldName,
				};

				break;
			}
			default:
				throw SyntaxError{
					"Too many tokens passed to keyword [{}]",
					Keyword::ToString(KeywordType::Unscope)
				};
			}

			break;
		}
		case St::Val_SubParser: {
			if (tokens.size() < 2) {
				SetState(St::Val_UnscopeFunc);
			} else {
				auto const& name{tokens[1]};

				// Better error messages.
				if (m_LitMan.IsVisible(name)) { 
					throw SyntaxError{
						"Tried to delete literal [{}] in the scope of function [{}]",
						name, m_FunMan.CurrFunctionName(),
					};
				} else if (m_FunMan.IsDefined(name)) {
					// Deleting a function inside another function is just out of the question.
					throw SyntaxError{
						"Tried to delete function [{}] in the scope of function [{}]",
						name, m_FunMan.CurrFunctionName(),
					};
				} else throw SyntaxError{
					"Tried to delete invalid name [{}] in the scope of function [{}]",
					name, m_FunMan.CurrFunctionName(),
				};
			}
			break;
		}
		default:
			// I chose to handle conditionals in HandleConditionalBody instead.
			ARCALC_UNREACHABLE_CODE();
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
				m_LitMan.SetLast(res);
				Print("{}\n", res);
			} else {
				Print("None.\n");
			}
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
		m_bConditionRegister.Reset();
		m_ReturnValueRegister.reset();
		m_pValSubParser.reset();
	}
}