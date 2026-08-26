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
	BLIB_TEST_REQUIRE(p.parse("exe get safe false"));

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
	BLIB_TEST_REQUIRE(p.parse("exe set varName varVal"));

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
	BLIB_TEST_REQUIRE(p.parse("exe set force yes varName varVal2"));

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
	BLIB_TEST_CHECK(!p.parse("exe set force maybe x"));
	BLIB_TEST_CHECK(!p.getLastError().empty());
}

BLIB_TEST_CASE("PDL demo1: 'get checkValue 2'")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));
	BLIB_TEST_REQUIRE(p.parse("exe get checkValue 2"));

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
	BLIB_TEST_REQUIRE(p.parse("exe get safe false |"));

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

	std::string text = "exe1 get safe true checkValue 1 varName varVal1 | exe2 set force yes varName varVal2";
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

BLIB_TEST_CASE("PDL demo1: __any captures one token")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));
	BLIB_TEST_REQUIRE(p.parse("exe one any hello"));

	std::string command;
	std::vector<std::string> options;
	std::vector<std::string> commandArgs;

	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_REQUIRE(p.getVar("@options", options));
	BLIB_TEST_REQUIRE(p.getVar("@commandArgs", commandArgs));

	BLIB_TEST_CHECK(command == "one");
	BLIB_TEST_CHECK(options.size() == 2);
	BLIB_TEST_CHECK(options[0] == "any" && options[1] == "hello");
	BLIB_TEST_CHECK(commandArgs.empty());
}

BLIB_TEST_CASE("PDL demo1: __any scalar binding")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));
	BLIB_TEST_REQUIRE(p.parse("exe get safe false"));

	std::string word;
	BLIB_TEST_REQUIRE(p.getVar("@word", word));
	BLIB_TEST_CHECK(word == "exe");
}

BLIB_TEST_CASE("PDL demo1: __any optional skip")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));
	BLIB_TEST_REQUIRE(p.parse("exe one skip"));

	std::string command;
	std::vector<std::string> options;
	std::vector<std::string> commandArgs;

	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_REQUIRE(p.getVar("@options", options));
	BLIB_TEST_REQUIRE(p.getVar("@commandArgs", commandArgs));

	BLIB_TEST_CHECK(command == "one");
	BLIB_TEST_CHECK(options.size() == 1);
	BLIB_TEST_CHECK(options[0] == "skip");
	BLIB_TEST_CHECK(commandArgs.empty());
}

BLIB_TEST_CASE("PDL demo1: __any stops at terminator in parseNext")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));

	std::string text = "exe1 one any hello | exe2 set force yes varName varVal2";
	size_t off = 0;

	std::string command;
	std::vector<std::string> options;
	std::vector<std::string> commandArgs;

	BLIB_TEST_REQUIRE(p.parseNext(text, off));

	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_REQUIRE(p.getVar("@options", options));
	BLIB_TEST_REQUIRE(p.getVar("@commandArgs", commandArgs));

	BLIB_TEST_CHECK(command == "one");
	BLIB_TEST_CHECK(options.size() == 2);
	BLIB_TEST_CHECK(options[0] == "any" && options[1] == "hello");
	BLIB_TEST_CHECK(commandArgs.empty());
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

BLIB_TEST_CASE("PDL demo1: __any fails on terminator and EOS")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));

	BLIB_TEST_CHECK(!p.parse("exe one any |"));
	BLIB_TEST_CHECK(p.getLastError().find("terminator '|'") != std::string::npos);

	BLIB_TEST_CHECK(!p.parse("exe one any"));
	BLIB_TEST_CHECK(p.getLastError().find("end of input") != std::string::npos);
}


BLIB_TEST_CASE("PDL demo2: __any")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 2));
	BLIB_TEST_REQUIRE(p.parse("exepath move forward 123 123 123"));

	std::string exepath;
	std::string command;
	std::vector<std::string> subCommandArgs;
	std::vector<std::string> commandArgs;

	BLIB_TEST_REQUIRE(p.getVar("@exepath", exepath));
	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_REQUIRE(p.getVar("@subCommandArgs", subCommandArgs));
	BLIB_TEST_REQUIRE(p.getVar("@commandArgs", commandArgs));

	BLIB_TEST_CHECK(exepath == "exepath");
	BLIB_TEST_CHECK(command == "move");
	BLIB_TEST_CHECK(subCommandArgs.size() == 3);
	BLIB_TEST_CHECK(subCommandArgs[0] == "123" && subCommandArgs[1] == "123" && subCommandArgs[2] == "123");
	BLIB_TEST_CHECK(commandArgs.size() == 4);
	BLIB_TEST_CHECK(commandArgs[0] == "forward" && commandArgs[1] == "123" && commandArgs[2] == "123" && commandArgs[3] == "123");
}

