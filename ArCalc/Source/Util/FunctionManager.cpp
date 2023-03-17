#include "FunctionManager.h"
#include "Util.h"
#include "Exception/ArCalcException.h"

namespace ArCalc {
	FunctionManager::FunctionManager(Parser& parent) : m_pParent{&parent} {
	}

	std::ostream& operator<<(std::ostream& os, FuncReturnType retype) {
		switch (retype) {
		using enum FuncReturnType;
		case Number:	return os << "Number";
		case None:		return os << "None";
		default:		return os << "Invalid FuncReturnType";
		}
	}

	bool FunctionManager::IsDefined(std::string_view name) const {
		return m_FuncMap.contains(std::string{name});
	}

	void FunctionManager::BeginDefination(std::string_view funcName, size_t lineNumber) {
		ARCALC_ASSERT(!IsDefinationInProgress(), "{} inside another function", Util::FuncName());
		ARCALC_ASSERT(!IsDefined(funcName), "Multiple definations of function [{}]", funcName);
		m_CurrFuncName = funcName;
		m_CurrFuncData.HeaderLineNumber = lineNumber;
	}

	void FunctionManager::MakeVariadic() {
		ARCALC_ASSERT(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		//ARCALC_ASSERT(!m_CurrFuncData.Params.empty(), "Variadic functions may at least have one parameter");
		m_CurrFuncData.IsVariadic = true;
	}

	void FunctionManager::AddParam(std::string_view name) {
		AddParamImpl(name, false);
	}

	void FunctionManager::AddVariadicParam(std::string_view name) {
		AddParamImpl(name, true);
	}

	void FunctionManager::AddCodeLine(std::string_view codeLine) {
		ARCALC_ASSERT(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		ARCALC_ASSERT(!m_CurrFuncData.Params.empty(), "Adding a function takes no arguments");
		m_CurrFuncData.CodeLines.push_back(std::string{codeLine});
	}

	void FunctionManager::SetReturnType(FuncReturnType retype) {
		ARCALC_ASSERT(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		m_CurrFuncData.ReturnType = retype;
	}

	FuncReturnType FunctionManager::GetCurrentReturnType() const {
		ARCALC_ASSERT(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		return m_CurrFuncData.ReturnType;
	}

	bool FunctionManager::IsCurrFuncVariadic() const {
		ARCALC_ASSERT(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		return m_CurrFuncData.IsVariadic;
	}

	std::vector<ParamData> const& FunctionManager::CurrParamData() const {
		return m_CurrFuncData.Params;
	}

	void FunctionManager::EndDefination() {
		ARCALC_ASSERT(!m_CurrFuncData.CodeLines.empty(), "Adding an empty function");
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

	void FunctionManager::AddParamImpl(std::string_view paramName, bool bParameterPack) {
		ARCALC_ASSERT(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		ARCALC_ASSERT(!m_CurrFuncData.IsVariadic, "Adding parameter after making function variadic");
		ARCALC_ASSERT(m_CurrFuncData.CodeLines.empty(), "Tried to add a parameter after adding a code line");

		// No doublicates please
		std::string const paramNameOwned{paramName};
		auto const it{range::find(m_CurrFuncData.Params, paramNameOwned, &ParamData::Name)};
		ARCALC_ASSERT(it == m_CurrFuncData.Params.end(),
			"Error: Adding function with doublicate parameter name [{}]", paramName);

		m_CurrFuncData.Params.push_back({
			.Name{paramNameOwned},
			.IsParameterPack{bParameterPack}, 
			.Values{}
		});
		m_CurrFuncData.IsVariadic = bParameterPack;
	}

	FuncData const& FunctionManager::Get(std::string_view funcName) const {
		ARCALC_ASSERT(IsDefined(funcName), "{} on invalid function [{}]", Util::FuncName(), funcName);
		return m_FuncMap.at(std::string{funcName});
	}
}