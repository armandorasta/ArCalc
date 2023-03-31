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
			// TODO: fix line number (start at 1, and increment after statement).
			IO::PrintStd("{:0>3} >> ", m_pParser->GetLineNumber() + 1);

			if (m_pParser->IsParsingFunction()) {
				// Indent the code inside the defination.
				IO::PrintStd("    ");
				if (m_pParser->IsParsingSelectionStatement()) {
					// Indent furthur for selection statements.
					IO::PrintStd("    ");
				}
			}

			try { 
				m_pParser->ParseLine(IO::GetLineStd()); 
				continue;
			} catch (SyntaxError const& err) {
				IO::Print(std::cerr, "Syntax Error {}\n", err.GetMessage());
			} catch (MathError const& err) {
				IO::Print(std::cerr, "Math Error {}.\n", err.GetMessage());
			} catch (ExprEvalError const& err) {
				/* Where is it thrown?
				 * 1) Normal expressions.
				 * 2) _Set [here].
				 * 3) [_If or _Elif] [here].
				 */
				IO::Print(std::cerr, "Expression Evaluation Error {}.\n", err.GetMessage());
			} catch (ParseError const& err) {
				/* Where is it thrown?
				 * 1) Invalid identifiers.
				 * 2) Too many tokens in a line.
				 * 3) Insufficient amount of tokens.
				 * 4) Invalid number syntax.
				 */
				IO::Print(std::cerr, "Parsing Error {}.\n", err.GetMessage());
			} 
#ifndef NDEBUG
			catch (ArCalcException const& err) {
				IO::Print(std::cerr, "Debug assertion failed {}.\n", err.GetMessage());
			}
#endif // ^^^^ Debug mode only.
			catch (std::exception const& err) {
				IO::Print(std::cerr, "Caught a std::exception {}.\n", err.what());
				std::terminate();
			} 

			m_pParser->ExceptionReset();
		}
	}
}
