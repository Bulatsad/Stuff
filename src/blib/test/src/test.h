#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <exception>
#include <stdexcept>

#include <blib/core/console/console.h>

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
			__blib_log_error("  FAIL [%s:%d] %s", __FILE__, __LINE__, #expr); \
		} \
	} while(0)

#define BLIB_TEST_REQUIRE(expr) \
	do \
	{ \
		if (!(expr)) \
		{ \
			blib::test::addFailure(__FILE__, __LINE__, #expr); \
			__blib_log_error("  FAIL [%s:%d] %s", __FILE__, __LINE__, #expr); \
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
			__blib_log_error("  FAIL [%s:%d] %s (%s) != %s (%s)", \
				__FILE__, __LINE__, #a, std::to_string(__blib_a).c_str(), \
				#b, std::to_string(__blib_b).c_str()); \
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
			__blib_log_error("  FAIL [%s:%d] expected throw of %s", __FILE__, __LINE__, #ex_type); \
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
			__blib_log_error("  FAIL [%s:%d] unexpected throw: %s", __FILE__, __LINE__, __blib_ex.what()); \
			return; \
		} \
		catch (...) \
		{ \
			blib::test::addFailure(__FILE__, __LINE__, #expr " threw unknown exception"); \
			__blib_log_error("  FAIL [%s:%d] unexpected throw", __FILE__, __LINE__); \
			return; \
		} \
	} while(0)

#define BLIB_TEST_MAIN \
	int main() \
	{ \
		/* Тесты — CLI: включаем stdout-эхо консоли, чтобы вывод был виден в терминале */ \
		blib::console::Console::instance().getOutput().setStdoutEcho(true); \
		auto& registry = blib::test::getRegistry(); \
		__blib_log_info("Running %zu test(s)...", registry.size()); \
		int passed = 0; \
		int knownFailed = 0; \
		for (size_t i = 0; i < registry.size(); ++i) \
		{ \
			auto& test = registry[i]; \
			blib::test::getFailures().clear(); \
			test.knownFailureReason = nullptr; \
			blib::test::getCurrentTestCase() = &test; \
			__blib_log_info("[%zu/%zu] %s ...", i + 1, registry.size(), test.name); \
			try \
			{ \
				test.func(); \
			} \
			catch (const std::exception& __blib_ex) \
			{ \
				blib::test::addFailure(test.file, test.line, __blib_ex.what()); \
				__blib_log_error("EXCEPTION: %s", __blib_ex.what()); \
			} \
			catch (...) \
			{ \
				blib::test::addFailure(test.file, test.line, "unknown exception"); \
				__blib_log_error("EXCEPTION: unknown"); \
			} \
			blib::test::getCurrentTestCase() = nullptr; \
			if (blib::test::getFailures().empty()) \
			{ \
				__blib_log_info("PASSED"); \
				++passed; \
			} \
			else if (test.knownFailureReason) \
			{ \
				__blib_log_warning("FAILED (KNOWN: %s)", test.knownFailureReason); \
				++knownFailed; \
			} \
			else \
			{ \
				__blib_log_error("FAILED (%zu check(s))", blib::test::getFailures().size()); \
			} \
		} \
		if (knownFailed > 0) \
			__blib_log_info("Results: %d/%zu passed (%d known failure(s))", passed, registry.size(), knownFailed); \
		else \
			__blib_log_info("Results: %d/%zu passed", passed, registry.size()); \
		return (passed + knownFailed == (int)registry.size()) ? 0 : 1; \
	}
