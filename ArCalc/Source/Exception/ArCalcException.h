#pragma once

#include "Core.h"

namespace ArCalc {
	class ArCalcException {
	public:
		ArCalcException() : ArCalcException{"No message"} {}

		ArCalcException(std::string_view message) {
			SetMessage(message);
		}

	public:
		std::string GetMessage() const noexcept {
			return std::format("Error ({}): {}", GetLineNumber(), m_Message);
		}

		constexpr void SetMessage(std::string_view newMessage) {
			m_Message = newMessage;
		}

		constexpr size_t GetLineNumber() const {
			return m_LineNumber;
		}

		constexpr bool SetLineNumber(size_t toWhat) {
			if (bLineNumberSet) { 
				return false; 
			} else { 
				m_LineNumber = toWhat; 
				return true;
			}
		}

		constexpr void LockNumberLine() {
			bLineNumberSet = true;
		}

	private:
		std::string m_Message{};
		size_t m_LineNumber{};
		bool bLineNumberSet{};
	};
}

#define ARCALC_UNREACHABLE_CODE() \
	{ \
		assert(!"Should be unreachable"); \
		std::unreachable(); \
	}

#define ARCALC_NOT_IMPLEMENTED() \
	{ \
		assert(!"Not implemented yet"); \
		std::unreachable(); \
	}


/// This macro is the same as ARCALC_ASSERT, except it survives in release builds as well.
#define ARCALC_EXPECT(_cond, _message, ...) \
	if (!(_cond)) throw ::ArCalc::ArCalcException{ \
		std::vformat(_message, std::make_format_args(__VA_ARGS__)) \
	}

/// This macro is the same as ARCALC_ERROR, except it survives in release builds as well.
#define ARCALC_THROW(_type, _message, ...)	\
	throw ::ArCalc::_type{std::vformat(_message, std::make_format_args(__VA_ARGS__))}

#ifdef NDEBUG

ARCALC_ASSERT(_cond, _message, ...)
ARCALC_ERROR(_message, ...)

#else // ^^^^^^ NDEBUG  vvvvvvv !NDEBUG

#define ARCALC_ASSERT(_cond, _message, ...) ARCALC_EXPECT(_cond, _message, __VA_ARGS__)
#define ARCALC_ERROR(_message, ...)         ARCALC_THROW(ArCalcException, _message, __VA_ARGS__)

#endif // NDEBUG
