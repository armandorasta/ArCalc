#include "MathOperator.h"
#include "Exception/ArCalcException.h"

namespace ArCalc {
	enum class MathOperatorType : size_t {
		Unary   = 0b0001,
		Binary  = 0b0010,
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
		AddBinaryOperator("<",  std::less         <>{});
		AddBinaryOperator("<=", std::less_equal   <>{});
		AddBinaryOperator("==", std::equal_to     <>{});
		AddBinaryOperator("!=", std::not_equal_to <>{});
		AddBinaryOperator(">=", std::greater_equal<>{});
		AddBinaryOperator(">",  std::greater      <>{});
		// Logical
		AddBinaryOperator("&&", [](auto l, auto r) { return l && r             ? 1.0 : 0.0; });
		AddBinaryOperator("||", [](auto l, auto r) { return l || r             ? 1.0 : 0.0; });
		AddBinaryOperator("^^", [](auto l, auto r) { return l && !r || r && !l ? 1.0 : 0.0; });
		// Utils
		AddBinaryOperator("max", [](auto l, auto r) { return std::max(l, r); });
		AddBinaryOperator("min", [](auto l, auto r) { return std::min(l, r); });
		// Unary
		AddUnaryOperator("negate", [](auto o) { return -o; });
		AddUnaryOperator("abs",    [](auto o) { return std::abs(o); });
		AddUnaryOperator("floor",  [](auto o) { return std::floor(o); });
		AddUnaryOperator("ceil",   [](auto o) { return std::ceil(o); });
		AddUnaryOperator("round",  [](auto o) { return std::round(o); });
		AddUnaryOperator("sign",   [](auto o) { return (o > 0.0) ? +1.0 : (o < 0.0) ? -1.0 : 0.0; });
		AddUnaryOperator("sqrt",   [](auto o) { 
			if (o >= 0.0) { return std::sqrt(o); }
			else          { throw MathError{"Square root of negative number [{}]", o}; };
		});
		// Probability
		AddUnaryOperator("!",      [](auto o) { return FloatFactorio(o); });
		AddUnaryOperator("!!",     [](auto o) { return o == 0.0; });
		AddBinaryOperator("perm", [](auto l, auto r) { 
			return FloatFactorio(l) / FloatFactorio(l - r);
		});
		AddBinaryOperator("choose", [](auto l, auto r) {
			return FloatFactorio(l) / (FloatFactorio(r) * FloatFactorio(l - r));
		});
		// Exponential
		AddBinaryOperator("^",    [](auto l, auto r) { return std::pow(l, r); });
		AddUnaryOperator("exp",   [](auto o) { return std::pow(std::numbers::e, o); });
		AddUnaryOperator("ln", [](auto o) {
			if (o >= 0.0) { return std::log(o); }
			else          { throw MathError{"ln of nagative number [{}]", o}; }
		});
		AddUnaryOperator("log2",  [](auto o) {
			if (o >= 0.0) { return std::log2(o); }
			else          { throw MathError{"log2 of nagative number [{}]", o}; }
		});
		AddUnaryOperator("log10", [](auto o) {
			if (o >= 0.0) { return std::log10(o); }
			else          { throw MathError{"log10 of nagative number [{}]", o}; } 
		});
	}

	void MathOperator::AddTrigOperators() {
#define ADD_TRIG_FUNC_BASE(_glyph, _operation)          \
	AddOperator(#_glyph, OT::Unary, [](auto operands) { \
		return _operation;                              \
	})

#define ADD_TRIG_FUNC_SHORT(_reg) \
	ADD_TRIG_FUNC_BASE(_reg, std::_reg(operands[0]))

#define ADD_TRIG_FUNC_REG_REV(_reg, _rev)                  \
	ADD_TRIG_FUNC_SHORT(_reg);                             \
	ADD_TRIG_FUNC_BASE(_rev, 1.0 / std::_reg(operands[0]))

#define ADD_TRIG_FUNC(_reg, _rev)                \
	ADD_TRIG_FUNC_REG_REV(_reg, _rev);           \
	ADD_TRIG_FUNC_REG_REV(_reg ## h, _rev ## h); \
	ADD_TRIG_FUNC_SHORT(a ## _reg);              \
	ADD_TRIG_FUNC_SHORT(a ## _reg ## h)

		ADD_TRIG_FUNC(sin, csc);
		ADD_TRIG_FUNC(cos, sec);
		ADD_TRIG_FUNC(tan, cot);

#undef ADD_TRIG_FUNC_BASE
#undef ADD_TRIG_FUNC_SHORT
#undef ADD_TRIG_FUNC_REG_REV
#undef ADD_TRIG_FUNC
	}

	double MathOperator::FloatFactorio(double n) {
		if (n < 0.0) {
			throw MathError{"Factorio of negative number [{}]", n};
		} else if (std::fmod(n, 1.0) > 0.000001) {
			throw MathError{"Factorio of number has digits after floating point [{}]", n};
		} 

		auto res{static_cast<size_t>(n)};
		for (size_t i : view::iota(1U, res)) {
			res *= i;
		}
		return static_cast<double>(res);
	}
}
