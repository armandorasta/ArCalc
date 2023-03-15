#pragma once

#include "Core.h"

namespace ArCalc {
	enum class MathOperatorType : size_t {
		Unary   = 0b0001,
		Binary  = 0b0010,
		Ternary = 0b0100,

		UnaryOrBinary = Unary | Binary,
	};

	constexpr static size_t operator&(MathOperatorType lhs, MathOperatorType rhs) {
		return static_cast<size_t>(lhs) & static_cast<size_t>(rhs);
	}

	constexpr static size_t operator|(MathOperatorType lhs, MathOperatorType rhs) {
		return static_cast<size_t>(lhs) | static_cast<size_t>(rhs);
	}

	class MathOperator {
	private:
		using OpType = MathOperatorType;

		struct OpInfo {
			OpType Type;
			std::function<double(std::vector<double> const&)> Func;
		};

		inline static std::unordered_map<std::string, OpInfo> s_Operators{};

	public:

		MathOperator() = delete;

	public:
		static bool IsInitialized();
		static void Initialize();
		static bool IsValid(std::string_view op);
		static bool IsUnary(std::string_view op);
		static bool IsBinary(std::string_view op);

	public:
		static double EvalBinary(std::string_view op, double lhs, double rhs);
		static double EvalUnary(std::string_view op, double operand);

	private:
		static void AddOperator(std::string const& glyph, OpType type,
			std::function<double(std::vector<double> const&)>&& func);
		static void AddUnaryOperator(std::string const& glyph, std::function<double(double)>&& func);
		static void AddBinaryOperator(std::string const& glyph, std::function<double(double, double)>&& func);
		static void AddTernaryOperator(std::string const& glyph, std::function<double(double, double, double)>&& func);
		static void AddBasicOperators();
		static void AddTrigOperators();
		static double FloatFactorio(double n);
	};
}
