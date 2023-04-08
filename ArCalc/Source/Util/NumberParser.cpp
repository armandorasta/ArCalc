#include "NumberParser.h"
#include "Exception/ArCalcException.h"
#include "Str.h"

namespace ArCalc {
	constexpr size_t NormBit  {1U << 31U};
	constexpr size_t HexBit   {1U << 30U};
	constexpr size_t OctalBit {1U << 29U};
	constexpr size_t BinaryBit{1U << 28U};
	constexpr size_t AllBits{NormBit | HexBit | OctalBit | BinaryBit};

	enum class NumberParser::St : size_t {
		Default      = 0U,
		ParsingBegin = 1U,
		FracPart     = 2U,
		FoundExp     = 3U,
		LeadingZero  = 4U,
	};

	bool NumberParser::IsNormalSt(St st) {
		return static_cast<size_t>(st) & NormBit;
	}

	bool NumberParser::IsBinarySt(St st) {
		return static_cast<size_t>(st) & BinaryBit;
	}

	bool NumberParser::IsOctalSt(St st) {
		return static_cast<size_t>(st) & OctalBit;
	}

	bool NumberParser::IsHexSt(St st) {
		return static_cast<size_t>(st) & HexBit;
	}

	NumberParser::NumberParser() : m_CurrState{St::ParsingBegin} {
	}

	void NumberParser::AddChar(char c) {
		m_StringAcc.push_back(c);
	}

	void NumberParser::Reset() {
		ResetAcc();
		SetState(St::ParsingBegin);
	}

	NumberParserResult NumberParser::Parse(char c) {
		switch (auto const state{GetState()}; state) {
		case St::ParsingBegin: 
			AddChar(c);
			SetState(St::Default);
			return {.IsDone{false}};
		case St::Default:
			if (IsHexDigit(c) || IsBaseSpec(c) || c == '.' || c == '\'') {
				AddChar(c);
				return {.IsDone{false}};
			} else if (std::isalpha(c)) { // Alphabetic but not valid hex or base spec.
				throw ParseError{
					"Found invalid character [{}] while parsing number [{}]",
					c, GetAcc(),
				};
			} else { // Symbol or whitespace.
				SetState(St::ParsingBegin);
				NumberParserResult res{
					.IsDone{true},
					.Value{ValidateAndFixParsedNumber()},
				};
				ResetAcc();
				return res;
			}

			break; // Do not remove this shit.
		default:
			ARCALC_UNREACHABLE_CODE();
		};

		ARCALC_UNREACHABLE_CODE();
	}

