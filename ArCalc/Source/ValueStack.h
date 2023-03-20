#pragma once

#include "Core.h"

namespace ArCalc {
	class ValueStack {
	public:
		ValueStack() = default;

	public:
		void Push(double newValue);
		double Pop();
		double Top() const;
		double& Top();
		size_t Size() const;
		bool IsEmpty() const;
		void Clear();

	private:
		std::stack<double> m_Data{};
	};
}