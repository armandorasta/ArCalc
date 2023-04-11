#pragma once

#include "Core.h"

namespace ArCalc {
	class ArCalcException {
	public:
		ArCalcException();

		template <class... FormatArgs>
		ArCalcException(std::string_view message, FormatArgs&&... fmtArgs) {
			std::vformat_to(std::back_inserter(m_Message), message, 
				std::make_format_args(std::forward<FormatArgs>(fmtArgs)...));
		}

	public:
		virtual std::string GetType() const = 0;

		void PrintMessage(std::string_view indent) const;
		std::string const& GetMessage() const noexcept;

		template <class... FormatArgs>
		void SetMessage(std::string_view newMessage, FormatArgs&&... fmtArgs) {
			m_Message.clear();
			std::vformat_to(std::back_inserter(m_Message), newMessage, 
				std::make_format_args(std::forward<FormatArgs>(fmtArgs)...));
		}

		constexpr size_t GetLineNumber() const noexcept {
			return m_LineNumber;
		}

		constexpr bool SetLineNumber(size_t toWhat) noexcept {
			if (!bLineNumberSet) { 
				m_LineNumber = toWhat; 
				return true;
			} 
			
			return false; 
		}

		constexpr void LockNumberLine() noexcept {
			bLineNumberSet = true;
		}

	protected:
		std::string m_Message{};

	private:
		size_t m_LineNumber{};
		bool bLineNumberSet{};
	};

	/// Thrown when a debug-only assertion fails. This means that the program can
	/// not continue and must terminate immediately. This may not be thrown in 
	/// Release.
	class DebugAssertionError : public ArCalcException {
	public:
		template <class... FormatArgs>
		DebugAssertionError(char const* fileName, char const* funcName, size_t lineNumber, 
			std::string_view message, FormatArgs&&... fmtArgs) : ArCalcException{""}
		{
			std::vformat_to(
				std::back_inserter(m_Message), 
				std::string{message} + ".\n\tFile: {}.\n\tFunction: {}.\n\tLineNumber: {}",
				std::make_format_args(
					std::forward<FormatArgs>(fmtArgs)...,
					fileName, funcName, lineNumber
				)
			);
		}

		std::string GetType() const override {
			return "DA";
		}
	};

	/// Thrown when an evaluator is passed an invalid expression.
	/// When this is thrown, evaluation is abandoned, and any furthur use of the 
	/// evaluator is undefined.
	class ExprEvalError : public ArCalcException {
	public:
		using ArCalcException::ArCalcException;
		std::string GetType() const override {
			return "EE";
		}
	};

	/// Used by functions in the IO header.
	class IOError : public ArCalcException {
	public:
		using ArCalcException::ArCalcException;
		std::string GetType() const override {
			return "IO";
		}
	};

	/// Thrown by Parser and all evaluators when an error other than a syntax error happens.
	class ParseError : public ArCalcException {
	public:
		using ArCalcException::ArCalcException;
		std::string GetType() const override {
			return "P";
		}
	};

	/// Thrown by Parser and all evaluators.
	class SyntaxError : public ArCalcException {
	public:
		using ArCalcException::ArCalcException;
		std::string GetType() const override {
			return "S";
		}
	};

	/// Thrown by the MathOperator class.
	class MathError : public ArCalcException {
	public:
		using ArCalcException::ArCalcException;
		std::string GetType() const override {
			return "M";
		}
	};

	/// Thrown by functions can't throw any of the other types of errors.
	class GenericError : public ArCalcException {
	public:
		using ArCalcException::ArCalcException;
		std::string GetType() const override {
			return "G";
		}
	};
}

#ifdef NDEBUG

#define ARCALC_DA(_cond, _message, ...)
#define ARCALC_DE(_message, ...)

#define ARCALC_NOT_POSSIBLE(_alwaysFalse)  
#define ARCALC_UNREACHABLE_CODE() throw


#define ARCALC_NOT_IMPLEMENTED(_whatIsNotImplemented)

#else // ^^^^ Release, vvvv Debug

/// ArCalc Debug Assert.
#define ARCALC_DA(_cond, _message, ...)                                      \
	if (!(_cond))                                                            \
		throw ::ArCalc::DebugAssertionError{                                 \
			__FILE__, __FUNCTION__, static_cast<::ArCalc::size_t>(__LINE__), \
			_message, __VA_ARGS__                                            \
		}

/// ArCalc Debug Error.
#define ARCALC_DE(_message, ...) ARCALC_DA(false, _message, __VA_ARGS__)

#define ARCALC_NOT_POSSIBLE(_alwaysFalse) \
	ARCALC_DA(!(_alwaysFalse), "Impossible code path")

#define ARCALC_UNREACHABLE_CODE() \
	ARCALC_DE("Uunreachable code path")

#define ARCALC_NOT_IMPLEMENTED(_whatIsNotImplemented) \
	ARCALC_DE("Unimplemented code path: " _whatIsNotImplemented);

#endif // NDEBUG