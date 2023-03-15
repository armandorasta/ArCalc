#include "pch.h"

#include <Util/Util.h>
#include <Util/Random.h>

#include "ArCalcSourceFiles.h"

using namespace ArCalc;

class UtilTests : public testing::Test {
};

TEST_F(UtilTests, Eq) {
	constexpr std::int32_t Min{-50};
	constexpr std::int32_t Max{+50};

	for (auto const currLhs : view::iota(Min, Max)) {
		auto const func{Util::Eq(currLhs)};
		for (auto const currRhs : view::iota(Min, Max)) {
			ASSERT_TRUE(currLhs == currRhs ? func(currRhs) : !func(currRhs));
		}
	}
}
