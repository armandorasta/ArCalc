#include "Function.h"
#include "Util.h"
#include "Exception/ArCalcException.h"

namespace ArCalc {
	std::ostream& operator<<(std::ostream& os, FuncReturnType retype) {
		switch (retype) {
			using enum FuncReturnType;
		case Number:	return os << "Number";
		case None:		return os << "None";
		default:		return os << "Invalid FuncReturnType";
		}
	}

	bool Function::IsDefined(std::string_view name) {
		return s_FuncMap.contains(std::string{name});
	}

	void Function::BeginDefination(std::string_view funcName, size_t lineNumber) {
		ARCALC_ASSERT(!IsDefinationInProgress(), "{} inside another function", Util::FuncName());
		ARCALC_ASSERT(!IsDefined(funcName), "Multiple definations of function [{}]", funcName);
		s_CurrFuncName = funcName;
		s_CurrFuncData.HeaderLineNumber = lineNumber;
	}

	void Function::MakeVariadic() {
		ARCALC_ASSERT(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		//ARCALC_ASSERT(!s_CurrFuncData.Params.empty(), "Variadic functions may at least have one parameter");
		s_CurrFuncData.IsVariadic = true;
	}

	void Function::AddParam(std::string_view name) {
		AddParamImpl(name, false);
	}

	void Function::AddVariadicParam(std::string_view name) {
		AddParamImpl(name, true);
	}

	void Function::AddCodeLine(std::string_view codeLine) {
		ARCALC_ASSERT(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		ARCALC_ASSERT(!s_CurrFuncData.Params.empty(), "Adding a function takes no arguments");
		s_CurrFuncData.CodeLines.push_back(std::string{codeLine});
	}

	void Function::SetReturnType(FuncReturnType retype) {
		ARCALC_ASSERT(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		s_CurrFuncData.ReturnType = retype;
	}

	FuncReturnType Function::GetCurrentReturnType() {
		ARCALC_ASSERT(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		return s_CurrFuncData.ReturnType;
	}

	bool Function::IsCurrFuncVariadic() {
		ARCALC_ASSERT(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		return s_CurrFuncData.IsVariadic;
	}

	std::vector<ParamData> const& Function::CurrParamData() {
		return s_CurrFuncData.Params;
	}

	void Function::EndDefination() {
		ARCALC_ASSERT(!s_CurrFuncData.CodeLines.empty(), "Adding an empty function");
		s_FuncMap.emplace(std::exchange(s_CurrFuncName, ""), std::exchange(s_CurrFuncData, {}));
	}

	size_t Function::ParamCountOf(std::string_view name) {
		return SafeGet(name).Params.size();
	}

	bool Function::IsVariadic(std::string_view name) {
		return SafeGet(name).IsVariadic;
	}

	FuncReturnType Function::ReturnTypeOf(std::string_view name) {
		return SafeGet(name).ReturnType;
	}

	size_t Function::HeaderLineNumberOf(std::string_view name) {
		return SafeGet(name).HeaderLineNumber;
	}

	std::vector<ParamData> const& Function::ParamDataOf(std::string_view name) {
		return SafeGet(name).Params;
	}

	std::vector<std::string> const& Function::CodeLinesOf(std::string_view name) {
		return SafeGet(name).CodeLines;
	}

	void Function::ResetCurrFunc() {
		s_CurrFuncName = {};
		s_CurrFuncData = {};
	}

	void Function::Reset() { 
		ResetCurrFunc();
		s_FuncMap.clear(); 
	}

	void Function::AddParamImpl(std::string_view paramName, bool bParameterPack) {
		ARCALC_ASSERT(IsDefinationInProgress(), "{} outside defination", Util::FuncName());
		ARCALC_ASSERT(!s_CurrFuncData.IsVariadic, "Adding parameter after making function variadic");
		ARCALC_ASSERT(s_CurrFuncData.CodeLines.empty(), "Tried to add a parameter after adding a code line");

		// No doublicates please
		std::string const paramNameOwned{paramName};
		auto const it{range::find(s_CurrFuncData.Params, paramNameOwned, &ParamData::Name)};
		ARCALC_ASSERT(it == s_CurrFuncData.Params.end(),
			"Error: Adding function with doublicate parameter name [{}]", paramName);

		s_CurrFuncData.Params.push_back({
			.Name{paramNameOwned},
			.IsParameterPack{bParameterPack}, 
			.Values{}
		});
		s_CurrFuncData.IsVariadic = bParameterPack;
	}

	FuncData const& Function::Get(std::string_view name) {
		return s_FuncMap.at(std::string{name});
	}

	FuncData const& Function::SafeGet(std::string_view funcName, std::source_location debugInfo) {
		ARCALC_ASSERT(IsDefined(funcName), "{} on invalid function [{}]", 
			Util::FuncName(debugInfo), funcName);
		return Get(funcName);
	}
}