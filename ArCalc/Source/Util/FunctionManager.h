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

	class FunctionManager {
	private:
		using FuncMap = std::unordered_map<std::string, FuncData>;

	public:
		FunctionManager(FunctionManager const&)             = default;
		FunctionManager(FunctionManager&&)                  = default;
		FunctionManager& operator=(FunctionManager const&)  = default;
		FunctionManager& operator=(FunctionManager&&)       = default;

		FunctionManager(std::ostream& os);

		//FunctionManager(class Parser& parent);

		void BeginDefination(std::string_view funcName, size_t lineNumber);
		constexpr bool IsDefinationInProgress() const 
			{ return !m_CurrFuncName.empty(); }
		void AddParam(std::string_view paramName);
		void AddVariadicParam(std::string_view paramName);
		void AddCodeLine(std::string_view codeLine);
		void SetReturnType(FuncReturnType retype);
		void EndDefination();

		bool IsDefined(std::string_view name) const;
		FuncReturnType GetCurrentReturnType() const;
		bool IsCurrFuncVariadic() const;
		std::vector<ParamData> const& CurrParamData() const;
		size_t CurrHeaderLineNumber() const;

		FuncData const& Get(std::string_view funcName) const;
		std::optional<double> CallFunction(std::string_view funcName, std::vector<double> const& args);

		void Reset();
		void ResetCurrFunc();

	private:
		void AddParamImpl(std::string_view paramName, bool bParameterPack = false);
		void MakeVariadic();

	private:
		//class Parser* m_pParent;
		std::string m_CurrFuncName{};
		FuncData m_CurrFuncData{};
		std::unordered_map<std::string, FuncData> m_FuncMap{};
		std::ostream* m_pOStream{};
	};
}