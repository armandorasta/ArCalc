#pragma once

#include "Core.h"

namespace ArCalc {
	class IEvaluator {
	public:
		virtual std::optional<double> Eval(std::string_view exprString) = 0;
	};
}