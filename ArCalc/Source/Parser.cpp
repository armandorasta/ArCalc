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
	enum class Parser::St : size_t {
		// Rest.
		Default = 0U,
		// Function validation.
		Val_LineCollection  = Secret::StValBit | 1,
		Val_SubParser       = Secret::StValBit | 2,
		Val_UnscopeFunc     = Secret::StValBit | 3,
		Val_UnscopeLastLine = Secret::StValBit | 4,
		Val_IfSameLine      = Secret::StValBit | Secret::StSelBit | 1,
		Val_ElifSameLine    = Secret::StValBit | Secret::StSelBit | 2,
		Val_ElseSameLine    = Secret::StValBit | Secret::StSelBit | 3,

		// Selection statements.
		IfSameLine     = Secret::StExecBit | Secret::StSelBit | 1,
		ElifSameLine   = Secret::StExecBit | Secret::StSelBit | 2,
		ElseSameLine   = Secret::StExecBit | Secret::StSelBit | 3,

		// Formatting, (not really used tho).
		FoundLeftCurly  = Secret::FormatBit | 1,
		FoundRightCurly = Secret::FormatBit | 2,
	};

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
		IncrementLineNumber();
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
		case St::Default:
		case St::Val_SubParser:
			if (m_CurrentLine.empty()) {
				break;
			}

			HandleFirstToken();
			break;
		case St::Val_LineCollection:
			try { 
				m_pValSubParser->ParseLine(line); 
				switch (m_pValSubParser->GetState()) {
				case St::Val_UnscopeFunc:
					SetState(St::Default);
					m_pValSubParser.reset();
					m_FunMan.ResetCurrFunc();
					break;
				case St::Val_UnscopeLastLine:
					m_FunMan.RemoveLastLineIfExists();
					m_FunMan.RedoEval(*m_pValSubParser);
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
		default:
			ARCALC_UNREACHABLE_CODE();
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

	#ifdef NDEBUG
	std::optional<double> Parser::GetReturnValue(FuncReturnType) {
		// retype is not checked in Release.
		return std::exchange(m_ReturnValueRegister, {});
	}
#else // ^^^^ Release, vvvv Debug.
	std::optional<double> Parser::GetReturnValue(FuncReturnType retype) {
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
		return std::exchange(m_ReturnValueRegister, {});
	}
#endif // ^^^^ Debug mode.

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
			m_bConditionRegister.Reset();
			// Assume it's a normal expression and show its result in the next line.
			HandleNormalExpression();
		} else {
			using KT = KeywordType;
			if (!IsSelSt(GetState())   // Not a statement inside a conditional? 
				&& !(keyword == KT::Elif || keyword == KT::Else)) // Not another branch? 
			{
				// Reset the condition register, so no normal statement sneaks in in-between.
				m_bConditionRegister.Reset();
			}

			switch (*keyword) {
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
			case KT::Err:     HandleErrKeyword(); break;
			default:         ARCALC_UNREACHABLE_CODE();
			}
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

		if (m_FunMan.IsDefined(litName)) { // Literals override constants and operators.
			throw SyntaxError{
				"Can not define a literal with the name {}, "
				"because a function with that name already exists.",
				litName,
			};
		} 

		auto const value = [&] {
			if (auto const opt{Eval(m_CurrentLine)}; opt.has_value()) {
				return *opt;
			} else throw ParseError{
				"Setting literal [{}] to an expression returns none.\n",
				litName
			};
		}(/*)(*/);

		if (m_LitMan.IsVisible(litName)) { 
			*m_LitMan.Get(litName) = value; 
		} else { 
			m_LitMan.Add(litName, value); 
		}

		if (state == St::Default || IsSelSt(state)) {
			Print("{} = {}\n", litName, value);
			if (MathConstant::IsValid(litName)) {
				Print("Shadowing constant [{} ({})]\n", litName, MathConstant::ValueOf(litName));
			} else if (MathOperator::IsValid(litName)) {
				Print("Shadowing {} operator [{}]", [&] {
					if (MathOperator::IsUnary(litName)) {
						return "unary";
					} else if (MathOperator::IsBinary(litName)) {
						return "binary";
					} else if (MathOperator::IsVariadic(litName)) {
						return "variadic";
					} else {
						ARCALC_UNREACHABLE_CODE();
					}
				}(/*)(*/), litName);
			}
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
		if (m_bInFunction) {
			throw SyntaxError{
				"Found keyword [{}] in an invalid context (inside a function)", 
				KeywordType::Func,
			};
		}

		auto const tokens{Str::SplitOnSpaces<std::string_view>(m_CurrentLine)};
		KeywordDebugDoubleCheck(tokens.front(), KeywordType::Func);
		
		if (tokens.size() == 1) {
			throw SyntaxError{
				"Expected Function name after {0} keyword, but found nothing.\n"
				"{0} [function name] [parameter list]",
				KeywordType::Func
			};
		} 

		// Function name
		auto& funcName = tokens[1];
		ExpectIdentifier(funcName);


		if (m_LitMan.IsVisible(funcName)) {
			// Functions shadow constants and operators.
			throw SyntaxError{
				"Can not define a function with the name {}, "
				"because a literal with that name already exists.",
				funcName,
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
				} else try {
					auto const currParamName{std::string_view{paramName}.substr(1)};
					ExpectIdentifier(currParamName);
					m_FunMan.AddRefParam(currParamName);
				} catch (ArCalcException const&) {
					m_FunMan.ResetCurrFunc();
					throw;
				}
			} else if (paramName.ends_with("...")) {
				ARCALC_NOT_IMPLEMENTED("Parameter packs");
				// ARCALC_DA(i == tokens.size() - 1, "Found '...' in the middle of the parameter list");
				// ARCALC_DA(paramName.size() > 3, "Found '...' but no variable name");
				// 
				// auto const currParamName{std::string_view{paramName}.substr(0, paramName.size() - 3)};
				// AssertIdentifier(currParamName);
				// m_FunMan.AddVariadicParam(currParamName);
				// break; // Not needed because of the asserts above
			} else {
				try { 
					ExpectIdentifier(paramName); 
					m_FunMan.AddParam(paramName);
				} catch (ArCalcException const&) {
					m_FunMan.ResetCurrFunc();
					throw;
				}
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

		ARCALC_NOT_POSSIBLE(!(state == St::Default || state == St::Val_SubParser || IsSelSt(state)));
		if (IsValSt(state)) {
			if (!currLine.empty()) {
				Eval(currLine); // Just make sure no exception is thrown.
			}
		} else {
			if (!IsExecutingFunction()) {
				throw SyntaxError{"Found keyword {} in invalid context (global scope)", KeywordType::Return};
			}

			m_ReturnValueRegister = currLine.empty() 
				? decltype(m_ReturnValueRegister){} 
				: Eval(currLine); 
		}

		/*
		switch (state) {
		case St::Default: 
		case St::IfSameLine: 
		case St::ElifSameLine: 
		case St::ElseSameLine:
			if (!IsExecutingFunction()) {
				throw SyntaxError{"Found keyword {} in invalid context (global scope)", KeywordType::Return};
			}

			m_ReturnValueRegister = currLine.empty() 
				? decltype(m_ReturnValueRegister){} 
				: Eval(currLine); 

			break;
		case St::Val_SubParser: 
		case St::Val_IfSameLine:
		case St::Val_ElifSameLine:
		case St::Val_ElseSameLine: 
			if (!currLine.empty()) {
				Eval(currLine); // Just make sure no exception is thrown.
			}
			
			break;
		default: ARCALC_UNREACHABLE_CODE();
		}
		*/
	}

	void Parser::AddFunctionLine() {
		ARCALC_NOT_POSSIBLE(GetState() != St::Val_LineCollection);

		m_FunMan.AddCodeLine(std::string{m_CurrentLine} + (IsLineEndsWithSemiColon() ? ";" : ""));

		if (auto& subParser{*m_pValSubParser}; subParser.m_bFuncMustExist) {
			// ARCALC_NOT_POSSIBLE(!subParser.m_ReturnTypeRegister);
			m_FunMan.SetReturnType(subParser.m_ReturnTypeRegister.value_or(FuncReturnType::None));
			subParser.m_ReturnTypeRegister.reset();

			// The following warnings will not be output when the return ends with a ; xd
			if (auto const funcName{m_FunMan.CurrFunctionName()}; MathConstant::IsValid(funcName)) {
				Print("This function shadows constant [{} ({})].\n", 
					funcName, MathConstant::ValueOf(funcName));
			} else if (MathOperator::IsValid(funcName)) {
				Print("This function shadows operator [{}].\n", funcName);
			}

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
		auto const keyword{
			*Keyword::FromString(Str::ChopFirstToken<std::string_view>(m_CurrentLine))
		};
		if (IsSelSt(state)) {
			throw SyntaxError{
				"Found selection keyword [{}] in invalid context (inside another selection statement).\n"
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
		case KeywordType::If: 
		{
			ARCALC_NOT_POSSIBLE(!(state == St::Default || state == St::Val_SubParser));

			auto const [cond, statement] {ParseConditionalHeader(keyword, m_CurrentLine)};
			if (cond.empty()) {
				throw ParseError{
					"Expected a condition after keyword [{}], but found nothing", 
					keyword
				};
			}

			if (auto const opt{Eval(cond)}; !opt) {
				throw SyntaxError{"Found expression returns none in condition"};
			} else {
				m_bConditionRegister.Set(std::abs(*opt) > 0.000001);
			}

			if (statement.empty()) {
				throw ParseError{"Expected a statement after the `:`, but found nothing"};
			} 

			m_CurrentLine = statement;
			auto const newState = [=] {
				switch (keyword) {
				case KeywordType::If:   return St::IfSameLine;
				case KeywordType::Elif: return St::ElifSameLine;
				default:                ARCALC_UNREACHABLE_CODE();
				}
			}(/*)(*/);

			if (state == St::Default) {
				SetState(newState);
				HandleConditionalBody(keyword, m_bConditionRegister.Get()); 
			} else {
				SetState(newState & ~Secret::StExecBit | Secret::StValBit);
				HandleConditionalBody(keyword, true);
			}

			break;
		}
		case KeywordType::Else: 
		{
			ARCALC_NOT_POSSIBLE(!(state == St::Default || state == St::Val_SubParser));
			auto const [cond, statement] {ParseConditionalHeader(keyword, m_CurrentLine)};
			
			if (!m_bConditionRegister.IsAvail()) {
				throw SyntaxError{"Found a hanging [{}] keyword", keyword};
			}
			m_bConditionRegister.Reset(); // This disallows any elif's after this branch.

			if (statement.empty()) {
				throw ParseError{"Expected a statement after the `:`, but found nothing"};
			}

			m_CurrentLine = statement;
			if (state == St::Default) {
				SetState(St::ElseSameLine);
				HandleConditionalBody(keyword, !m_bSelectionBlockExecuted);
			} else {
				SetState(St::Val_ElseSameLine);
				HandleConditionalBody(keyword, true);
			}

			break;
		}
		default: ARCALC_UNREACHABLE_CODE();
		}
	}

	void Parser::HandleConditionalBody(KeywordType selKW, bool bExecute) {
		auto const state{GetState()}; // This cache is necessary!!!
		ARCALC_NOT_POSSIBLE(!IsSelSt(state));

		// unscopeStr is an optimiation prevents an unnecessary allocation.
		if (auto const unscopeStr{Keyword::ToStringView(KeywordType::Unscope)}; 
			m_CurrentLine.starts_with(unscopeStr)) 
		{
			throw SyntaxError{
				"Found {} keyword in invalid context (in a conditional statement)",
				unscopeStr,
			};
		}
		
		// indicates that the current statement is a return statement.
		auto const bReturn{m_CurrentLine.starts_with(Keyword::ToStringView(KeywordType::Return))};
		auto const bErr{m_CurrentLine.starts_with(Keyword::ToStringView(KeywordType::Err))};
		m_bAllOtherBranchesReturned = {
			(bReturn || bErr) && (selKW == KeywordType::If || m_bAllOtherBranchesReturned)
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
			default:
				throw ParseError{
					"File deserialization failed; expected either C or F at the begining of the line"
				};
		}
	}

	void Parser::HandleUnscopeKeyword() {
		auto const tokens{Str::SplitOnSpaces<std::string_view>(m_CurrentLine)};
		KeywordDebugDoubleCheck(tokens.front(), KeywordType::Unscope);

		auto const printUnshadowString = [this](auto name) {
			if (MathConstant::IsValid(name)) {
				Print("Revealing constant [{} ({})] once again.\n",
					name, MathConstant::ValueOf(name));
			} else if (MathOperator::IsValid(name)) {
				Print("Revealing operator [{}] once again.\n", name);
			}
		};

		switch (GetState()) { 
		case St::Default:
			switch (tokens.size()) { // This is silly xd.
			case 1:
				// Do nothing.
				break;
			case 2:
			{
				auto const& name{tokens[1]};
				ExpectIdentifier(name);

				if (m_LitMan.IsVisible(name)) {
					m_LitMan.Delete(name);
					Print("Deleted literal: [{}].\n", name); 
					printUnshadowString(name);
				} else if (m_FunMan.IsDefined(name)) {
					m_FunMan.Delete(name);
					Print("Deleted function: [{}]{}.\n", name); 
					printUnshadowString(name);
				} else if (MathConstant::IsValid(name)) {
					throw SyntaxError{"Tried to delete constant [{}]", name};
				} else if (MathOperator::IsValid(name)) {
					throw SyntaxError{"Tried to delete operator [{}]", name};
				} else {
					ARCALC_UNREACHABLE_CODE();
				}

				break;
			}
			case 3:
			{
				auto const& oldName{tokens[1]};
				ExpectIdentifier(oldName);

				auto const& newName{tokens[2]};
				ExpectIdentifier(newName);

				if (m_FunMan.IsDefined(oldName)) {
					m_FunMan.Rename(oldName, newName);
					Print("Function [{}] is now [{}].\n", oldName, newName);
					printUnshadowString(oldName);
					if (MathConstant::IsValid(newName)) {
						Print("But constant [{} ({})] is out.\n",
							newName, MathConstant::ValueOf(newName));
					} else if (MathOperator::IsValid(newName)) {
						Print("But operator [{}] is out.\n", newName);
					}
				} else { // Better error messages.
					throw SyntaxError{
						"Tried to rename {} [{}], only functions may be renamed",
						[&] {
							if (m_LitMan.IsVisible(oldName)) {
								return "literal"; 
							} else if (MathConstant::IsValid(oldName)) {
								return "constant";
							} else if (MathOperator::IsValid(oldName)) {
								return "operator";
							} else {
								ARCALC_UNREACHABLE_CODE();
							}
						}(/*)(*/),
						oldName,
					};
				}

				break;
			}
			default:
				throw SyntaxError{
					"Too many tokens passed to keyword [{}]", 
					Keyword::ToStringView(KeywordType::Unscope)
				};
			}

			break;
		case St::Val_SubParser:
			switch (tokens.size()) {
			case 0:
				ARCALC_UNREACHABLE_CODE();
				break; // ^^^^ If (true) throw ...
			case 1:
				SetState(St::Val_UnscopeLastLine);
				break;
			case 2:
			{
				auto const& name{tokens[1]};

				if (auto const opt{Keyword::FromString(name)}; opt) {
					if (opt != KeywordType::Func) {
						throw SyntaxError{
							"Found keyword {} in invalid context (after the {} keyword)",
							*opt, KeywordType::Unscope,
						};
					}

					SetState(St::Val_UnscopeFunc);
				} else {
					ExpectIdentifier(name);
					throw SyntaxError{
						"Tried to delete {} [{}] in the scope of another function",
						[&] {
							if (m_LitMan.IsVisible(name)) {
								return "literal";
							} else if (m_FunMan.IsDefined(name)) {
								// Deleting a function inside another function is just 
								// out of the question.
								return "function";
							} else if (MathConstant::IsValid(name)) {
								return "constant";
							} else if (MathOperator::IsValid(name)) {
								return "operator";
							} else {
								ARCALC_UNREACHABLE_CODE();
							}
						}(/*)(*/),
						name,
					};
				}
				break;
			}
			default:
				throw SyntaxError{
					"Too many tokens passed to keyword [{}]", 
					Keyword::ToStringView(KeywordType::Unscope)
				};
			}

			break;
		default:
			ARCALC_UNREACHABLE_CODE();
		}
	}

	void Parser::HandleErrKeyword() {
		if (!IsExecutingFunction()) {
			throw SyntaxError{
				"Found keyword {} in invalid context (in global scope)",
				KeywordType::Err,
			};
		}

		auto const state{GetState()};
		m_bFuncMustExist = (state == St::Val_SubParser);
		KeywordDebugDoubleCheck(Str::ChopFirstToken<std::string_view>(m_CurrentLine),
			KeywordType::Err);

		auto const nNextChar = [this] {
			for (size_t res{}; res < m_CurrentLine.size(); ++res) {
				if (auto const ch{m_CurrentLine[res]}; !std::isspace(ch)) {
					if (ch != '\'') {
						throw SyntaxError{"Expected a single quote, but found [{}]", ch};
					} else {
						return res;
					}
				}
			}

			throw SyntaxError{"Expected a single quote, but found nothing"};
		}(/*)(*/);

		if (nNextChar == m_CurrentLine.size() - 1) {
			throw SyntaxError{"Expected an error message"};
		}

		if (auto const qi{m_CurrentLine.find('\'', nNextChar + 1)}; qi == std::string::npos) {
			throw SyntaxError{"Expected a single quote terminating the error message"};
		} else if (qi != m_CurrentLine.size() - 1) {
			throw SyntaxError{"Found a single quote in the middle of the error message"};
		} else if (!IsValSt(state)) {
			m_CurrentLine.remove_prefix(nNextChar + 1);
			m_CurrentLine.remove_suffix(1);
			if (m_CurrentLine.empty()) {
				throw SyntaxError{"Found empty error message"};
			}

			auto error = UserError{m_CurrentLine};
			error.SetLineNumber(GetLineNumber());
			throw error;
		}
	}

	std::string Parser::FormatErrorMessage(std::string_view message) {
		St st{St::Default};
		std::string formattedMessage{};
		formattedMessage.reserve(message.size()); // Lower bound.
		std::string exprAcc{};

		for (auto const c : message) {
			switch (st) {
			case St::Default:
				switch (c) {
				case '{':
					st = St::FoundLeftCurly;
					break;
				case '}':
					st = St::FoundRightCurly;
					break;
				default:
					formattedMessage.push_back(c);
					break;
				}

				break;
			case St::FoundLeftCurly:
				switch (c) {
				case '{':
					formattedMessage.push_back(c);
					st = St::Default;
					break;
				case '}':
					if (exprAcc.empty()) {
						throw SyntaxError{"Found an empty {{}} inside formatted message"};
					} 

					if (auto const res{Eval(exprAcc)}; !res) {
						throw SyntaxError{"Found an expression returns none inside {{}}"};
					} else {
						// TODO: make this more flexible
						formattedMessage.append(std::format("{:.2f}", *res));
						exprAcc.clear();
						st = St::Default;
					}

					break;
				default:
					exprAcc.push_back(c);
					break;
				}

				break;
			case St::FoundRightCurly: // Found a `}` before a `{`
				if (c == '}') {
					formattedMessage.push_back(c);
					st = St::Default;
				} else {
					throw SyntaxError{"Found `}` with no matching `{`"};
				}

				break;
			default:
				ARCALC_UNREACHABLE_CODE();
			}
		}

		if (st == St::FoundLeftCurly) {
			throw SyntaxError{"Expected a `}` terminating formatted message"};
		}

		return formattedMessage;
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
	void Parser::KeywordDebugDoubleCheck(std::string_view, KeywordType) {
	}
#else // ^^^^ Release, vvvv Debug.
	void Parser::KeywordDebugDoubleCheck(std::string_view glyph, KeywordType what) {
		return ExpectKeyword(glyph, what);
	}
#endif

	void Parser::ExpectIdentifier(std::string_view what) {
		if (std::isdigit(what[0])) {
			throw ParseError{
				"Invalid identifier ({}); found digit [{}]", what, what[0]
			};
		} else if (Keyword::IsValid(what)) {
			throw ParseError{"Expected identifier, but found keyword ({})", what};
		} 

		for (auto const c : what) {
			if (!Str::IsAlNum(c) && c != '_') {
				throw ParseError{"Found invalid character [{}] in indentifier [{}]", c, what};
			}
		}
	}

	bool Parser::IsValidIdentifier(std::string_view what) {
		return !std::isdigit(what[0]) 
			&& !Keyword::IsValid(what) 
			&& std::ranges::all_of(what, 
				[](auto const c) { return std::isalnum(c) || c == '_'; });
	}

	void Parser::SubReset() {
		ARCALC_DA(m_bInFunction, "Parser::SubReset called on non-sub-parser");
		m_CurrState   = St::Val_SubParser;
		m_CurrentLine = {};
		m_LineNumber  = {};
		m_bConditionRegister.Reset();
		m_ReturnValueRegister.reset();
		m_ReturnTypeRegister.reset();

		m_bSelectionBlockExecuted = {};
		m_bSemiColon              = {};
		m_bJustHitReturn          = {};
		m_bFuncMustExist          = {};
	}
}