#include "MathConstant.h"
#include "Exception/ArCalcException.h"
#include "IO.h"

namespace ArCalc {
	std::unordered_map<std::string, double> MathConstant::s_ConstantMap{
		{"e", std::numbers::e},
		{"pi", std::numbers::pi},
		{"_Inf", std::numeric_limits<double>::infinity()},
	};

	bool MathConstant::IsValid(std::string_view glyph) {
		return s_ConstantMap.contains(std::string{glyph});
	}

	double MathConstant::ValueOf(std::string_view glyph) {
		if (auto const it{s_ConstantMap.find(std::string{glyph})}; it != s_ConstantMap.end()) {
			return it->second;
		} 
		else throw ArCalcException{"Value of invalid constant ({})", glyph};
	}
}
