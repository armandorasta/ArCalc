#include "MathOperator.h"
#include "Exception/ArCalcException.h"

namespace ArCalc {
	enum class MathOperatorType : size_t {
		Unary    = 1UI64 << 0U,
		Binary   = 1UI64 << 1U,
		Ternary  = 1UI64 << 2U,
		Variadic = 1UI64 << 3U,
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
		AddConversionOperators();
	}

	bool MathOperator::IsValid(std::string_view op) {
		return s_Operators.contains(std::string{op});
	}

	bool MathOperator::IsUnary(std::string_view op) {
		return CheckHelper(op, OT::Unary, "IsUnary");
	}

	bool MathOperator::IsBinary(std::string_view op) {
		return CheckHelper(op, OT::Binary, "IsBinary");
	}

	bool MathOperator::IsVariadic(std::string_view op) {
		return CheckHelper(op, OT::Variadic, "IsVariadic");
	}

	double MathOperator::EvalBinary(std::string_view op, double lhs, double rhs) {
		ARCALC_DA(IsValid(op), "MathOperator::EvalBinary on invalid operator: [{}]", op);
		ARCALC_DA(IsBinary(op), "MathOperator::EvalBinary on non-binary operator: [{}]", op);
		return s_Operators.at(std::string{op}).Func({lhs, rhs});
	}

	double MathOperator::EvalUnary(std::string_view op, double operand) {
		ARCALC_DA(IsValid(op), "MathOperator::EvalUnary invalid operator: [{}]", op);
		ARCALC_DA(IsUnary(op), "MathOperator::EvalUnary on non-unary operator: [{}]", op);
		return s_Operators.at(std::string{op}).Func({operand});
	}

	double MathOperator::EvalVariadic(std::string_view op, std::vector<double> const& operands) {
		ARCALC_DA(IsValid(op), "MathOperator::EvalVariadic invalid operator: [{}]", op);
		ARCALC_DA(IsVariadic(op), "MathOperator::EvalVariadic on non-variadic operator: [{}]", op);
		return s_Operators.at(std::string{op}).Func(operands);
	}

	bool MathOperator::CheckHelper(std::string_view op, OT type, std::string_view funcName) {
		ARCALC_DA(IsValid(op), "MathOperator::{} invalid operator: {}", funcName, op);
		return s_Operators.at(std::string{op}).Type & type;
	}

	void MathOperator::AddOperator(std::string const& glyph, OT type,
		std::function<double(std::vector<double>const&)>&& func) 
	{
		s_Operators.insert({glyph,{type, std::move(func)}});
	}

	void MathOperator::AddUnaryOperator(std::string const& glyph, std::function<double(double)>&& func) {
		AddOperator(glyph, OT::Unary, [func = std::move(func)](auto const& operands) {
			return func(operands[0]);
		});
	}

	void MathOperator::AddBinaryOperator(std::string const& glyph, 
		std::function<double(double, double)>&& func) 
	{
		AddOperator(glyph, OT::Binary, [func = std::move(func)](auto const& operands) {
			return func(operands[0], operands[1]);
		});
	}

	void MathOperator::AddTernaryOperator(std::string const& glyph,
		std::function<double(double, double, double)>&& func) 
	{
		AddOperator(glyph, OT::Ternary, [func = std::move(func)](auto const& operands) {
			return func(operands[0], operands[1], operands[2]);
		});
	}

	void MathOperator::AddVariadicOperator(std::string const& glyph,
		std::function<double(std::vector<double> const&)>&& func) 
	{
		AddOperator(glyph, OT::Variadic, std::move(func));
	}


