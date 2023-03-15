#include "ValueStack.h"
#include "Exception/ArCalcException.h"

namespace ArCalc {
	void ValueStack::Push(double newValue) {
		m_Data.push(newValue);
	}

	double ValueStack::Pop() {
		ARCALC_ASSERT(!m_Data.empty(), "Popped empty ValueStack");
		auto const value{m_Data.top()};
		m_Data.pop();
		return value;
	}

	double ValueStack::Top() const {
		return const_cast<ValueStack&>(*this).Top();
	}

	double& ValueStack::Top() {
		ARCALC_ASSERT(!m_Data.empty(), "Tried to get top from empty ValueStack");
		return m_Data.top();
	}

	size_t ValueStack::Size() const {
		return m_Data.size();
	}

	bool ValueStack::IsEmpty() const {
		return m_Data.empty();
	}
}