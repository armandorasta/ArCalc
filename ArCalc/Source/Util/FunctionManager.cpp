#include "FunctionManager.h"
#include "Util.h"
#include "IO.h"
#include "Exception/ArCalcException.h"
#include "../Parser.h"

namespace ArCalc {
	std::ostream& operator<<(std::ostream& os, FuncReturnType retype) {
		switch (retype) {
		using enum FuncReturnType;
		case Number: return os << "Number";
		case None:   return os << "None";
		default:     return os << "Invalid FuncReturnType";
		}
	}

	ParamData::ParamData(std::string_view paramName) : m_Name{std::string{paramName}} {
	}

	ParamData ParamData::MakeByValue(std::string_view paramName, bool bParameterPack) {
		ParamData data{paramName};
		data.bReference = false;

		if (bParameterPack) {
			ARCALC_NOT_IMPLEMENTED();
			// data.m_ByValue.IsParameterPack = bParameterPack;
		}

		return data;
	}

	ParamData ParamData::MakeByRef(std::string_view paramName) {
		ParamData data{paramName};
		data.bReference = true;
		return data;
	}

	void ParamData::SetRef(double* ptr) {
		ARCALC_DA(IsPassedByRef(), "SetRef on by-value parameter");
		m_ByRef.Ptr = ptr;
	}

	void ParamData::PushValue(double newValue) {
		ARCALC_DA(!IsPassedByRef(), "Push on by-reference parameter");
		
		if (/*!m_ByValue.IsParameterPack &&*/ !m_ByValue.Values.empty()) {
			ARCALC_NOT_IMPLEMENTED();
			// ARCALC_DE("Pushing more than one value on a normal by-value parameter");
		}
		
		m_ByValue.Values.push_back(newValue);
		m_ByRef.Ptr = {};
	}

	void ParamData::ClearValues() {
		ARCALC_DA(!IsPassedByRef(), "ClearValues on by-reference parameter");
		m_ByValue.Values.clear();
	}

	double ParamData::GetValue(size_t index) const {
		ARCALC_DA(!IsPassedByRef(), "GetValue on by-reference parameter");
		ARCALC_DA(index < m_ByValue.Values.size(), "Parameter pack index out of bounds");
		return m_ByValue.Values[index];
	}

	size_t ParamData::ValueCount() {
		ARCALC_DA(!IsPassedByRef(), "ValueCount on by-reference parameter");
		return m_ByValue.Values.size();
	}

	FunctionManager::FunctionManager(std::ostream& os) : m_OStream{os} {
	}


	bool FunctionManager::IsDefined(std::string_view name) const {
		return m_FuncMap.contains(std::string{name});
	}

	void FunctionManager::BeginDefination(std::string_view funcName, size_t lineNumber) {
		ARCALC_DA(!IsDefinationInProgress(), "{} inside another function", Util::FuncName());
		ARCALC_EXPECT(!IsDefined(funcName), "Multiple definations of function [{}]", funcName);
		m_CurrFuncName = funcName;
		m_CurrFuncData.HeaderLineNumber = lineNumber;
	}

