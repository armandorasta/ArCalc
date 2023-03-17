#pragma once

#include "Core.h"

namespace ArCalc::IO {
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

	namespace Secret {
		template <class Condtion>
		concept Not = !Condtion::value;
	}

	template <class ValueType> requires Secret::Not<std::is_const<ValueType>>
	void Input(std::istream& is, ValueType* pOutValue) {
		is >> *pOutValue;
	}

	template <class ValueType>
	auto InputStd(ValueType* pOutValue) requires requires { Input(std::cin, pOutValue); }
	{
		return Input(std::cin, pOutValue);
	}

	template<class ValueType>
	std::ostream& Output(std::ostream& os, ValueType&& what)  requires requires 
	{ os << std::forward<ValueType>(what); } 
	{
		return os << std::forward<ValueType>(what);
	}

	template<class ValueType>
	std::ostream& OutputStd(ValueType&& what) requires requires 
	{ Output(std::cout, std::forward<ValueType>(what)); } 
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
	constexpr auto PrintStd(std::string_view formatString, FormatArgs&&... fmtArgs) requires requires
	{ Print(std::cout, formatString, std::forward<FormatArgs>(fmtArgs)...); }
	{
		return Print(std::cout, formatString, std::forward<FormatArgs>(fmtArgs)...);
	}

	std::string FileToString(fs::path const& filePath, bool bKeepTrailingNulls = false);
	size_t IStreamSize(std::istream& is);

	template <std::default_initializable ByteContainer> requires requires 
		(ByteContainer& con) {
			{ con.data() } -> std::same_as<char*>;
			{ con.size() } -> std::same_as<size_t>;
			con.resize(std::declval<size_t>());
	}
	ByteContainer IStreamToContainer(std::istream& is, bool bKeepTrailingNulls) {
		ByteContainer bytes{};

		if (auto const streamSize{IStreamSize(is)}; streamSize) { 
			bytes.resize(streamSize);
		} 
		else return {}; // Stream is empty? an empty string will do.

		is.read(bytes.data(), bytes.size());
		if (!bKeepTrailingNulls) { // Chop-off the null characters at the end?
			for (size_t i : view::iota(0U, bytes.size()) | view::reverse) {
				if (bytes[i] != '\0') {
					bytes.resize(i + 1);
					break;
				}
			}
		}

		return bytes;
	}

	std::string IStreamToString(std::istream& is, bool bKeepTrailingNulls = false);
}