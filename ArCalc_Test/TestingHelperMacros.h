#pragma once

#define CHECK_ASSERT_NO_THROW(_expr) \
	try { _expr; }                                   \
	catch(ArCalcException& err) {                    \
		std::cout << err.GetMessage() << '\n';       \
		__debugbreak();                              \
	} catch (std::exception& err) {                  \
		std::cout << err.what() << '\n';             \
		__debugbreak();                              \
	}                                                \
	std::cout