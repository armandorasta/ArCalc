#include "MathConstant.h"
#include "Exception/ArCalcException.h"
#include "IO.h"

namespace ArCalc {
	std::unordered_map<std::string, double> MathConstant::s_ConstantMap{
		{"_e", std::numbers::e},
		{"_pi", std::numbers::pi},
		{"_inf", std::numeric_limits<double>::infinity()},

	};

	bool MathConstant::IsValid(std::string_view glyph) {
		return s_ConstantMap.contains(std::string{glyph});
	}

	double MathConstant::ValueOf(std::string_view glyph) {
		auto const it{s_ConstantMap.find(std::string{glyph})}; 
		ARCALC_DA(it != s_ConstantMap.end(), "Value of invalid constant ({})", glyph);
		return it->second;
	}
}
