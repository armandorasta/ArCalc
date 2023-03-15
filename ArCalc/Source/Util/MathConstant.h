#pragma once

#include "Core.h"

namespace ArCalc {
	class MathConstant {
	public:
		MathConstant() = delete;

	private:
		static std::unordered_map<std::string, double> const sc_ConstantMap;

	public:
		static bool IsValid(std::string_view glyph);
		static double ValueOf(std::string_view glyph);
	};
}