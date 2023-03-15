#pragma once

#include "Core.h"

namespace ArCalc::IO {
	namespace Secret {
		template <class Condition>
		concept Not = !Condition::value;
	}

	std::string GetLine(std::istream& is);
	std::string GetLineStd();
	
	constexpr auto FuncName(std::source_location debugInfo = {}) {
		return debugInfo.function_name();
	}

	template<std::default_initializable ValueType>
	ValueType Input(std::istream& is) {
		ValueType value{ };
		is >> value;
		return value;
	}

	template<std::default_initializable ValueType>
	auto InputStd() {
		return Input<ValueType>(std::cin);
	}

	template <class ValueType> requires Secret::Not<std::is_const<ValueType>>
	void Input(std::istream& is, ValueType* pOutValue) {
		is >> *pOutValue;
	}

	template <class ValueType> requires Secret::Not<std::is_const<ValueType>>
	auto InputStd(ValueType* pOutValue) {
		return Input(std::cin, pOutValue);
	}

	template<class ValueType>
	std::ostream& Output(std::ostream& os, ValueType&& what)  
		requires requires { os << std::forward<ValueType>(what); } 
	{
		os << std::forward<ValueType>(what);
		return os;
	}

	template<class ValueType>
	std::ostream& OutputStd(ValueType&& what) 
		requires requires { std::cout << std::forward<ValueType>(what); } 
	{
		return Output(std::cout, std::forward<ValueType>(what));
	}

	template <class... FormatArgs> 
	constexpr auto Print(std::ostream& os, std::string_view formatString, FormatArgs&&... fmtArgs) {
		Output(os, std::vformat(
			formatString,
			std::make_format_args(std::forward<FormatArgs>(fmtArgs)...)
		));
	}

	template <class... FormatArgs> 
	constexpr auto PrintStd(std::string_view formatString, FormatArgs&&... fmtArgs) {
		return Print(std::cout, formatString, std::forward<FormatArgs>(fmtArgs)...);
	}
}