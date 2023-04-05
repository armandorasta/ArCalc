#include "MathOperator.h"
#include "Exception/ArCalcException.h"

namespace ArCalc {
	enum class MathOperatorType : size_t {
		Unary = 0b0001,
		Binary = 0b0010,
		Ternary = 0b0100,

		UnaryOrBinary = Unary | Binary,
	};

	MathOperator const MathOperator::s_InitializationInstance{};

	MathOperator::MathOperator() {
		if (!IsInitialized()) {
			Initialize();
		}
	}

	bool MathOperator::IsInitialized() {
		return !s_Operators.empty();
	}

	void MathOperator::Initialize() {
		ARCALC_DA(!IsInitialized(), "Double initialization of MathOperator");
		AddBasicOperators();
		AddTrigOperators();
	}

	bool MathOperator::IsValid(std::string_view op) {
		return s_Operators.contains(std::string{op});
	}

	bool MathOperator::IsUnary(std::string_view op) {
		ARCALC_DA(IsValid(op), "[IsUnary] Invalid operator: {}", op);
		return s_Operators.at(std::string{op}).Type & OT::Unary;
	}

	bool MathOperator::IsBinary(std::string_view op) {
		ARCALC_DA(IsValid(op), "[IsBinary] Invalid operator: {}", op);
		return s_Operators.at(std::string{op}).Type & OT::Binary;
	}

	double MathOperator::EvalBinary(std::string_view op, double lhs, double rhs) {
		if (!IsValid(op)) {
			throw ExprEvalError{"[EvalBinary] Invalid operator: {}", op};
		} else if (!IsBinary(op)) {
			throw ExprEvalError{"Evaluating non-binary operator [{}] using EvalBinary", op};
		}

		return s_Operators.at(std::string{op}).Func({lhs, rhs});
	}

	double MathOperator::EvalUnary(std::string_view op, double operand) {
		if (!IsValid(op)) {
			throw ExprEvalError{"[EvalUnary] Invalid operator: {}", op};
		} else if (!IsUnary(op)) {
			throw ExprEvalError{"Evaluating non-unary [{}] operator using EvalUnary", op};
		}

		return s_Operators.at(std::string{op}).Func({operand});
	}

	void MathOperator::AddOperator(std::string const& glyph, OT type,
		std::function<double(std::vector<double>const&)>&& func) {
		s_Operators.insert({glyph,{type, std::move(func)}});
	}

	void MathOperator::AddUnaryOperator(std::string const& glyph, std::function<double(double)>&& func) {
		AddOperator(glyph, OT::Unary, [func = std::move(func)](auto const& operands) {
			return func(operands[0]);
		});
	}

	void MathOperator::AddBinaryOperator(std::string const& glyph, std::function<double(double, double)>&& func) {
		AddOperator(glyph, OT::Binary, [func = std::move(func)](auto const& operands) {
			return func(operands[0], operands[1]);
		});
	}

	void MathOperator::AddTernaryOperator(std::string const& glyph,
		std::function<double(double, double, double)>&& func) {
		AddOperator(glyph, OT::Ternary, [func = std::move(func)](auto const& operands) {
			return func(operands[0], operands[1], operands[2]);
		});
	}

	void MathOperator::AddBasicOperators() {
		// Arithmatic
		AddBinaryOperator("+", std::plus<>{});
		AddBinaryOperator("-", std::minus<>{});
		AddBinaryOperator("*", [](auto l, auto r) { return l * r; });
		AddBinaryOperator("/", [](auto l, auto r) { return l / r; });
		// Relational
		AddBinaryOperator("<", std::less          <>{});
		AddBinaryOperator("<=", std::less_equal   <>{});
		AddBinaryOperator("==", std::equal_to     <>{});
		AddBinaryOperator("!=", std::not_equal_to <>{});
		AddBinaryOperator(">=", std::greater_equal<>{});
		AddBinaryOperator(">", std::greater       <>{});
		// Logical
		AddBinaryOperator("&&", [](auto l, auto r) { return l && r ? 1.0 : 0.0; });
		AddBinaryOperator("||", [](auto l, auto r) { return l || r ? 1.0 : 0.0; });
		AddBinaryOperator("^^", [](auto l, auto r) { return l && !r || r && !l ? 1.0 : 0.0; });
		AddUnaryOperator("!",   [](auto o        ) { return std::abs(o) < 0.000001; });
		// Utils
		AddBinaryOperator("max", [](auto l, auto r) { return std::max(l, r); });
		AddBinaryOperator("min", [](auto l, auto r) { return std::min(l, r); });
		AddUnaryOperator("rtod", [](auto o) { return o * 180.0 / std::numbers::pi; });
		AddUnaryOperator("dtor", [](auto o) { return o * std::numbers::pi / 180.0; });
		// Unary
		AddUnaryOperator("negate", [](auto o) { return -o; });
		AddUnaryOperator("abs",    [](auto o) { return std::abs(o); });
		AddUnaryOperator("floor",  [](auto o) { return std::floor(o); });
		AddUnaryOperator("ceil",   [](auto o) { return std::ceil(o); });
		AddUnaryOperator("round",  [](auto o) { return std::round(o); });
		AddUnaryOperator("sign",   [](auto o) { return (o > 0.0) ? +1.0 : (o < 0.0) ? -1.0 : 0.0; });
		AddUnaryOperator("sqrt", [](auto o) {
			AssertNotInfinity(o, "sqrt");
			AssertGreaterThan(o, 0, "sqrt");
			return std::sqrt(o); 
		});
		// Probability
		AddUnaryOperator("fac", [](auto o) {
			return FloatFactorio(o);
		});
		AddBinaryOperator("perm", [](auto l, auto r) {
			return FloatFactorio(l) / FloatFactorio(l - r);
		});
		AddBinaryOperator("choose", [](auto l, auto r) {
			return FloatFactorio(l) / (FloatFactorio(r) * FloatFactorio(l - r));
		});
		// Exponential
		AddBinaryOperator("^", [](auto l, auto r) {
			return std::pow(l, r);
		});
		AddUnaryOperator("exp", [](auto o) {
			return std::pow(std::numbers::e, o);
		});
		AddUnaryOperator("ln", [](auto o) {
			AssertNotInfinity(o, "ln");
			AssertGreaterThan(o, 0.0, "ln");
			return std::log(o);
		});
		AddUnaryOperator("log2", [](auto o) {
			AssertNotInfinity(o, "log2");
			AssertGreaterThan(o, 0.0, "log2");
			return std::log2(o);
		});
		AddUnaryOperator("log10", [](auto o) {
			AssertNotInfinity(o, "log10");
			AssertGreaterThan(o, 0.0, "log10");
			return std::log10(o);
		});
	}