	double NumberParser::ValidateAndFixParsedNumber() {
		auto numStr = std::string_view{GetAcc()}; 
		
		auto const bMinus = [&]{
			auto const cond{numStr.front() == '-'};
			numStr = numStr.substr(cond ? 1 : 0);
			return cond;
		}(/*)(*/);

		auto myNum = double{};
		auto base = size_t{10U};
		auto st{St::Default};
		auto fracDigitCount = size_t{};

		auto bNegativeExp{false};
		auto expAcc = size_t{};

		if (auto const c0{numStr.front()}; IsNumberDigit(c0)) {
			if (c0 == '0') {
				st = St::LeadingZero;
			} else {
				myNum = 1.0 * c0 - '0';
			}
		} else if (c0 == '.') {
			st = St::FracPart;
		} else {
			// Should not happen because the parser would not be able to 
			// know it's a number in the first place.
			ARCALC_UNREACHABLE_CODE();
		}

		for (auto it{std::next(numStr.cbegin())}; it < numStr.end(); ++it) {
			auto const c{*it};
			switch (st) {
			case St::LeadingZero:
				if (IsNumberDigit(c)) {
					myNum = 10.0 * myNum + (1.0 * c - '0');
				} else switch (c) {
				case 'b': case 'B': base = 2; break;
				case 'o': case 'O': base = 8; break;
				case 'x': case 'X': base = 16; break;
				}
				
				st = St::Default;
				break;
			case St::Default: // Parsing decimal number
				if (IsNumberDigit(c)) {
					myNum = base * myNum + (1.0 * c - '0');
				} else if (base == 16 && IsHexDigit(c)) {
					myNum = base * myNum 
						+ (1.0 * c - (std::tolower(c) == c ? 'a' : 'A')) 
						+ 10.0;
				} else switch (c) {
				case '\'': 
					if (*std::prev(it) == '\'') {
						throw ParseError{
							"Found two `'` in a row while parsing number [{}]",
							numStr,
						};
					}

					// Ignore it.
					break;
				case '.':
					if (*std::prev(it) == '\'') {
						throw ParseError{
							"Found `'` just before the floating point while parsing number [{}]",
							numStr,
						};
					}
					st = St::FracPart;
					break;
				case 'e':
					if (base == 10) {
						st = St::FoundExp;
						break;
					} 

					[[fallthrough]];
				default:
					throw ParseError{
						"Found invalid character [{}] while parsing number [{}]",
						c, numStr,
					};
				}

				break;
			case St::FracPart:
				if (IsNumberDigit(c)) {
					myNum = base * myNum + (1.0 * c - '0');
					fracDigitCount += 1;
				} else if (base == 16 && IsHexDigit(c)) {
					myNum = base * myNum 
						+ (1.0 * c - (std::tolower(c) == c ? 'a' : 'A')) 
						+ 10.0;
					fracDigitCount += 1;
				} else switch (c) {
				case '\'': 
					if (auto const prev{*std::prev(it)}; prev == '.') {
						throw ParseError{
							"Found `'` right after the floating point while parsing number [{}]",
							numStr,
						};
					} else if (prev == '\'') {
						throw ParseError{
							"Found two `'` in a row while parsing number [{}]",
							numStr,
						};
					}

					// Ignore it.
					break;
				case '.':
					throw ParseError{
						"Found multiple floating points while parsing number [{}]", 
						numStr,
					};
				case 'e':
					if (base == 10) {
						st = St::FoundExp;
						break;
					}

					[[fallthrough]];
				default:
					throw ParseError{
						"Found invalid character [{}] while parsing number [{}]",
						c, numStr,
					};
				}

				break;
			case St::FoundExp:
				if (IsNumberDigit(c)) {
					expAcc = expAcc * 10 + (static_cast<size_t>(c) - '0');
				} else switch (c) {
				case '-': 
					if (*std::prev(it) == 'e') {
						bNegativeExp = true;
					} else throw ParseError{
						"Found a `-` in an invalid location while parsing number [{}]",
						numStr,
					};

					break;
				case '\'':
					if (auto const prev{*std::prev(it)}; prev == 'e') {
						throw ParseError{
							"Found a `'` just after the `e` while parsing number [{}]",
							numStr,
						};
					} else if (prev == '\'') {
						throw ParseError{
							"Found two `'` in a row in exponent while parsing number [{}]",
							numStr,
						};
					}

					// Ignore it.
					break;
				default:
					throw ParseError{
						"Found invalid character [{}] in exponent while parsing number [{}]",
						c, numStr,
					};
				}
				break;
			default:
				ARCALC_UNREACHABLE_CODE();
			}
		}

		if (fracDigitCount > 0) {
			myNum /= [&] {
				auto bases{base};
				for ([[maybe_unused]] auto const i : view::iota(1U, fracDigitCount)) {
					bases *= base;
				}
				return bases;
			}(/*)(*/);
		}

		if (expAcc > 0) {
			for ([[maybe_unused]] auto const p : view::iota(0U, expAcc)) {
				myNum *= 10.0;
			}
			if (bNegativeExp) {
				myNum = 1.0 / myNum;
			}
		}

		return bMinus ? -myNum : myNum;
	}
}
