#pragma once

#include "Core.h"

namespace ArCalc {
	class ArCalcException {
	public:
		ArCalcException() : ArCalcException{"No message"} {}

		template <class... FormatArgs>
		ArCalcException(std::string_view message, FormatArgs&&... fmtArgs) {
			std::vformat_to(std::back_inserter(m_Message), message, 
				std::make_format_args(std::forward<FormatArgs>(fmtArgs)...));
		}

	public:
		std::string GetMessage() const noexcept {
			return std::format("Error ({}): {}", GetLineNumber(), m_Message);
		}

		template <class... FormatArgs>
		void SetMessage(std::string_view newMessage, FormatArgs&&... fmtArgs) {
			m_Message.clear();
			std::vformat_to(std::back_inserter(m_Message), newMessage, 
				std::make_format_args(std::forward<FormatArgs>(fmtArgs)...));
		}

		constexpr size_t GetLineNumber() const {
			return m_LineNumber;
		}

		constexpr bool SetLineNumber(size_t toWhat) {
			if (!bLineNumberSet) { 
				m_LineNumber = toWhat; 
				return true;
			} 
			
			return false; 
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

#define ARCALC_NOT_POSSIBLE(_alwaysFalse) \
	assert(!(_alwaysFalse) && "Reached a condition assumed to be impossible")

#define ARCALC_UNREACHABLE_CODE()         \
	{                                     \
		assert(!"Should be unreachable"); \
		std::unreachable();               \
	}

#define ARCALC_NOT_IMPLEMENTED()        \
	{                                   \
		assert(!"Not implemented yet"); \
		std::unreachable();             \
	}


/// This macro is the same as ARCALC_DA, except it survives in release builds as well.
#define ARCALC_EXPECT(_cond, _message, ...) \
	if (!(_cond)) throw ::ArCalc::ArCalcException{_message, __VA_ARGS__} \

#ifdef NDEBUG

#define ARCALC_DA(_cond, _message, ...)
#define ARCALC_DE(_message, ...)

#else // ^^^^^^ NDEBUG  vvvvvvv !NDEBUG

/// ArCalc Debug Assert.
#define ARCALC_DA(_cond, _message, ...) ARCALC_EXPECT(_cond, _message, __VA_ARGS__)

/// ArCalc Debug Error.
#define ARCALC_DE(_message, ...)        throw ::ArCalc::ArCalcException{_message, __VA_ARGS__}

#endif // NDEBUG