	void MathOperator::AddTrigOperators() {
		auto const addTrig = [&](auto regGlyph, auto revGlyph, auto func) {
			AddUnaryOperator(regGlyph, [=](auto o) {
				AssertNotInfinity(o, regGlyph);
				return func(o);
			});
			AddUnaryOperator(revGlyph, [=](auto o) {
				AssertNotInfinity(o, revGlyph);
				return 1.0 / func(o);
			});
		};

		auto addArcTrig = [&](auto glyph, auto func) {
			AddUnaryOperator(glyph, [=](auto o) {
				AssertInRange(o, -1.0, 1.0, glyph);
				return func(o);
			});
		};

		addTrig("sin", "csc", [](auto o) { return std::sin(o); });
		addTrig("cos", "sec", [](auto o) { return std::cos(o); });
		addTrig("tan", "cot", [](auto o) { return std::tan(o); });

		addTrig("sinh", "csch", [](auto o) { return std::sinh(o); });
		addTrig("cosh", "sech", [](auto o) { return std::cosh(o); });
		addTrig("tanh", "coth", [](auto o) { return std::tanh(o); });

		addArcTrig("arcsin", [](auto o) { return std::asin(o); });
		addArcTrig("arccos", [](auto o) { return std::acos(o); });
		addArcTrig("arctan", [](auto o) { return std::atan(o); });

		addArcTrig("arcsinh", [](auto o) { return std::asinh(o); });
		addArcTrig("arccosh", [](auto o) { return std::acosh(o); });
		addArcTrig("arctanh", [](auto o) { return std::atanh(o); });
	}

	double MathOperator::FloatFactorio(double n) {
		AssertGreaterThan(n, 0.0, "fac");
		AssertInteger(n, "fac");

		auto res{static_cast<size_t>(n)};
		for (size_t i : view::iota(1U, res)) {
			res *= i;
		}
		return static_cast<double>(res);
	}

	void MathOperator::AssertInRange(double n, double min, double max, std::string_view funcName) {
		if (n < min || n > max) {
			throw MathError{
				"Tried to take `{}` of {}, while `{}` is only defined in [{}, {}]",
				funcName, n, funcName, min, max,
			};
		}
	}

	void MathOperator::AssertGreaterThan(double n, double min, std::string_view funcName) {
		if (n <= min) {
			throw MathError{
				"Tried to take `{}` of {}, while `{}` is only defined for values greater than [{}]",
				funcName, n, funcName, min,
			};
		}
	}

	void MathOperator::AssertLessThan(double n, double max, std::string_view funcName) {
		if (n >= max) {
			throw MathError{
				"Tried to take `{}` of {}, while `{}` is only defined for values less than [{}]",
				funcName, n, funcName, max,
			};
		}
	}

	void MathOperator::AssertNotInfinity(double n, std::string_view funcName) {
		if (n == std::numeric_limits<double>::infinity()) {
			throw MathError{"Tried to take `{}` of infinity", funcName};
		} else if (n == -std::numeric_limits<double>::infinity()) {
			throw MathError{"Tried to take `{}` of negative infinity", funcName};
		}
	}

	void MathOperator::AssertInteger(double n, std::string_view funcName) {
		if (std::fmod(n, 1.0) > 0.000001) {
			throw MathError{
				"Tried to take `{}` of {}, while `{}` is only defined for integers",
				funcName, n, funcName,
			};
		}
	}
}