	void FunctionManager::MakeVariadic() {
		ARCALC_DA(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		//ARCALC_DA(!m_CurrFuncData.Params.empty(), "Variadic functions may at least have one parameter");
		m_CurrFuncData.IsVariadic = true;
	}

	void FunctionManager::AddParam(std::string_view name) {
		AddParamImpl(name, false);
	}

	void FunctionManager::AddRefParam(std::string_view paramName) {
		AddParamImpl(paramName, false, true);
	}

	void FunctionManager::AddVariadicParam(std::string_view name) {
		AddParamImpl(name, true);
	}

	void FunctionManager::AddCodeLine(std::string_view codeLine) {
		ARCALC_DA(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		ARCALC_DA(!m_CurrFuncData.Params.empty(), "Adding a function takes no arguments");
		m_CurrFuncData.CodeLines.push_back(std::string{codeLine});
	}

	void FunctionManager::SetReturnType(FuncReturnType retype) {
		ARCALC_DA(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		m_CurrFuncData.ReturnType = retype;
	}

	FuncReturnType FunctionManager::GetCurrentReturnType() const {
		ARCALC_DA(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		return m_CurrFuncData.ReturnType;
	}

	bool FunctionManager::IsCurrFuncVariadic() const {
		ARCALC_DA(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		return m_CurrFuncData.IsVariadic;
	}

	std::vector<ParamData> const& FunctionManager::CurrParamData() const {
		return const_cast<FunctionManager&>(*this).CurrParamData();
	}

	std::vector<ParamData>& FunctionManager::CurrParamData() {
		return m_CurrFuncData.Params;
	}

	void FunctionManager::EndDefination() {
		ARCALC_EXPECT(!m_CurrFuncData.CodeLines.empty(), "Adding an empty function");
		m_FuncMap.emplace(std::exchange(m_CurrFuncName, ""), std::exchange(m_CurrFuncData, {}));
	}

	void FunctionManager::ResetCurrFunc() {
		m_CurrFuncName = {};
		m_CurrFuncData = {};
	}

	size_t FunctionManager::CurrHeaderLineNumber() const {
		return m_CurrFuncData.HeaderLineNumber;
	}

	void FunctionManager::Reset() { 
		ResetCurrFunc();
		m_FuncMap.clear(); 
	}

	void FunctionManager::AddParamImpl(std::string_view paramName, bool bParameterPack, 
		bool bReference) 
	{
		ARCALC_DA(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		ARCALC_DA(!m_CurrFuncData.IsVariadic, "Adding parameter after making function variadic");
		ARCALC_DA(m_CurrFuncData.CodeLines.empty(), 
			"Tried to add a parameter after adding a code line");

		// No doublicates please
		auto const paramNameOwned = std::string{paramName};
		if (auto const it{range::find(m_CurrFuncData.Params, paramNameOwned, &ParamData::GetName)};
			it != m_CurrFuncData.Params.end())
		{
			throw ArCalcException{
				"Error: Adding function with doublicate parameter name [{}]", paramName
			};
		}

		m_CurrFuncData.Params.push_back(bReference 
			? ParamData::MakeByRef(paramName) 
			: ParamData::MakeByValue(paramName, bParameterPack));
		m_CurrFuncData.IsVariadic = bParameterPack;
	}

	FuncData const& FunctionManager::Get(std::string_view funcName) const {
		return const_cast<FunctionManager&>(*this).Get(funcName);
	}

	FuncData& FunctionManager::Get(std::string_view funcName) {
		ARCALC_DA(IsDefined(funcName), "{} on invalid function [{}]", Util::FuncName(), funcName);
		return m_FuncMap.at(std::string{funcName});
	}

	std::optional<double> FunctionManager::CallFunction(std::string_view funcName) {
		ARCALC_DA(IsDefined(funcName), "Call of undefined function [{}]", funcName);

		auto& func{Get(funcName)};
		if (func.IsVariadic) {
			ARCALC_NOT_IMPLEMENTED();

			// auto& parameterPack{func.Params.back().Values};
			// for (auto const arg : args | view::drop(func.Params.size())) { // Drop the named arguments.
			// 	parameterPack.push_back(arg);
			// }
		}

		auto subParser = Parser{m_OStream, func.Params, false};
		for (auto const& codeLine : func.CodeLines) {
			subParser.ParseLine(codeLine);
		}

		return subParser.GetReturnValue(func.ReturnType);
	}

	void FunctionManager::Serialize(std::string_view name, std::ostream& os) {
		// F [name] [param count] ( { ref [0 or 1] [param name]... } ) 
		// [return type: 0 for None, 1 for Number] [line count] { [line of code]... }

		if (!IsDefined(name)) {
			throw ArCalcException{"Serializing undefined function [{}]", name};
		} 

		// Name and parameter count.
		auto const& func{Get(name)};
		std::string res{std::format("F {} {} ( ", name, func.Params.size())};
		auto outIt{std::back_inserter(res)};

		// Paramter list.
		for (auto const& param : func.Params) {
			std::format_to(outIt, "{{ ref {:d} {} }} ", param.IsPassedByRef(), param.GetName());

			if (param.IsParameterPack()) {
				ARCALC_NOT_IMPLEMENTED();
			}
		}

		// Return type and line count.
		std::format_to(outIt, ") {:d} {} {{\n", 
			func.ReturnType == FuncReturnType::Number, func.CodeLines.size());

		// Body.
		for (auto const& line : func.CodeLines) {
			std::format_to(outIt, "\t{}\n", line);
		}
		res += "}\n";

		os << res;
	}

	void FunctionManager::Deserialize(std::istream& is) {
		auto const expectSeq = [&](std::string_view what) {
#ifdef NDEBUG
			is.seekg(what.size(), std::ios::cur);
#else // ^^^^ Release, vvvv Debug
			std::string rubbish{};
			rubbish.resize(what.size());
			is.read(rubbish.data(), rubbish.size());
			ARCALC_DA(rubbish == what, "Deserializing function: expected exactly `{}` but found `{}`", 
				what, rubbish);
#endif
		};

		auto const funcName{IO::Input<std::string>(is)};
		auto func = FuncData{};

		// Parameters.
		auto const paramCount{IO::Input<size_t>(is)};
		expectSeq(" ( ");
		for ([[maybe_unused]] auto const i : view::iota(0U, paramCount)) {
			expectSeq("{ ");
			expectSeq("ref ");

			auto const bByRef{static_cast<bool>(IO::Input<int>(is))};
			if (auto const paramName{IO::Input<std::string>(is)}; bByRef) {
				func.Params.emplace_back(ParamData::MakeByRef(paramName));
			} else {
				func.Params.emplace_back(ParamData::MakeByValue(paramName, false));
			}

			expectSeq(" } ");
		}
		expectSeq(") ");

		// Return type.
		func.ReturnType = IO::Input<size_t>(is) == 1 ? FuncReturnType::Number 
		                                             : FuncReturnType::None;

		// Lines of code.
		auto const lineCount{IO::Input<size_t>(is)};
		expectSeq(" {\n");
		for ([[maybe_unused]] auto const i : view::iota(0U, lineCount)) {
			func.CodeLines.emplace_back(IO::GetLine(is).substr(1)); // Skip the tab at the begining.
		}
		expectSeq("}\n");

		// Functions with the same names will be overriden.
		m_FuncMap.insert_or_assign(funcName, func); 
	}
}