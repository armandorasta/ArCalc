#pragma once

#include "Core.h"

/**** Rules for parameter passing
 * Both numbers and literals can be passed by value.
 * Only literals can be passed by reference.
 * A by-reference parameter is prefixed by an & with no spaces in between.
 * {
 *     _Fun Set &a b
 *         _Set a b
 *         _Return
 * 
 *     myVar 5 Set
 *     _Set myVar 5
 * }
 * Passing by reference will probably lead to weird behaviour in complex expressions,
 * but I am not going to fix them.
*/

namespace ArCalc {
	enum class FuncReturnType : size_t {
		None = 0,
		Number,
	};

	constexpr std::string FuncReturnTypeToString(FuncReturnType retype) {
		constexpr std::array sc_Lookup{"None", "Number"};

		auto const i{static_cast<std::underlying_type_t<FuncReturnType>>(retype)};
		return i < sc_Lookup.size() ? sc_Lookup[i] : "Invalid FuncReturnType";
	}

	std::ostream& operator<<(std::ostream& os, FuncReturnType retype);

	class ParamData {
	private:
		ParamData(std::string_view paramName);

	public:
		static ParamData MakeByValue(std::string_view paramName, bool bParameterPack);
		static ParamData MakeByRef(std::string_view paramName);

		constexpr bool IsPassedByRef() const
			{ return m_bReference; }
		void SetRef(double* toWhat);
		double* GetRef() const
			{ return m_ByRef.Ptr; }

		constexpr bool IsParameterPack() const
			{ return m_ByValue.IsParameterPack; }
		void PushValue(double newValue);
		void ClearValues();
		double GetValue(size_t index = 0U) const;
		size_t ValueCount();

		constexpr std::string const& GetName() const
			{ return m_Name; }

	private:
		std::string m_Name;
		bool m_bReference;

		struct { // Passing by value.
			bool IsParameterPack{};
			std::vector<double> Values{};
		} m_ByValue;

		struct { // Passing by reference.
			double* Ptr{};
		} m_ByRef;
	};

	struct FuncData {
		std::vector<ParamData> Params;
		std::vector<std::string> CodeLines;
		bool IsVariadic;
		FuncReturnType ReturnType;
		size_t HeaderLineNumber;
	};

	std::ostream& operator<<(std::ostream& lhs, FuncData const& rhs);

	class FunctionManager {
	private:
		using FuncMap = std::unordered_map<std::string, FuncData>;

	public:
		FunctionManager(FunctionManager const&)             = default;
		FunctionManager(FunctionManager&&)                  = default;
		FunctionManager& operator=(FunctionManager const&)  = default;
		FunctionManager& operator=(FunctionManager&&)       = default;

		FunctionManager(std::ostream& os);

		void BeginDefination(std::string_view funcName, size_t lineNumber);
		constexpr bool IsDefinationInProgress() const 
			{ return !m_CurrFuncName.empty(); }

		void AddParam(std::string_view paramName);
		void AddRefParam(std::string_view paramName);
		void AddVariadicParam(std::string_view paramName);

		void AddCodeLine(std::string_view codeLine);
		void SetReturnType(FuncReturnType retype);
		void EndDefination();

		bool IsDefined(std::string_view name) const;
		FuncReturnType GetCurrentReturnType() const;
		bool IsCurrFuncVariadic() const;
		std::vector<ParamData> const& CurrParamData() const;
		std::vector<ParamData>& CurrParamData();
		size_t CurrHeaderLineNumber() const;

		FuncData const& Get(std::string_view funcName) const;
		FuncData& Get(std::string_view funcName);
		std::optional<double> CallFunction(std::string_view funcName);

		void Serialize(std::string_view name, std::ostream& os);
		void Deserialize(std::istream& is);

		void Reset();
		void ResetCurrFunc();

	private:
		void AddParamImpl(std::string_view paramName, bool bParameterPack = false, 
			bool m_bReference = false);
		void MakeVariadic();

	private:
		std::string m_CurrFuncName{};
		FuncData m_CurrFuncData{};
		std::unordered_map<std::string, FuncData> m_FuncMap{};

		std::ostream& m_OStream;
	};
}

namespace std {
	template <>
	struct formatter<ArCalc::FuncReturnType> : public formatter<std::string> {
		using Base = formatter<std::string>;
		using Self = ArCalc::FuncReturnType;
		using Base::parse;
		auto format(Self rt, format_context ctx) {
			return Base::format(ArCalc::FuncReturnTypeToString(rt), ctx);
		}
	};
}