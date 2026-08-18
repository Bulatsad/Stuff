#include <test/src/test.h>
#include <blib/pdl/pdl.h>

#include <string>
#include <vector>

namespace
{
	bool loadDemoNum(blib::pdl::Parser& p, size_t num)
	{
		std::string name = "demo";
		if (num > 0)
			name += std::to_string(num);
		name += ".pdl";

		std::string path = std::string(TEST_SOURCE_DIR) + "/blib/pdl/" + name;
		bool ok = p.load(path);
		if (!ok)
			std::cerr << path << " load failed: " << p.getLastError() << std::endl;
		return ok;
	}
}

// ============================================================
// PDL parser
// ============================================================

BLIB_TEST_CASE("PDL: 'add 1 2'")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 0));
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
	BLIB_TEST_REQUIRE(loadDemoNum(p, 0));
	BLIB_TEST_CHECK(!p.parse("add signed true"));
	BLIB_TEST_CHECK(!p.getLastError().empty());
}

BLIB_TEST_CASE("PDL: 'sub signed 1 1 2'")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 0));
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

// ============================================================
// PDL demo1: guards, blocks, explicit terminator, parseNext
// ============================================================

BLIB_TEST_CASE("PDL demo1: 'get safe false'")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));
	BLIB_TEST_REQUIRE(p.parse("get safe false"));

	std::string command;
	std::vector<std::string> options;
	std::vector<std::string> commandArgs;

	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_REQUIRE(p.getVar("@options", options));
	BLIB_TEST_REQUIRE(p.getVar("@commandArgs", commandArgs));

	BLIB_TEST_CHECK(command == "get");
	BLIB_TEST_CHECK(options.size() == 2);
	BLIB_TEST_CHECK(options[0] == "safe" && options[1] == "false");
	BLIB_TEST_CHECK(commandArgs.empty());
}

BLIB_TEST_CASE("PDL demo1: 'set varName varVal'")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));
	BLIB_TEST_REQUIRE(p.parse("set varName varVal"));

	std::string command;
	std::vector<std::string> options;
	std::vector<std::string> commandArgs;

	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_REQUIRE(p.getVar("@options", options));
	BLIB_TEST_REQUIRE(p.getVar("@commandArgs", commandArgs));

	BLIB_TEST_CHECK(command == "set");
	BLIB_TEST_CHECK(options.empty());
	BLIB_TEST_CHECK(commandArgs.size() == 2);
	BLIB_TEST_CHECK(commandArgs[0] == "varName" && commandArgs[1] == "varVal");
}

BLIB_TEST_CASE("PDL demo1: 'set force yes varName varVal2'")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));
	BLIB_TEST_REQUIRE(p.parse("set force yes varName varVal2"));

	std::string command;
	std::vector<std::string> options;
	std::vector<std::string> commandArgs;

	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_REQUIRE(p.getVar("@options", options));
	BLIB_TEST_REQUIRE(p.getVar("@commandArgs", commandArgs));

	BLIB_TEST_CHECK(command == "set");
	BLIB_TEST_CHECK(options.size() == 2);
	BLIB_TEST_CHECK(options[0] == "force" && options[1] == "yes");
	BLIB_TEST_CHECK(commandArgs.size() == 2);
	BLIB_TEST_CHECK(commandArgs[0] == "varName" && commandArgs[1] == "varVal2");
}

BLIB_TEST_CASE("PDL demo1: 'set force maybe x' fails")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));
	BLIB_TEST_CHECK(!p.parse("set force maybe x"));
	BLIB_TEST_CHECK(!p.getLastError().empty());
}

BLIB_TEST_CASE("PDL demo1: 'get checkValue 2'")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));
	BLIB_TEST_REQUIRE(p.parse("get checkValue 2"));

	std::string command;
	std::vector<std::string> options;
	std::vector<std::string> commandArgs;

	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_REQUIRE(p.getVar("@options", options));
	BLIB_TEST_REQUIRE(p.getVar("@commandArgs", commandArgs));

	BLIB_TEST_CHECK(command == "get");
	BLIB_TEST_CHECK(options.size() == 2);
	BLIB_TEST_CHECK(options[0] == "checkValue" && options[1] == "2");
	BLIB_TEST_CHECK(commandArgs.empty());
}

BLIB_TEST_CASE("PDL demo1: trailing terminator via parse")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));
	BLIB_TEST_REQUIRE(p.parse("get safe false |"));

	std::string command;
	std::vector<std::string> options;
	std::vector<std::string> commandArgs;

	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_REQUIRE(p.getVar("@options", options));
	BLIB_TEST_REQUIRE(p.getVar("@commandArgs", commandArgs));

	BLIB_TEST_CHECK(command == "get");
	BLIB_TEST_CHECK(options.size() == 2);
	BLIB_TEST_CHECK(options[0] == "safe" && options[1] == "false");
	BLIB_TEST_CHECK(commandArgs.empty());
}

BLIB_TEST_CASE("PDL demo1: two commands via parseNext")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));

	std::string text = "get safe true checkValue 1 varName varVal1 | set force yes varName varVal2";
	size_t off = 0;

	std::string command;
	std::vector<std::string> options;
	std::vector<std::string> commandArgs;

	BLIB_TEST_REQUIRE(p.parseNext(text, off));

	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_REQUIRE(p.getVar("@options", options));
	BLIB_TEST_REQUIRE(p.getVar("@commandArgs", commandArgs));

	BLIB_TEST_CHECK(command == "get");
	BLIB_TEST_CHECK(options.size() == 2);
	BLIB_TEST_CHECK(options[0] == "safe" && options[1] == "true");
	BLIB_TEST_CHECK(commandArgs.size() == 4);
	BLIB_TEST_CHECK(commandArgs[0] == "checkValue" && commandArgs[1] == "1");
	BLIB_TEST_CHECK(commandArgs[2] == "varName" && commandArgs[3] == "varVal1");
	BLIB_TEST_CHECK(off < text.size());

	BLIB_TEST_REQUIRE(p.parseNext(text, off));

	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_REQUIRE(p.getVar("@options", options));
	BLIB_TEST_REQUIRE(p.getVar("@commandArgs", commandArgs));

	BLIB_TEST_CHECK(command == "set");
	BLIB_TEST_CHECK(options.size() == 2);
	BLIB_TEST_CHECK(options[0] == "force" && options[1] == "yes");
	BLIB_TEST_CHECK(commandArgs.size() == 2);
	BLIB_TEST_CHECK(commandArgs[0] == "varName" && commandArgs[1] == "varVal2");
	BLIB_TEST_CHECK(off == text.size());

	BLIB_TEST_CHECK(!p.parseNext(text, off));
	BLIB_TEST_CHECK(p.getLastError().empty());
}
