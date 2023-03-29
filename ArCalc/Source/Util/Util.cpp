#include "Util.h"
#include "Str.h"

namespace ArCalc::Util {
	bool IsValidIndentifier(std::string_view what) {
		return !what.empty()
			&& std::ranges::all_of(what, Union(Eq('_'), Str::IsAlNum))
			&& !Str::IsDigit(what.front());
	}
}