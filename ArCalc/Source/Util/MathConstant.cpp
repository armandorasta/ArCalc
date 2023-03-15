#include "MathConstant.h"
#include "Exception/ArCalcException.h"

namespace ArCalc {
	std::unordered_map<std::string, double> const MathConstant::sc_ConstantMap{
		{"e", std::numbers::e},
		{"pi", std::numbers::pi},
	};

	bool MathConstant::IsValid(std::string_view glyph) {
		return sc_ConstantMap.contains(std::string{glyph});
	}

	double MathConstant::ValueOf(std::string_view glyph) {
		if (auto const it{sc_ConstantMap.find(std::string{glyph})}; it != sc_ConstantMap.end()) {
			return it->second;
		} 
		else ARCALC_THROW(ArCalcException, "Value of invalid constant ({})", glyph);
	}
}
