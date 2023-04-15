#include "Keyword.h"
#include "../Exception/ArCalcException.h"

namespace ArCalc {
	std::string Keyword::ToString(KT type) {
		return std::string{ToStringView(type)};
	}
}