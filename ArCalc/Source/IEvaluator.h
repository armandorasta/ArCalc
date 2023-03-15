#pragma once

#include "Core.h"

namespace ArCalc {
	class IEvaluator {
	public:
		virtual double Eval(std::string const& exprString) = 0;
	};
}