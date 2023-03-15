#include "Random.h"

#define ARCALC_DEFINE_RANDOM_INT_FUNC(_funcName, _type) \
	_type _funcName() { return Generic<_type>(); } \
	_type _funcName(_type max) { return Generic<_type>(max);	} \
	_type _funcName(_type min, _type max) { return Generic<_type>(min, max); }

namespace ArCalc::Random {

	ARCALC_DEFINE_RANDOM_INT_FUNC(Bool, bool);
	ARCALC_DEFINE_RANDOM_INT_FUNC(SizeT, size_t);
	ARCALC_DEFINE_RANDOM_INT_FUNC(Int, int);
	ARCALC_DEFINE_RANDOM_INT_FUNC(Int32, std::int32_t);
	ARCALC_DEFINE_RANDOM_INT_FUNC(Uint32, std::uint32_t);
	ARCALC_DEFINE_RANDOM_INT_FUNC(Float, float);
	ARCALC_DEFINE_RANDOM_INT_FUNC(Double, double);

}

#undef ARCALC_DEFINE_RANDOM_INT_FUNC