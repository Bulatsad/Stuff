#include <test/src/test.h>
#include <blib/pdl/pdl.h>

#include <string>
#include <vector>

namespace
{
	bool loadDemo(blib::pdl::Parser& p)
	{
		bool ok = p.load(std::string(TEST_SOURCE_DIR) + "/blib/pdl/demo.pdl");
		if (!ok)
			std::cerr << "demo.pdl load failed: " << p.getLastError() << std::endl;
		return ok;
	}
}

// ============================================================
// PDL parser
// ============================================================

BLIB_TEST_CASE("PDL: 'add 1 2'")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemo(p));
	BLIB_TEST_REQUIRE(p.parse("add 1 2"));

	std::string command;
	std::vector<std::string> options;
	std::vector<std::string> commandArgs;

	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_REQUIRE(p.getVar("@options", options));
	BLIB_TEST_REQUIRE(p.getVar("@commandArgs", commandArgs));

	BLIB_TEST_CHECK(command == "add");
	BLIB_TEST_CHECK(options.empty());
	BLIB_TEST_CHECK(commandArgs.size() == 2);
	BLIB_TEST_CHECK(commandArgs[0] == "1" && commandArgs[1] == "2");
}

BLIB_TEST_CASE("PDL: 'add signed true' fails")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemo(p));
	BLIB_TEST_CHECK(!p.parse("add signed true"));
	BLIB_TEST_CHECK(!p.getLastError().empty());
}

BLIB_TEST_CASE("PDL: 'sub signed 1 1 2'")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemo(p));
	BLIB_TEST_REQUIRE(p.parse("sub signed 1 1 2"));

	std::string command;
	std::vector<std::string> options;
	std::vector<std::string> commandArgs;

	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_REQUIRE(p.getVar("@options", options));
	BLIB_TEST_REQUIRE(p.getVar("@commandArgs", commandArgs));

	BLIB_TEST_CHECK(command == "sub");
	BLIB_TEST_CHECK(options.size() == 2);
	BLIB_TEST_CHECK(options[0] == "signed" && options[1] == "1");
	BLIB_TEST_CHECK(commandArgs.size() == 2);
	BLIB_TEST_CHECK(commandArgs[0] == "1" && commandArgs[1] == "2");
}
