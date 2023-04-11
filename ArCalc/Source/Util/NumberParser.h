#pragma once

#include "Core.h"

namespace ArCalc {
	struct NumberParserResult {
		bool IsDone;
		double Value;
	};

	class NumberParser {
	private:
		enum class St : size_t;

		bool IsNormalSt(St st);
		bool IsBinarySt(St st);
		bool IsOctalSt(St st);
		bool IsHexSt(St st);

	public:
		NumberParser();
		NumberParser(NumberParser const&)            = default;
		NumberParser(NumberParser&&)                 = default;
		NumberParser& operator=(NumberParser const&) = default;
		NumberParser& operator=(NumberParser&&)      = default;

	public:
		NumberParserResult Parse(char c);

		void Reset();

	private:
		double ValidateAndFixParsedNumber();

		constexpr St GetState() const { 
			return m_CurrState;
		}
		
		constexpr void SetState(St toWhat) { 
			m_CurrState = toWhat; 
		}

		void AddChar(char c);
		
		constexpr std::string const& GetAcc() const {
			return m_StringAcc;
		}

		constexpr void ResetAcc() {
			m_StringAcc.clear();
		}

		constexpr bool IsNumberDigit(char c) {
			switch (c) {
			case '0': case '1': case '2': case '3': case '4': 
			case '5': case '6': case '7': case '8': case '9':
				return true;
			default:
				return false;
			}
		}

		constexpr bool IsValidNumberToken(char c) {
			return IsHexDigit(c)
				|| IsBaseSpec(c)
				|| c == '.'
				|| c == '\'';
		}

		constexpr bool IsBaseSpec(char c) {
			switch (c) {
			case 'o': case 'O':
			case 'x': case 'X':
			case 'b': case 'B':
				return true;
			default:
				return false;
			}
		}

		constexpr bool IsHexDigit(char c) {
			return IsNumberDigit(c) || [&] {
				switch (c) {
				case 'a': case 'A':
				case 'b': case 'B':
				case 'c': case 'C':
				case 'd': case 'D':
				case 'e': case 'E':
				case 'f': case 'F':
					return true;
				default:
					return false;
				}
			}(/*)(*/);
		}

	private:
		std::string m_StringAcc{};
		St m_CurrState{};
	};
}