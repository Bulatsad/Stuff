#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <exception>

namespace blib
{
	namespace test
	{
		struct TestCase
		{
			const char*	name;
			void		(*func)();
			const char*	file;
			int			line;
			const char*	knownFailureReason; // != nullptr, если падение теста ожидаемо
		};

		struct Failure
		{
			const char*	file;
			int			line;
			const char*	expr;
		};

		inline std::vector<TestCase>& getRegistry()
		{
			static std::vector<TestCase> registry;
			return registry;
		}

		inline int registerTestCase(const char* name, void (*func)(), const char* file, int line)
		{
			TestCase tc;
			tc.name = name;
			tc.func = func;
			tc.file = file;
			tc.line = line;
			tc.knownFailureReason = nullptr;
			getRegistry().push_back(tc);
			return 0;
		}

		// Текущий исполняемый тестовый кейс. Выставляется main'ом
		// перед запуском каждого теста; нужен макросу
		// BLIB_TEST_KNOWN_FAILURE, чтобы пометить кейс изнутри.
		inline TestCase*& getCurrentTestCase()
		{
			static TestCase* current = nullptr;
			return current;
		}

		inline void markCurrentTestKnownFailure(const char* reason)
		{
			if (getCurrentTestCase())
				getCurrentTestCase()->knownFailureReason = reason;
		}

		inline std::vector<Failure>& getFailures()
		{
			static std::vector<Failure> failures;
			return failures;
		}

		inline void addFailure(const char* file, int line, const char* expr)
		{
			Failure f;
			f.file = file;
			f.line = line;
			f.expr = expr;
			getFailures().push_back(f);
		}
	}
}

#define BLIB_TEST_CAT_IMPL(a, b) a##b
#define BLIB_TEST_CAT(a, b) BLIB_TEST_CAT_IMPL(a, b)

#define BLIB_TEST_CASE(name) \
	static void BLIB_TEST_CAT(__blib_test_func_, __LINE__)(); \
	namespace \
	{ \
		struct BLIB_TEST_CAT(__blib_test_reg_, __LINE__) \
		{ \
			BLIB_TEST_CAT(__blib_test_reg_, __LINE__)() \
			{ \
				blib::test::registerTestCase(name, &BLIB_TEST_CAT(__blib_test_func_, __LINE__), __FILE__, __LINE__); \
			} \
		} BLIB_TEST_CAT(__blib_test_reg_inst_, __LINE__); \
	} \
	static void BLIB_TEST_CAT(__blib_test_func_, __LINE__)()

#define BLIB_TEST_CHECK(expr) \
	do \
	{ \
		if (!(expr)) \
		{ \
			blib::test::addFailure(__FILE__, __LINE__, #expr); \
			std::cerr << "  FAIL [" << __FILE__ << ":" << __LINE__ << "] " << #expr << std::endl; \
		} \
	} while(0)

#define BLIB_TEST_REQUIRE(expr) \
	do \
	{ \
		if (!(expr)) \
		{ \
			blib::test::addFailure(__FILE__, __LINE__, #expr); \
			std::cerr << "  FAIL [" << __FILE__ << ":" << __LINE__ << "] " << #expr << std::endl; \
			return; \
		} \
	} while(0)

// Пометить текущий тест как "известный падающий" (XFAIL).
// Тест продолжает исполняться и падать как обычно, но итоговый прогон
// учитывает это падение как ожидаемое и не валит весь набор.
// Использование: тесты-документаторы незакрытых багов (см. комментарий
// внутри теста), чтобы сборка оставалась зелёной, а баг — видимым.
#define BLIB_TEST_KNOWN_FAILURE(reason) \
	do \
	{ \
		blib::test::markCurrentTestKnownFailure(reason); \
	} while(0)

#define BLIB_TEST_CHECK_CLOSE(a, b, eps) \
	do \
	{ \
		auto __blib_a = (a); \
		auto __blib_b = (b); \
		auto __blib_e = (eps); \
		if (std::abs(__blib_a - __blib_b) > __blib_e) \
		{ \
			blib::test::addFailure(__FILE__, __LINE__, #a " ~= " #b); \
			std::cerr << "  FAIL [" << __FILE__ << ":" << __LINE__ << "] " \
				<< #a " (" << __blib_a << ") != " #b " (" << __blib_b << ")" << std::endl; \
		} \
	} while(0)

#define BLIB_TEST_REQUIRE_THROWS(expr, ex_type) \
	do \
	{ \
		bool __blib_caught = false; \
		try { expr; } \
		catch (const ex_type&) { __blib_caught = true; } \
		if (!__blib_caught) \
		{ \
			blib::test::addFailure(__FILE__, __LINE__, #expr " did not throw " #ex_type); \
			std::cerr << "  FAIL [" << __FILE__ << ":" << __LINE__ << "] expected throw of " << #ex_type << std::endl; \
			return; \
		} \
	} while(0)

#define BLIB_TEST_REQUIRE_NOTHROW(expr) \
	do \
	{ \
		try { expr; } \
		catch (const std::exception& __blib_ex) \
		{ \
			blib::test::addFailure(__FILE__, __LINE__, #expr " threw: " __blib_ex.what()); \
			std::cerr << "  FAIL [" << __FILE__ << ":" << __LINE__ << "] unexpected throw: " << __blib_ex.what() << std::endl; \
			return; \
		} \
		catch (...) \
		{ \
			blib::test::addFailure(__FILE__, __LINE__, #expr " threw unknown exception"); \
			std::cerr << "  FAIL [" << __FILE__ << ":" << __LINE__ << "] unexpected throw" << std::endl; \
			return; \
		} \
	} while(0)

#define BLIB_TEST_MAIN \
	int main() \
	{ \
		auto& registry = blib::test::getRegistry(); \
		std::cout << "Running " << registry.size() << " test(s)..." << std::endl; \
		int passed = 0; \
		int knownFailed = 0; \
		for (size_t i = 0; i < registry.size(); ++i) \
		{ \
			auto& test = registry[i]; \
			blib::test::getFailures().clear(); \
			test.knownFailureReason = nullptr; \
			blib::test::getCurrentTestCase() = &test; \
			std::cout << "[" << (i + 1) << "/" << registry.size() << "] " << test.name << " ... "; \
			try \
			{ \
				test.func(); \
			} \
			catch (const std::exception& __blib_ex) \
			{ \
				blib::test::addFailure(test.file, test.line, __blib_ex.what()); \
				std::cerr << "EXCEPTION: " << __blib_ex.what() << std::endl; \
			} \
			catch (...) \
			{ \
				blib::test::addFailure(test.file, test.line, "unknown exception"); \
				std::cerr << "EXCEPTION: unknown" << std::endl; \
			} \
			blib::test::getCurrentTestCase() = nullptr; \
			if (blib::test::getFailures().empty()) \
			{ \
				std::cout << "PASSED" << std::endl; \
				++passed; \
			} \
			else if (test.knownFailureReason) \
			{ \
				std::cout << "FAILED (KNOWN: " << test.knownFailureReason << ")" << std::endl; \
				++knownFailed; \
			} \
			else \
			{ \
				std::cout << "FAILED (" << blib::test::getFailures().size() << " check(s))" << std::endl; \
			} \
		} \
		std::cout << "\nResults: " << passed << "/" << registry.size() << " passed"; \
		if (knownFailed > 0) \
			std::cout << " (" << knownFailed << " known failure(s))"; \
		std::cout << std::endl; \
		return (passed + knownFailed == (int)registry.size()) ? 0 : 1; \
	}