BLIB_TEST_CASE("PDL demo2: unknown command fails")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 2));

	BLIB_TEST_CHECK(!p.parse("exepath unknowncmd forward"));
	BLIB_TEST_CHECK(p.getLastError().find("'move', 'jump', 'stop'") != std::string::npos);
	BLIB_TEST_CHECK(p.getLastError().find("but got 'unknowncmd'") != std::string::npos);
	BLIB_TEST_CHECK(p.getLastError().find("alternative at '::GCommand' failed after consuming") != std::string::npos);

	BLIB_TEST_CHECK(!p.parse("exepath move forward 123 | 123"));
	BLIB_TEST_CHECK(p.getLastError().find("unexpected trailing input") != std::string::npos);
}

BLIB_TEST_CASE("PDL demo1: error message details")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));
	BLIB_TEST_CHECK(!p.parse("exe set force maybe x"));

	const std::string& err = p.getLastError();
	BLIB_TEST_CHECK(err.find("expected 'yes', 'no'") != std::string::npos);
	BLIB_TEST_CHECK(err.find("::CommandTemplate::<globalTemplate>::<options>::<flagValue>") != std::string::npos);
	BLIB_TEST_CHECK(err.find("but got 'maybe'") != std::string::npos);
	BLIB_TEST_CHECK(err.find("byte offset") != std::string::npos);
	BLIB_TEST_CHECK(err.find("line 1:") != std::string::npos);
	BLIB_TEST_CHECK(err.find("failed after consuming") != std::string::npos);
}

BLIB_TEST_CASE("PDL demo2: error message details")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 2));
	BLIB_TEST_CHECK(!p.parse("exepath stop"));

	const std::string& err = p.getLastError();
	BLIB_TEST_CHECK(err.find("no alternative matched at '::GCommand::<CommandArgs>'") != std::string::npos);
	BLIB_TEST_CHECK(err.find("guard '::GCommand::<Command> == \"jump\"'") != std::string::npos);
	BLIB_TEST_CHECK(err.find("captured 'stop'") != std::string::npos);
	BLIB_TEST_CHECK(err.find("alternative at '::GCommand' failed after consuming") != std::string::npos);
}

BLIB_TEST_CASE("PDL demo2: __none base case")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 2));
	BLIB_TEST_REQUIRE(p.parse("exepath jump forward"));

	std::string command;
	std::vector<std::string> options;
	std::vector<std::string> subCommandArgs;

	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_REQUIRE(p.getVar("@options", options));
	BLIB_TEST_REQUIRE(p.getVar("@subCommandArgs", subCommandArgs));

	BLIB_TEST_CHECK(command == "jump");
	BLIB_TEST_CHECK(options.empty());
	BLIB_TEST_CHECK(subCommandArgs.empty());
}

BLIB_TEST_CASE("PDL demo2: __none one option")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 2));
	BLIB_TEST_REQUIRE(p.parse("exepath jump notempfiles forward"));

	std::string command;
	std::vector<std::string> options;
	std::vector<std::string> subCommandArgs;

	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_REQUIRE(p.getVar("@options", options));
	BLIB_TEST_REQUIRE(p.getVar("@subCommandArgs", subCommandArgs));

	BLIB_TEST_CHECK(command == "jump");
	BLIB_TEST_CHECK(options.size() == 1);
	BLIB_TEST_CHECK(options[0] == "notempfiles");
	BLIB_TEST_CHECK(subCommandArgs.empty());
}

