#pragma once

#include "Core.h"
#include "Exception/ArCalcException.h"

namespace ArCalc {
	class ValueStack {
	public:
		struct Entry {
			constexpr static Entry MakeRValue(double value) {
				auto res     = Entry{};
				res.bLValue  = false; 
				res.Value    = value;
				res.Ptr      = {};
				return res;
			}

			constexpr static Entry MakeLValue(double* ptr) {
				auto res     = Entry{};
				res.bLValue  = true; 
				res.Value    = {};
				res.Ptr      = ptr;
				return res;
			}

			constexpr double Deref() const { 
				return bLValue ? *Ptr : Value;
			}

			constexpr void SetValue(double toWhat) {
				if (bLValue) { *Ptr  = toWhat; } 
				else         { Value = toWhat; }
			}

			bool bLValue{}; // Union flag.
			double Value{}; // RValue.
			double* Ptr{};  // LValue.
		};

	public:
		ValueStack() = default;

	public:
		constexpr void PushRValue(double newValue) { 
			m_Data.push_back(Entry::MakeRValue(newValue)); 
		}

		constexpr void PushLValue(double* ptr) { 
			m_Data.push_back(Entry::MakeLValue(ptr)); 
		}

		constexpr Entry Pop() {
			ARCALC_DA(!m_Data.empty(), "Popped empty ValueStack");
			auto const value{m_Data.back()};
			m_Data.pop_back();
			return value;
		}

		constexpr Entry Top() const {
			return const_cast<ValueStack&>(*this).Top();
		}

		constexpr Entry& Top() {
			ARCALC_DA(!m_Data.empty(), "Tried to get top from empty ValueStack");
			return m_Data.back();
		}

		constexpr size_t Size()  const { return m_Data.size(); }
		constexpr bool IsEmpty() const { return m_Data.empty(); }
		constexpr void Clear()         { m_Data.clear(); }

	private:
		std::vector<Entry> m_Data{};
	};
}