	void MathOperator::AddBasicOperators() {
		// Arithmatic
		AddBinaryOperator("+", std::plus<>{});
		AddBinaryOperator("-", std::minus<>{});
		AddBinaryOperator("*", [](auto l, auto r) { return l * r; });
		AddBinaryOperator("/", [](auto l, auto r) { return l / r; });
		AddBinaryOperator("mod", [](auto l, auto r) {
			if (l < 0.0) {
				throw MathError{"Modulus operator with lhs [{}] (negative)"};
			} else if (std::fmod(l, 1.0) > 0.000001) {
				throw MathError{"Modulus operator with lhs [{}] (non-integer)"};
			}

			if (r < 0.0) {
				throw MathError{"Modulus operator with rhs [{}] (negative)"};
			} else if (std::fmod(r, 1.0) > 0.000001) {
				throw MathError{"Modulus operator with rhs [{}] (non-integer)"};
			}
			return static_cast<double>(static_cast<size_t>(l) % static_cast<size_t>(r)); 
		});

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
		AddBinaryOperator("gcd", [](auto l, auto r) {
			if (l < 0.0) {
				throw MathError{"Operator gcd with lhs [{}] (negative)"};
			} else if (std::fmod(l, 1.0) > 0.000001) {
				throw MathError{"Operator gcd with lhs [{}] (non-integer)"};
			}

			if (r < 0.0) {
				throw MathError{"Operator gcd with rhs [{}] (negative)"};
			} else if (std::fmod(r, 1.0) > 0.000001) {
				throw MathError{"Operator gcd with rhs [{}] (non-integer)"};
			}

			return static_cast<double>(std::gcd(static_cast<size_t>(l), static_cast<size_t>(r)));
		});

		AddVariadicOperator("sum", [](auto const& operands) {
			return std::accumulate(operands.begin(), operands.end(), 0.0);
		});

		AddVariadicOperator("mul", [](auto const& operands) {
			return std::accumulate(operands.begin(), operands.end(), 1.0, std::multiplies<>{});
		});

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
		AddUnaryOperator("fac", &FloatFactorio);
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

	// These will eventually be depricated, and be replaced by the new unit system,
	// but that is not happening any time soon.
	void MathOperator::AddConversionOperators() {
		auto const addRatioConvOp = [](auto fromGlyph, auto toGlyph, auto ratio) {
			AddUnaryOperator(std::string{fromGlyph} + "_to_" + toGlyph,
				[=](auto n) { return n * ratio; });
			AddUnaryOperator(std::string{toGlyph} + "_to_" + fromGlyph,
				[=](auto n) { return n / ratio; });
		};

		// I got these numbers from the windows shitty calculator.

		// Length:
		addRatioConvOp("m", "ft", 3.28084);
		addRatioConvOp("ft", "in", 12.0);
		addRatioConvOp("m", "in", 39.37008);
		addRatioConvOp("lb", "kg", 2.204623);

		// Temperature:
		AddUnaryOperator("cel_to_fah", [](auto n) { return  n * 1.8 + 32.0; });
		AddUnaryOperator("cel_to_kel", [](auto n) { return  n + 273.15; });
		AddUnaryOperator("fah_to_cel", [](auto n) { return (n - 32.0) / 1.8; });
		AddUnaryOperator("fah_to_kel", [](auto n) { return (n + 459.67) / 1.8; });
		AddUnaryOperator("kel_to_cel", [](auto n) { return  n - 273.15; });
		AddUnaryOperator("kel_to_fah", [](auto n) { return n * 1.8 - 459.67; });

		// Energy:
		addRatioConvOp("ev", "j", 1.6e-19);
		addRatioConvOp("cal", "j", 4.184);
		addRatioConvOp("btu", "kj", 1.055056);
		addRatioConvOp("btu", "j", 1.055056e3);

		// Time:
		constexpr std::array names{"year", "month", "day", "hour", "min", "sec"};
		constexpr std::array ratios{413.0 /*does not matter*/, 12.0, 30.0, 24.0, 60.0, 60.0};
		for (auto const i : view::iota(0U, names.size() - 1)) {
			auto mulAcc{1.0};
			for (auto const j : view::iota(i + 1, names.size())) {
				mulAcc *= ratios[j];
				addRatioConvOp(names[i], names[j], mulAcc);
			}
		}

		// Angles:
		AddUnaryOperator("rtod", [](auto o) { return o * 180.0 / std::numbers::pi; });
		AddUnaryOperator("dtor", [](auto o) { return o * std::numbers::pi / 180.0; });
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