BLIB_TEST_CASE("PDL demo2: __none repeated options")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 2));
	BLIB_TEST_REQUIRE(p.parse("exepath jump notempfiles notempfiles forward"));

	std::string command;
	std::vector<std::string> options;
	std::vector<std::string> subCommandArgs;

	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_REQUIRE(p.getVar("@options", options));
	BLIB_TEST_REQUIRE(p.getVar("@subCommandArgs", subCommandArgs));

	BLIB_TEST_CHECK(command == "jump");
	BLIB_TEST_CHECK(options.size() == 2);
	BLIB_TEST_CHECK(options[0] == "notempfiles" && options[1] == "notempfiles");
	BLIB_TEST_CHECK(subCommandArgs.empty());
}

BLIB_TEST_CASE("PDL demo3: unfilled scalar binding doesn't fail parse")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 3));
	BLIB_TEST_REQUIRE(p.parse("run"));

	std::string s;
	BLIB_TEST_CHECK(!p.getVar("@opt", s));
	BLIB_TEST_CHECK(p.getLastError() == "variable '@opt' was not captured");
	BLIB_TEST_CHECK(!p.isCaptured("@opt"));
}

BLIB_TEST_CASE("PDL demo3: __none match is captured but empty")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 3));
	BLIB_TEST_REQUIRE(p.parse("run"));

	std::string s;
	BLIB_TEST_CHECK(!p.getVar("@maybe", s));
	BLIB_TEST_CHECK(p.getLastError() == "variable '@maybe' was not captured");
	BLIB_TEST_CHECK(p.isCaptured("@maybe"));
}

BLIB_TEST_CASE("PDL demo3: scalar bindings filled")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 3));
	BLIB_TEST_REQUIRE(p.parse("run verbose quiet"));

	std::string opt;
	std::string maybe;

	BLIB_TEST_REQUIRE(p.getVar("@opt", opt));
	BLIB_TEST_REQUIRE(p.getVar("@maybe", maybe));

	BLIB_TEST_CHECK(opt == "verbose");
	BLIB_TEST_CHECK(maybe == "quiet");
	BLIB_TEST_CHECK(p.isCaptured("@opt"));
	BLIB_TEST_CHECK(p.isCaptured("@maybe"));
}

BLIB_TEST_CASE("PDL demo3: isCaptured on undeclared variable")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 3));
	BLIB_TEST_REQUIRE(p.parse("run"));

	BLIB_TEST_CHECK(!p.isCaptured("@unknown"));
	BLIB_TEST_CHECK(p.getLastError() == "variable '@unknown' is not declared in the grammar");
}

BLIB_TEST_CASE("PDL getVar: undeclared variable sets error")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));
	BLIB_TEST_REQUIRE(p.parse("exe one any hello"));

	std::string s;
	BLIB_TEST_CHECK(!p.getVar("@unknown", s));
	BLIB_TEST_CHECK(p.getLastError() == "variable '@unknown' is not declared in the grammar");
}

BLIB_TEST_CASE("PDL getVar: wrong requested type sets error")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));
	BLIB_TEST_REQUIRE(p.parse("exe one any hello"));

	bint32 i = 0;
	BLIB_TEST_CHECK(!p.getVar("@command", i));
	BLIB_TEST_CHECK(p.getLastError() == "variable '@command' has type string, requested int");

	std::string s;
	BLIB_TEST_CHECK(!p.getVar("@options", s));
	BLIB_TEST_CHECK(p.getLastError() == "variable '@options' has type array<string>, requested string");
}

BLIB_TEST_CASE("PDL getVar: not captured variable sets error")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));

	std::string s;
	BLIB_TEST_CHECK(!p.getVar("@command", s));
	BLIB_TEST_CHECK(p.getLastError() == "variable '@command' was not captured");
}

BLIB_TEST_CASE("PDL getVar: success clears previous error")
{
	blib::pdl::Parser p;
	BLIB_TEST_REQUIRE(loadDemoNum(p, 1));
	BLIB_TEST_REQUIRE(p.parse("exe one any hello"));

	std::string command;
	std::vector<std::string> options;

	BLIB_TEST_REQUIRE(p.getVar("@command", command));
	BLIB_TEST_CHECK(p.getLastError() == "");

	BLIB_TEST_CHECK(!p.getVar("@unknown", command));
	BLIB_TEST_CHECK(p.getLastError() == "variable '@unknown' is not declared in the grammar");

	BLIB_TEST_REQUIRE(p.getVar("@options", options));
	BLIB_TEST_CHECK(p.getLastError() == "");
}
