#pragma once

#include "Core.h"


namespace ArCalc {
	enum class FuncReturnType : size_t {
		None = 0,
		Number,
	};

	std::ostream& operator<<(std::ostream& os, FuncReturnType retype);

	struct ParamData {
		std::string Name;
		bool IsParameterPack;
		std::vector<double> Values;
	};

	struct FuncData {
		std::vector<ParamData> Params;
		std::vector<std::string> CodeLines;
		bool IsVariadic;
		FuncReturnType ReturnType;
		size_t HeaderLineNumber;
	};

	class Function {
	private:
		inline static std::unordered_map<std::string, FuncData> s_FuncMap{};

	public:
		static void BeginDefination(std::string_view funcName, size_t lineNumber);
		static constexpr bool IsDefinationInProgress() { return !s_CurrFuncName.empty(); }
		static void AddParam(std::string_view paramName);
		static void AddVariadicParam(std::string_view paramName);
		static void AddCodeLine(std::string_view codeLine);
		static void SetReturnType(FuncReturnType retype);
		static FuncReturnType GetCurrentReturnType();
		static bool IsCurrFuncVariadic();
		static std::vector<ParamData> const& CurrParamData();
		static void EndDefination();

		static bool IsDefined(std::string_view name);
		static size_t ParamCountOf(std::string_view name);
		static bool IsVariadic(std::string_view name);
		static FuncReturnType ReturnTypeOf(std::string_view name);
		static size_t HeaderLineNumberOf(std::string_view name);
		static std::vector<ParamData> const& ParamDataOf(std::string_view name);
		static std::vector<std::string> const& CodeLinesOf(std::string_view name);

		static void ResetCurrFunc();
		static void Reset();

	private:
		static void AddParamImpl(std::string_view paramName, bool bParameterPack = false);
		static void MakeVariadic();
		static FuncData const& Get(std::string_view name);
		static FuncData const& SafeGet(std::string_view funcName, std::source_location debugInfo ={});

	private:
		inline static std::string s_CurrFuncName{};
		inline static FuncData s_CurrFuncData{};
	};
}