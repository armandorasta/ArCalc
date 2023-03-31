#pragma once

#include "Core.h"
#include "Exception/ArCalcException.h"

namespace ArCalc::Random {
	namespace Secret {
		inline static std::random_device s_Urbg{};
		inline static std::mt19937 s_Rng{s_Urbg()};
	}

	constexpr std::mt19937& Engine() { return Secret::s_Rng; }

#define ARCALC_DECLARE_RANDOM_INT_FUNC(_funcName, _type) \
	_type _funcName();                                   \
	_type _funcName(_type max);                          \
	_type _funcName(_type min, _type max)

	ARCALC_DECLARE_RANDOM_INT_FUNC(Bool, bool);
	ARCALC_DECLARE_RANDOM_INT_FUNC(SizeT, size_t);
	ARCALC_DECLARE_RANDOM_INT_FUNC(Int, int);
	ARCALC_DECLARE_RANDOM_INT_FUNC(Int32, std::int32_t);
	ARCALC_DECLARE_RANDOM_INT_FUNC(Uint32, std::uint32_t);
	ARCALC_DECLARE_RANDOM_INT_FUNC(Float, float);
	ARCALC_DECLARE_RANDOM_INT_FUNC(Double, double);

#undef ARCALC_DECLARE_RANDOM_INT_FUNC

	template <class ValueType> requires std::is_arithmetic_v<ValueType>
	ValueType Generic(ValueType min, ValueType max) {
		ARCALC_DA(min <= max, "Tried to generate a random number with the range reversed");
		if constexpr (std::is_same_v<ValueType, bool>) {
			return std::bernoulli_distribution{}(Secret::s_Rng);
		} else if constexpr (std::is_integral_v<ValueType>) {
			return std::uniform_int_distribution{min, max}(Secret::s_Rng);
		} else {
			return std::uniform_real_distribution{min, max}(Secret::s_Rng);
		}
	}

	template <class ValueType> requires std::is_arithmetic_v<ValueType>
	ValueType Generic(ValueType max) {
		return Generic(static_cast<ValueType>(0), max);
	}

	template <class ValueType> requires std::is_arithmetic_v<ValueType>
	ValueType Generic() {
		using LimitsType = std::numeric_limits<ValueType>;
		return Generic(LimitsType::min(), LimitsType::max());
	}
}
