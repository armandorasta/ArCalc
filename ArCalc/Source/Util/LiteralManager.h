#pragma once

#include "Core.h"

namespace ArCalc {
	class LiteralData {
	private:
		LiteralData() = default;

	public:
		LiteralData(LiteralData const&)            = default;
		LiteralData(LiteralData&&)                 = default;
		LiteralData& operator=(LiteralData const&) = default;
		LiteralData& operator=(LiteralData&&)      = default;

	public:
		// Makes a normal literal or a by-value parameter.
		constexpr static LiteralData Make(double what) {
			auto res = LiteralData{};
			res.m_Value = what;
			return res;
		}

		// Makes a by-reference parameter.
		constexpr static LiteralData MakeRef(double* toWhat) {
			auto res = LiteralData{};
			res.m_Ptr = toWhat;
			return res;
		}

	public:
		constexpr double* operator&() // Takes care of both cases.
			{ return m_Ptr ? m_Ptr : &m_Value; }
		constexpr double const* operator&() const
			{ return const_cast<LiteralData&>(*this).operator&(); }
		
		constexpr double& operator*() // Takes care of both cases.
			{ return m_Ptr ? *m_Ptr : m_Value; }
		constexpr double const& operator*() const
			{ return const_cast<LiteralData&>(*this).operator*(); }
		
		constexpr double* operator->() // Takes care of both cases.
			{ return operator&(); }
		constexpr double const* operator->() const
			{ return const_cast<LiteralData&>(*this).operator&(); }

	private:
		double m_Value;
		double* m_Ptr;
	};

	class LiteralManager {
	private:
		using LiteralMap = std::unordered_map<std::string, LiteralData>;

	public:
		LiteralManager(LiteralManager const&)            = default;
		LiteralManager(LiteralManager&&)                 = default;
		LiteralManager& operator=(LiteralManager const&) = default;
		LiteralManager& operator=(LiteralManager&&)      = default;

		LiteralManager(std::ostream& os);

		void Add(std::string_view litName, double value);
		void Add(std::string_view litName, double* ptr);
		void Delete(std::string_view litName);

		// Sets the value of an existing literal
		void SetLast(double toWhat);

		void List(std::string_view perfix = {}) const;
		double GetLast() const;
		LiteralData const& Get(std::string_view litName) const;
		LiteralData& Get(std::string_view litName);
		bool IsVisible(std::string_view litName) const;

		void Serialize(std::string_view name, std::ostream& os);
		void Deserialize(std::istream& is);

		constexpr void ToggleOutput()          { m_bSuppressOutput ^= 1; }
		constexpr bool IsOutputEnabled() const { return !m_bSuppressOutput; }

		void Reset();

	private:
		bool m_bSuppressOutput{};
		std::ostream& m_OStream;
		LiteralMap m_LitMap{};
	};
}