#pragma once

#include <vector>
#include <cassert>
#include <map>
#include <list>
#include <unordered_map>
#include <string>
#include <string_view>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <ranges>
#include <concepts>
#include <type_traits>
#include <functional>
#include <numbers>
#include <variant>
#include <array>
#include <stack>
#include <format>
#include <exception>
#include <stdexcept>
#include <optional>
#include <source_location>
#include <filesystem>
#include <stacktrace>
#include <random>
#include <charconv>

namespace ArCalc {
	using size_t    = std::size_t;
	using int32_t   = std::int32_t;

	namespace view  = std::views;
	namespace range = std::ranges;
	namespace fs    = std::filesystem;
}