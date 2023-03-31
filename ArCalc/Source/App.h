#pragma once

#include "Core.h"
#include "Util/Util.h"
#include "Parser.h"

namespace ArCalc {
	class App {
	public:
		App();

	public:
		void Run();

	private:
		std::unique_ptr<Parser> m_pParser{};
	};
}