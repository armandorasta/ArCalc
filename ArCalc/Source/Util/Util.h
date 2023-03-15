#pragma once

#include "Core.h"
#include "Exception/ArCalcException.h"

#define ARCALC_MAKE_PUBLIC(_originalSpec, ...) public: __VA_ARGS__ _originalSpec

namespace Util {
	constexpr auto FuncName(std::source_location debugInfo = {}) {
		return debugInfo.function_name();
	}

	constexpr auto Eq(auto const& lhs) {
		return [=](auto const& rhs) { return lhs == rhs; };
	}
}