#pragma once

#include "Core.h"
#include "Exception/ArCalcException.h"
#include "Util.h"

namespace ArCalc::Str {
	template <std::constructible_from<std::string_view> TReturn = std::string>
	constexpr std::vector<TReturn> SplitOn(std::string_view str, std::string_view chars, bool bChain = true) {
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
		return SplitOn(str, " \t");
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
		return TReturn{str.substr(0, str.size() - range::distance(
			str
			| view::reverse
			| view::take_while([](auto c) { return std::isspace(c); })
		))};
	}

	template <std::constructible_from<std::string_view> TReturn = std::string>
	TReturn Trim(std::string_view str) {
		return TrimLeft<TReturn>(TrimRight<std::string_view>(str));
	}

	std::string FileToString(fs::path const& filePath);
	std::string Mangle(std::string_view paramName, size_t paramIndex);

	struct DemangledName {
		std::string PackName;
		size_t Index;
	};

	constexpr DemangledName Demangle(std::string_view mangledName) {
		if (auto const index{mangledName.find("__", 2)}; index != std::string::npos) {
			return {
				.PackName{std::string{mangledName.substr(2U, index - 2)}},
				.Index{static_cast<size_t>(std::atoi(mangledName.substr(index + 2, mangledName.size() - 2).data()))}
			};
		} 
		else ARCALC_ERROR("Demangling invalidly mangled name [{}]", mangledName);
	}
}
