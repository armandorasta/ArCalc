#include "App.h"
#include "PostfixMathEvaluator.h"
#include "Util/Util.h"
#include "Util/MathOperator.h"
#include "Util/IO.h"

namespace ArCalc {
	App::App() : m_pParser{std::make_unique<Parser>(std::cout)} {
	}

	void App::Run() {
		for (;;) {
			constexpr auto Tab{"...."};
			auto const indentation = [&, this] {
				auto res = std::string{};
				// Indent the code inside the defination.
				if (m_pParser->IsParsingFunction()) { 
					res.append(Tab);
				}
				// Indent furthur for selection statements.
				if (m_pParser->IsParsingSelectionStatement()) { 
					res.append(Tab);
				} 

				return res;
			}(/*)(*/);

			constexpr auto errorPrefix{"ERROR: "};

			// TODO: fix line number (start at 1, and increment after statement).
			IO::PrintStd("{:0>3} >> ", m_pParser->GetLineNumber() + 1);
			IO::OutputStd(indentation);

			try { 
				m_pParser->ParseLine(IO::GetLineStd());
				continue;
			} 
#ifndef NDEBUG
			catch (DebugAssertionError const& err) {
				err.PrintMessage(errorPrefix);
			}
#endif // ^^^^ Debug mode only.
			catch (ArCalcException& err) {
				if (err.GetLineNumber() == 0) {
					// Exceptions that get thrown one level deep do not encounter 
					// a try-catch block in the way, which means nothing sets their 
					// line number to anything.
					err.SetLineNumber(m_pParser->GetLineNumber());
				}

				err.PrintMessage(errorPrefix);
			} catch (std::exception const& err) {
				IO::Print(std::cerr, "Caught a std::exception {}.\n", err.what());
				std::terminate();
			}
			m_pParser->ExceptionReset();
		}
	}
}
