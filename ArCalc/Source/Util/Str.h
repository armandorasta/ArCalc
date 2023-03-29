#pragma once

#include "Core.h"
#include "Exception/ArCalcException.h"
#include "Util.h"

namespace ArCalc::Str {
	constexpr bool IsWhiteSpace(char c) {
		return c == ' ' || c == '\t' || c == '\n';
	}

	constexpr bool IsNotWhiteSpace(char c) {
		return !IsWhiteSpace(c);
	}

	constexpr bool IsAlpha(char c) {
		return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z';
	}

	constexpr bool IsDigit(char c) {
		return c >= '0' && c <= '9';
	}

	constexpr bool IsAlNum(char c) {
		return IsAlpha(c) || IsDigit(c);
	}

	template <std::constructible_from<std::string_view> TReturn = std::string>
	constexpr std::vector<TReturn> SplitOn(std::string_view str, std::string_view chars, 
		bool bChain = true) 
	{
		if (str.empty()) { // Get rid of this edge case quickly.
			return {TReturn{""}};
		}

		std::vector<TReturn> res{};
		size_t il{}, ir{};
		for (; ir < str.size(); ++ir) {
			if (range::none_of(chars, [&](auto c) { return str[ir] == c; })) {
				continue;
			}

			res.push_back(TReturn{str.substr(il, ir - il)});
			if (bChain) {
				// Setting this to false will insert an empty token in between each two 
				// consecutive delimeters, otherwise the function will remove that token.

				while (ir < str.size() && range::any_of(chars, Util::Eq(str[ir]))) {
					ir += 1;
				}
				il = ir;
			}
			else il = ir + 1; // Do not increment ir ffs.
		}
		if (il != str.size()) { // So it does not push an empty token for the sake of it.
			res.push_back(TReturn{str.substr(il, ir - il)});
		}

		return res;
	}

	template <std::constructible_from<std::string_view> TReturn = std::string>
	constexpr std::vector<TReturn> SplitOnSpaces(std::string_view str) {
		return SplitOn<TReturn>(str, " \t");
	}

	namespace Secret {
		struct IndexPair {
			size_t StartIndex;
			size_t EndIndex;
		};

		// Used by GetFirstToken and ChopFirstToken.
		IndexPair GetFirstTokenIndices(std::string_view line);
	}

	template <std::constructible_from<std::string_view> TReturn = std::string>
	TReturn GetFirstToken(std::string_view line) {
		auto const [startIndex, endIndex] {Secret::GetFirstTokenIndices(line)};
		return TReturn{line.substr(startIndex, endIndex - startIndex)};
	}

	template <class TParam, class TReturn = std::string>
		requires std::convertible_to<TParam, std::string_view>
		      && std::constructible_from<TReturn, TParam>
	TReturn ChopFirstToken(TParam& str) {
		auto const [startIndex, endIndex] {Secret::GetFirstTokenIndices(str)};
		auto const firstToken = TReturn{str.substr(startIndex, endIndex - startIndex)};
		str = str.substr(endIndex);
		return firstToken;
	}

	// Trim functions return a string by default for my sanity.

	template <std::constructible_from<std::string_view> TReturn = std::string>
	TReturn TrimLeft(std::string_view str) {
		return TReturn{str.substr(range::distance(
			str | view::take_while([](auto c) { return std::isspace(c); })
		))};
	}

	template <std::constructible_from<std::string_view> TReturn = std::string>
	TReturn TrimRight(std::string_view str) {
		auto const trailingSpaces{str | view::reverse | view::take_while(IsWhiteSpace)};
		return TReturn{str.substr(0, str.size() - range::distance(trailingSpaces))};
	}

	template <std::constructible_from<std::string_view> TReturn = std::string>
	TReturn Trim(std::string_view str) {
		return TrimLeft<TReturn>(TrimRight<std::string_view>(str));
	}

	std::string Mangle(std::string_view paramName, size_t paramIndex);

	struct DemangledName {
		std::string PackName;
		size_t Index;
	};

	constexpr DemangledName Demangle(std::string_view mangledName) {
		if (auto const index{mangledName.find("__", 2)}; index != std::string::npos) {
			return {
				.PackName{std::string{mangledName.substr(2U, index - 2)}},
				.Index{static_cast<size_t>(
					std::atoi(mangledName.substr(index + 2, mangledName.size() - 2).data())
				)}
			};
		} else {
			// This should never happen in release.
			ARCALC_DE("Demangling invalidly mangled name [{}]", mangledName);

#ifdef NDEBUG
			return {};
#endif // ^^^^ Release mode only.
		}

	}

	template <class ValueType> requires std::is_arithmetic_v<ValueType>
	constexpr ValueType FromString(std::string_view str) {
		if (auto const leftIndex{str.find_first_not_of(" \t")}; leftIndex != std::string_view::npos) {
			//                                       v  ^ those spaces are important
			auto const rightIndex{str.find_first_of(" \t", leftIndex)};
			str = str.substr(leftIndex, rightIndex);
		}

		ValueType resNum{};
		auto const res{std::from_chars(str.data(), str.data() + str.size(), resNum)};
		if (auto const [ptr, code] {res}; code != std::errc{}) {
			throw ArCalcException{
				"Invalid Argument; found invalid character [{}] at index [{}]",
				*ptr, ptr - str.data()
			};
		} 

		return resNum;
	}

	template <std::integral IntType>
	constexpr IntType StringToInt(std::string_view str) {
		return FromString<std::decay_t<IntType>>(str);
		/*auto numberRange{str | view::drop_while(IsWhiteSpace)
			                 | view::take_while(IsNotWhiteSpace)};
		ARCALC_EXPECT(!numberRange.empty(), "Passed in a string full of white space");

		if (numberRange.front() == '-') {
			if (std::is_unsigned_v<IntType>) {
				ARCALC_THROW(ArCalcException, "Parsing negative number using unsigned type");
			}
			
			std::make_signed_t<size_t> res{};
			for (auto const c : numberRange | view::drop(1)) {
				ARCALC_EXPECT(IsDigit(c), "Found a non-numeric character [{}]", c);
				res = 10 * res + (c - '0');
			}
			
			return static_cast<IntType>(-res);
		} 
			
		size_t res{};
		for (auto const c : numberRange) {
			ARCALC_EXPECT(IsDigit(c), "Found a non-numeric character [{}]", c);
			res = 10 * res + (c - '0');
		}
			
		return static_cast<IntType>(res);*/
	}

	template <std::integral IntType>
	constexpr std::optional<IntType> StringToIntOpt(std::string_view str) {
		// Can't be done the other way around.
		// This function must be implemented in terms of the StringToInt because this version is not 
		// tested and that one is.
		try { return {StringToInt<IntType>(str)}; }
		catch (ArCalcException&) { return {}; }
	}

	template <std::floating_point FloatType>
	constexpr FloatType StringToFloat(std::string_view str) {
		return FromString<std::decay_t<FloatType>>(str);
	}
}
