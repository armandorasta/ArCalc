#pragma once

#include "Core.h"
#include "Exception/ArCalcException.h"

namespace ArCalc::Util {
	constexpr auto Eq(auto const& lhs) {
		return [=](auto const& rhs) { return lhs == rhs; };
	}

	template <class Callable>
	constexpr auto Negate(Callable&& func) {
		return [_func = std::forward<Callable>(func)]
			<class... Args> requires std::invocable<Callable, Args...>
					              && std::same_as<std::invoke_result_t<Callable, Args...>, bool>
			(Args&&... args) {
				return !static_cast<bool>(_func(std::forward<Args>(args)...)); 
			};
	}

	template <class Callable0, class Callable1>
	constexpr auto Intersect(Callable0&& func0, Callable1&& func1) {
		return [_func0 = std::forward<Callable0>(func0), _func1 = std::forward<Callable1>(func1)]
			<class... Args> requires std::invocable<Callable0, Args...>
					              && std::invocable<Callable1, Args...>
					              && std::same_as<std::invoke_result_t<Callable0, Args...>, bool>
					              && std::same_as<std::invoke_result_t<Callable1, Args...>, bool>
			(Args&&... args) {
				return static_cast<bool>(_func0(std::forward<Args>(args)...)) 
					&& static_cast<bool>(_func1(std::forward<Args>(args)...)); 
			};
	}

	template <class Callable0, class Callable1>
	constexpr auto Union(Callable0&& func0, Callable1&& func1) {
		return [_func0 = std::forward<Callable0>(func0), _func1 = std::forward<Callable1>(func1)] 
			<class... Args> requires std::invocable<Callable0, Args...>
			                      && std::invocable<Callable1, Args...>
			                      && std::same_as<std::invoke_result_t<Callable0, Args...>, bool>
			                      && std::same_as<std::invoke_result_t<Callable1, Args...>, bool>
			(Args&&... args) {
				return static_cast<bool>(_func0(std::forward<Args>(args)...)) 
					|| static_cast<bool>(_func1(std::forward<Args>(args)...)); 
			};
	}

	bool IsValidIndentifier(std::string_view what);
}