#include <blib/test/src/test.h>

// Потоки ядра: интерфейсы, type-erased обёртки, конкретные реализации
#include <blib/core/istream.h>
#include <blib/core/ostream.h>
#include <blib/core/memoryStream.h>
#include <blib/core/sliceStream.h>
#include <blib/core/stdStreamAdapter.h>
#include <blib/core/fileStream.h>
#include <blib/system/memory/globalAllocator.h>

#include <string>
#include <sstream>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
	#include <windows.h>
#endif

using namespace blib::core;

// ============================================================
// 1. Probe-реализации: контрактные тесты интерфейса
// ============================================================

namespace
{
	/**
	 * ProbeInputStream - независимая ручная реализация IInputStream.
	 * 
	 * Контрактные тесты на ней проверяют, что type-erased InputStream
	 * (и всё, что строится поверх интерфейса) работает ТОЛЬКО через
	 * интерфейсные методы - с любой сторонней реализацией, а не завязан
	 * на конкретные классы ядра.
	 */
	class ProbeInputStream : public IInputStream
	{
	public:
		int readCalls;
		int seekCalls;

		explicit ProbeInputStream(const std::string& text)
			: data(text)
			, pos(0)
			, readCalls(0)
			, seekCalls(0)
		{
		}

		size_t read(_Out void* buffer, size_t size) __blib_override
		{
			++this->readCalls;
			if (!buffer || !size)
				return 0;

			size_t available = this->data.size() - this->pos;
			size_t n = size < available ? size : available;
			if (n)
				std::memcpy(buffer, this->data.c_str() + this->pos, n);
			this->pos += n;
			return n;
		}

		bool canSeek() const __blib_override { return true; }

		bool seek(bint64 offset, SeekOrigin origin) __blib_override
		{
			++this->seekCalls;

			bint64 base;
			switch (origin)
			{
				case SeekOrigin::Begin:    base = 0; break;
				case SeekOrigin::Current:  base = static_cast<bint64>(this->pos); break;
				case SeekOrigin::End:      base = static_cast<bint64>(this->data.size()); break;
				default: return false;
			}

			bint64 newPos = base + offset;
			if (newPos < 0 || newPos > static_cast<bint64>(this->data.size()))
				return false;

			this->pos = static_cast<size_t>(newPos);
			return true;
		}

		buint64 tell() const __blib_override { return static_cast<buint64>(this->pos); }
		buint64 size() const __blib_override { return static_cast<buint64>(this->data.size()); }

	private:
		std::string data;
		size_t pos;
	};

	/**
	 * ProbeOutputStream - независимая ручная реализация IOutputStream.
	 * Собирает всё записанное в строку для проверок.
	 */
	class ProbeOutputStream : public IOutputStream
	{
	public:
		int writeCalls;

		ProbeOutputStream()
			: writeCalls(0)
		{
		}

		size_t write(_In const void* data, size_t size) __blib_override
		{
			++this->writeCalls;
			if (!data || !size)
				return 0;

			this->received.append(static_cast<const char*>(data), size);
			return size;
		}

		bool canSeek() const __blib_override { return false; }
		bool seek(bint64 offset, SeekOrigin origin) __blib_override { (void)offset; (void)origin; return false; }
		buint64 tell() const __blib_override { return 0; }
		buint64 size() const __blib_override { return 0; }

		const std::string& getReceived() const { return this->received; }

	private:
		std::string received;
	};
}

BLIB_TEST_CASE("Interface contract: InputStream works with any IInputStream impl")
{
	ProbeInputStream probe("abcdef");

	// Владение: lvalue -> копия probe (независимая позиция)
	InputStream in(probe);
	char buf[8];
	BLIB_TEST_CHECK(in.read(buf, 4) == 4);
	BLIB_TEST_CHECK(std::memcmp(buf, "abcd", 4) == 0);
	BLIB_TEST_CHECK(probe.tell() == 0); // копия изолирована от исходника

	// Счётчик вызовов растёт только у копии внутри обёртки
	BLIB_TEST_CHECK(probe.readCalls == 0);

	// seek/tell/size пробрасываются через интерфейс
	BLIB_TEST_CHECK(in.canSeek());
	BLIB_TEST_CHECK(in.seek(0, SeekOrigin::Begin));
	BLIB_TEST_CHECK(in.size() == 6);
	BLIB_TEST_CHECK(in.tell() == 0);
}

BLIB_TEST_CASE("Interface contract: borrow shares state with external impl")
{
	ProbeInputStream probe("1234567");
	{
		InputStream borrowed = InputStream::borrow(probe);
		char buf[4];
		BLIB_TEST_CHECK(borrowed.read(buf, 2) == 2);
		BLIB_TEST_CHECK(std::memcmp(buf, "12", 2) == 0);
	}
	// Позиция изменилась в самом probe (общая)
	BLIB_TEST_CHECK(probe.tell() == 2);
}

BLIB_TEST_CASE("Interface contract: OutputStream works with any IOutputStream impl")
{
	ProbeOutputStream probe;

	OutputStream out(probe); // владеющая копия
	BLIB_TEST_CHECK(out.write("hello", 5) == 5);
	BLIB_TEST_CHECK(probe.getReceived().empty()); // копия изолирована
	BLIB_TEST_CHECK(!out.canSeek());

	// borrow пишет в сам probe
	OutputStream borrowed = OutputStream::borrow(probe);
	BLIB_TEST_CHECK(borrowed.write("xy", 2) == 2);
	BLIB_TEST_CHECK(probe.getReceived() == "xy");
	BLIB_TEST_CHECK(probe.writeCalls == 1);
}

// ============================================================
// 2. MemoryStream
// ============================================================

BLIB_TEST_CASE("MemoryStream: write grows buffer, read consumes")
{
	MemoryStream s;

	BLIB_TEST_CHECK(s.canSeek());
	BLIB_TEST_CHECK(s.size() == 0);
	BLIB_TEST_CHECK(s.tell() == 0);

	BLIB_TEST_CHECK(s.write("hello world", 11) == 11);
	BLIB_TEST_CHECK(s.size() == 11);
	BLIB_TEST_CHECK(s.tell() == 11);

	// Чтение с конца - EOF
	char buf[16];
	BLIB_TEST_CHECK(s.read(buf, sizeof(buf)) == 0);

	// seek в начало, чтение по частям
	BLIB_TEST_CHECK(s.seek(0, SeekOrigin::Begin));
	BLIB_TEST_CHECK(s.read(buf, 5) == 5);
	BLIB_TEST_CHECK(std::memcmp(buf, "hello", 5) == 0);
	BLIB_TEST_CHECK(s.tell() == 5);
}

BLIB_TEST_CASE("MemoryStream: seek origins and strictness")
{
	MemoryStream s;
	s.write("0123456789", 10);

	// От текущей позиции
	BLIB_TEST_CHECK(s.seek(-5, SeekOrigin::Current));
	BLIB_TEST_CHECK(s.read(nullptr, 0) == 0); // null/0 - no-op
	char buf[4];
	BLIB_TEST_CHECK(s.read(buf, 4) == 4);
	BLIB_TEST_CHECK(std::memcmp(buf, "5678", 4) == 0);

	// От конца (назад)
	BLIB_TEST_CHECK(s.seek(-3, SeekOrigin::End));
	BLIB_TEST_CHECK(s.tell() == 7);

	// Строгость: выход за пределы - false, позиция не меняется
	BLIB_TEST_CHECK(!s.seek(100, SeekOrigin::Begin));
	BLIB_TEST_CHECK(!s.seek(-100, SeekOrigin::Current));
	BLIB_TEST_CHECK(s.tell() == 7);
}

BLIB_TEST_CASE("MemoryStream: overwrite does not grow, write past end does")
{
	MemoryStream s;
	s.write("hello", 5);

	// Перезапись поверх
	BLIB_TEST_CHECK(s.seek(1, SeekOrigin::Begin));
	BLIB_TEST_CHECK(s.write("ELLO", 4) == 4);
	BLIB_TEST_CHECK(s.size() == 5);

	// Запись за конец - буфер растёт
	BLIB_TEST_CHECK(s.seek(0, SeekOrigin::End));
	BLIB_TEST_CHECK(s.write("!!", 2) == 2);
	BLIB_TEST_CHECK(s.size() == 7);

	s.seek(0, SeekOrigin::Begin);
	char buf[8];
	BLIB_TEST_CHECK(s.read(buf, 7) == 7);
	BLIB_TEST_CHECK(std::memcmp(buf, "hELLO!!", 7) == 0);
}

BLIB_TEST_CASE("MemoryStream: constructors, getData, release, clear")
{
	// Копия буфера
	blib::core::ByteArray src;
	src.push_back(1);
	src.push_back(2);
	src.push_back(3);

	MemoryStream copyCtor(src);
	BLIB_TEST_CHECK(copyCtor.size() == 3);
	BLIB_TEST_CHECK(src.size() == 3); // исходник не тронут

	// Перенос буфера
	MemoryStream moveCtor(std::move(src));
	BLIB_TEST_CHECK(moveCtor.size() == 3);
	BLIB_TEST_CHECK(src.size() == 0);

	// getData
	const blib::core::ByteArray& d = moveCtor.getData();
	BLIB_TEST_CHECK(d.size() == 3 && d[0] == 1 && d[2] == 3);

	// release - буфер забирается, поток пуст
	blib::core::ByteArray taken = moveCtor.release();
	BLIB_TEST_CHECK(taken.size() == 3);
	BLIB_TEST_CHECK(moveCtor.size() == 0);
	BLIB_TEST_CHECK(moveCtor.tell() == 0);

	// clear
	moveCtor.write("abc", 3);
	moveCtor.clear();
	BLIB_TEST_CHECK(moveCtor.size() == 0);
}

BLIB_TEST_CASE("MemoryStream: copy and move of the stream itself")
{
	MemoryStream a;
	a.write("data", 4);

	MemoryStream b(a); // копия: свой буфер
	BLIB_TEST_CHECK(b.size() == 4);
	b.seek(0, SeekOrigin::Begin);
	b.write("D", 1);
	BLIB_TEST_CHECK(a.size() == 4); // a не изменился

	MemoryStream c(std::move(a));
	BLIB_TEST_CHECK(c.size() == 4);
	BLIB_TEST_CHECK(a.size() == 0); // буфер переехал
}

// ============================================================
// 3. SliceStream
// ============================================================

BLIB_TEST_CASE("SliceStream: window over memory, clamp and strict seek")
{
	const char* big = "0123456789";

	// Окно [2, 7): "23456"
	SliceStream slice(big + 2, 5);

	BLIB_TEST_CHECK(slice.size() == 5);
	BLIB_TEST_CHECK(slice.canSeek());

	char buf[8];
	BLIB_TEST_CHECK(slice.read(buf, 3) == 3);
	BLIB_TEST_CHECK(std::memcmp(buf, "234", 3) == 0);

	// Чтение за границей окна обрезается
	BLIB_TEST_CHECK(slice.seek(0, SeekOrigin::Begin));
	BLIB_TEST_CHECK(slice.read(buf, sizeof(buf)) == 5);
	BLIB_TEST_CHECK(std::memcmp(buf, "23456", 5) == 0);
	BLIB_TEST_CHECK(slice.read(buf, 1) == 0); // EOF

	// Строгий seek
	BLIB_TEST_CHECK(!slice.seek(99, SeekOrigin::Begin));
	BLIB_TEST_CHECK(slice.seek(-2, SeekOrigin::End));
	BLIB_TEST_CHECK(slice.tell() == 3);
}

BLIB_TEST_CASE("SliceStream: empty window")
{
	SliceStream empty(nullptr, 0);
	char buf[4];
	BLIB_TEST_CHECK(empty.size() == 0);
	BLIB_TEST_CHECK(empty.read(buf, 1) == 0);

	// Default-конструктор
	SliceStream def;
	BLIB_TEST_CHECK(def.size() == 0);
	BLIB_TEST_CHECK(def.read(buf, 1) == 0);
}

// ============================================================
// 4. StdStreamAdapter (переходники std <-> blib)
// ============================================================

BLIB_TEST_CASE("StdStreamAdapter: input from std::stringstream")
{
	std::stringstream ss("abcdef");

	InputStream in{StdInputStreamAdapter(&ss)};
	char buf[8];

	BLIB_TEST_CHECK(in.read(buf, 4) == 4);
	BLIB_TEST_CHECK(std::memcmp(buf, "abcd", 4) == 0);

	BLIB_TEST_CHECK(in.read(buf, sizeof(buf)) == 2);
	BLIB_TEST_CHECK(std::memcmp(buf, "ef", 2) == 0);

	BLIB_TEST_CHECK(in.read(buf, sizeof(buf)) == 0); // EOF

	// Адаптер не поддерживает позиционирование
	BLIB_TEST_CHECK(!in.canSeek());
	BLIB_TEST_CHECK(!in.seek(0, SeekOrigin::Begin));
	BLIB_TEST_CHECK(in.tell() == 0);
	BLIB_TEST_CHECK(in.size() == 0);
}

BLIB_TEST_CASE("StdStreamAdapter: output to std::stringstream (compress-to-string scenario)")
{
	std::stringstream ss;

	OutputStream out{StdOutputStreamAdapter(&ss)};
	BLIB_TEST_CHECK(out.write("compressed-data", 15) == 15);
	BLIB_TEST_CHECK(ss.str() == "compressed-data");

	// Null-поток внутри адаптера: write возвращает 0
	StdOutputStreamAdapter nullAdapter(nullptr);
	BLIB_TEST_CHECK(nullAdapter.write("x", 1) == 0);

	StdInputStreamAdapter nullIn(nullptr);
	char buf[4];
	BLIB_TEST_CHECK(nullIn.read(buf, 1) == 0);
}

// ============================================================
// 5. FileStream (временные файлы в %TEMP%)
// ============================================================

namespace
{
	/**
	 * Путь к временному файлу тестов: %TEMP% под Windows,
	 * TMPDIR/TEMP под остальными платформами.
	 */
	std::string makeTempPath(const char* name)
	{
#ifdef _WIN32
		char buf[MAX_PATH];
		DWORD n = GetTempPathA(MAX_PATH, buf);
		if (n > 0 && n < MAX_PATH)
			return std::string(buf, n) + name;
		return std::string(name);
#else
		const char* tmp = std::getenv("TMPDIR");
		if (!tmp)
			tmp = std::getenv("TEMP");
		if (!tmp)
			tmp = ".";
		return std::string(tmp) + "/" + name;
#endif
	}
}

BLIB_TEST_CASE("FileStream: open nonexistent file returns CantOpen")
{
	std::string path = makeTempPath("blib_test_no_such_file_12345.bin");
	std::remove(path.c_str());

	FileStream fs;
	FileStream::OpenModeFlags mode;
	mode.storage = static_cast<buint8>(OpenMode::Read);

	BLIB_TEST_CHECK(fs.open(path.c_str(), mode) == FileStatus::CantOpen);
	BLIB_TEST_CHECK(!fs.isOpen());
	BLIB_TEST_CHECK(!fs.canSeek());

	// Операции на неоткрытом потоке безопасны
	char buf[4];
	BLIB_TEST_CHECK(fs.read(buf, 4) == 0);
	BLIB_TEST_CHECK(fs.write("x", 1) == 0);
	BLIB_TEST_CHECK(fs.readAll().empty());
}

BLIB_TEST_CASE("FileStream: write/seek/overwrite/readAll round trip")
{
	std::string path = makeTempPath("blib_test_filestream.bin");
	std::remove(path.c_str());

	// Запись с Truncate
	FileStream fs;
	FileStream::OpenModeFlags wm;
	wm.storage = static_cast<buint8>(OpenMode::Write) |
	             static_cast<buint8>(OpenMode::Binary) |
	             static_cast<buint8>(OpenMode::Truncate);
	BLIB_TEST_REQUIRE(fs.open(path.c_str(), wm) == FileStatus::OK);
	BLIB_TEST_CHECK(fs.isOpen());
	BLIB_TEST_CHECK(fs.canSeek());

	BLIB_TEST_CHECK(fs.write("file stream data 123", 20) == 20);
	BLIB_TEST_CHECK(fs.size() == 20);
	BLIB_TEST_CHECK(fs.tell() == 20);

	// Перезапись поверх
	BLIB_TEST_CHECK(fs.seek(0, SeekOrigin::Begin));
	BLIB_TEST_CHECK(fs.write("XXXX", 4) == 4);
	BLIB_TEST_CHECK(fs.size() == 20);

	fs.close();
	BLIB_TEST_CHECK(!fs.isOpen());

	// Переоткрытие на чтение + readAll
	FileStream::OpenModeFlags rm;
	rm.storage = static_cast<buint8>(OpenMode::Read) |
	             static_cast<buint8>(OpenMode::Binary);
	BLIB_TEST_REQUIRE(fs.open(path.c_str(), rm) == FileStatus::OK);

	blib::core::ByteArray all = fs.readAll();
	BLIB_TEST_CHECK(all.size() == 20);
	BLIB_TEST_CHECK(std::memcmp(&all[0], "XXXX stream data 123", 20) == 0);

	// Позиционное чтение
	BLIB_TEST_CHECK(fs.seek(5, SeekOrigin::Begin));
	char buf[16];
	BLIB_TEST_CHECK(fs.read(buf, 7) == 7);
	BLIB_TEST_CHECK(std::memcmp(buf, "stream ", 7) == 0);
	BLIB_TEST_CHECK(fs.tell() == 12);

	fs.close();
	std::remove(path.c_str());
}

BLIB_TEST_CASE("FileStream: mode guards (read in write-only, write in read-only)")
{
	std::string path = makeTempPath("blib_test_filestream_guard.bin");
	std::remove(path.c_str());

	// Write-only поток не читает
	FileStream fs;
	FileStream::OpenModeFlags wm;
	wm.storage = static_cast<buint8>(OpenMode::Write) |
	             static_cast<buint8>(OpenMode::Binary) |
	             static_cast<buint8>(OpenMode::Truncate);
	BLIB_TEST_REQUIRE(fs.open(path.c_str(), wm) == FileStatus::OK);

	char buf[4];
	BLIB_TEST_CHECK(fs.read(buf, 4) == 0);
	BLIB_TEST_CHECK(fs.write("zz", 2) == 2);
	fs.close();

	// Read-only поток не пишет
	FileStream::OpenModeFlags rm;
	rm.storage = static_cast<buint8>(OpenMode::Read) |
	             static_cast<buint8>(OpenMode::Binary);
	BLIB_TEST_REQUIRE(fs.open(path.c_str(), rm) == FileStatus::OK);
	BLIB_TEST_CHECK(fs.write("no", 2) == 0);
	fs.close();

	std::remove(path.c_str());
}

BLIB_TEST_CASE("FileStream: truncate clears previous content")
{
	std::string path = makeTempPath("blib_test_filestream_trunc.bin");
	std::remove(path.c_str());

	// Первая запись
	FileStream fs;
	FileStream::OpenModeFlags wm;
	wm.storage = static_cast<buint8>(OpenMode::Write) |
	             static_cast<buint8>(OpenMode::Binary) |
	             static_cast<buint8>(OpenMode::Truncate);
	BLIB_TEST_REQUIRE(fs.open(path.c_str(), wm) == FileStatus::OK);
	BLIB_TEST_CHECK(fs.write("long content here", 17) == 17);
	fs.close();

	// Вторая запись с Truncate затирает файл
	BLIB_TEST_REQUIRE(fs.open(path.c_str(), wm) == FileStatus::OK);
	BLIB_TEST_CHECK(fs.write("ab", 2) == 2);
	fs.close();

	FileStream::OpenModeFlags rm;
	rm.storage = static_cast<buint8>(OpenMode::Read) |
	             static_cast<buint8>(OpenMode::Binary);
	BLIB_TEST_REQUIRE(fs.open(path.c_str(), rm) == FileStatus::OK);
	blib::core::ByteArray all = fs.readAll();
	BLIB_TEST_CHECK(all.size() == 2);
	fs.close();

	std::remove(path.c_str());
}

// ============================================================
// 6. Type-erased InputStream / OutputStream: владение и баланс памяти
// ============================================================

BLIB_TEST_CASE("InputStream: ownership allocates and releases through GlobalAllocator")
{
	auto& ga = blib::memory::GlobalAllocator::instance();
	size_t countBefore = ga.getAllocationCount();

	{
		MemoryStream mem;
		mem.write("abcde", 5);
		mem.seek(0, SeekOrigin::Begin);

		// Владение: lvalue -> heap-копия mem
		InputStream inCopy(mem);
		BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore + 1);

		char buf[8];
		BLIB_TEST_CHECK(inCopy.read(buf, 3) == 3);
		BLIB_TEST_CHECK(std::memcmp(buf, "abc", 3) == 0);
		BLIB_TEST_CHECK(mem.tell() == 0); // исходник не двигается

		// Владение: rvalue -> heap-перенос
		MemoryStream tmp;
		tmp.write("xyz", 3);
		tmp.seek(0, SeekOrigin::Begin);
		InputStream inMove(std::move(tmp));
		BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore + 2);
		BLIB_TEST_CHECK(inMove.read(buf, 3) == 3);

		// borrow не аллоцирует
		InputStream borrowed = InputStream::borrow(mem);
		BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore + 2);

		// Move value-класса не аллоцирует и не освобождает
		InputStream movedFrom(std::move(inCopy));
		BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore + 2);
		BLIB_TEST_CHECK(inCopy.isNull());
		BLIB_TEST_CHECK(movedFrom.seek(0, SeekOrigin::Begin));
		BLIB_TEST_CHECK(movedFrom.read(buf, 3) == 3);
	}

	// Все владеемые копии освобождены (регрессия бага deallocate
	// по смещённому указателю базового класса)
	BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore);
}

BLIB_TEST_CASE("InputStream: null stream behaviour")
{
	InputStream nullIn;

	BLIB_TEST_CHECK(nullIn.isNull());

	char buf[4];
	BLIB_TEST_CHECK(nullIn.read(buf, 4) == 0);
	BLIB_TEST_CHECK(!nullIn.canSeek());
	BLIB_TEST_CHECK(!nullIn.seek(0, SeekOrigin::Begin));
	BLIB_TEST_CHECK(nullIn.tell() == 0);
	BLIB_TEST_CHECK(nullIn.size() == 0);
}

BLIB_TEST_CASE("OutputStream: ownership, move and GlobalAllocator balance")
{
	auto& ga = blib::memory::GlobalAllocator::instance();
	size_t countBefore = ga.getAllocationCount();

	{
		MemoryStream mem;
		OutputStream owned(mem); // владеющая heap-копия
		BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore + 1);
		BLIB_TEST_CHECK(owned.write("data", 4) == 4);
		BLIB_TEST_CHECK(mem.size() == 0); // копия изолирована

		OutputStream borrowed = OutputStream::borrow(mem);
		BLIB_TEST_CHECK(borrowed.write("xy", 2) == 2);
		BLIB_TEST_CHECK(mem.size() == 2); // borrow пишет в оригинал

		OutputStream moved(std::move(owned));
		BLIB_TEST_CHECK(owned.isNull());
		BLIB_TEST_CHECK(moved.write("z", 1) == 1);

		OutputStream nullOut;
		BLIB_TEST_CHECK(nullOut.isNull());
		BLIB_TEST_CHECK(nullOut.write("x", 1) == 0);
	}

	BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore);
}

BLIB_TEST_CASE("OutputStream: move assignment releases previous stream")
{
	auto& ga = blib::memory::GlobalAllocator::instance();
	size_t countBefore = ga.getAllocationCount();

	{
		MemoryStream memA;
		MemoryStream memB;

		OutputStream outA(memA);
		OutputStream outB(memB);
		BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore + 2);

		// Замена: старый владеемый поток (A) освобождается
		outB = std::move(outA);
		BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore + 1);
		BLIB_TEST_CHECK(outA.isNull());
		BLIB_TEST_CHECK(outB.write("k", 1) == 1);
	}

	BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore);
}

// ============================================================
// 7. Композиция: FileStream через type-erased обёртки
// ============================================================

BLIB_TEST_CASE("Composition: FileStream inside type-erased InputStream")
{
	std::string path = makeTempPath("blib_test_filestream_compose.bin");
	std::remove(path.c_str());

	{
		FileStream fs;
		FileStream::OpenModeFlags wm;
		wm.storage = static_cast<buint8>(OpenMode::Write) |
		             static_cast<buint8>(OpenMode::Binary) |
		             static_cast<buint8>(OpenMode::Truncate);
		BLIB_TEST_REQUIRE(fs.open(path.c_str(), wm) == FileStatus::OK);
		BLIB_TEST_CHECK(fs.write("0123456789", 10) == 10);
	}

	{
		FileStream fs;
		FileStream::OpenModeFlags rm;
		rm.storage = static_cast<buint8>(OpenMode::Read) |
		             static_cast<buint8>(OpenMode::Binary);
		BLIB_TEST_REQUIRE(fs.open(path.c_str(), rm) == FileStatus::OK);

		// FileStream (двойное наследование IInputStream+IOutputStream)
		// move-only: корректно упаковывается в type-erased обёртку как rvalue
		// (смещение подобъекта базового класса - регрессия бага deallocate
		// по смещённому указателю)
		InputStream in(std::move(fs));
		char buf[8];
		BLIB_TEST_CHECK(in.read(buf, 5) == 5);
		BLIB_TEST_CHECK(std::memcmp(buf, "01234", 5) == 0);
		BLIB_TEST_CHECK(in.seek(8, SeekOrigin::Begin));
		BLIB_TEST_CHECK(in.read(buf, 4) == 2);
		BLIB_TEST_CHECK(std::memcmp(buf, "89", 2) == 0);
	} // in разрушается -> файл закрывается

	std::remove(path.c_str());
}

BLIB_TEST_CASE("FileStream: move transfers open file")
{
	std::string path = makeTempPath("blib_test_filestream_move.bin");
	std::remove(path.c_str());

	{
		FileStream fs;
		FileStream::OpenModeFlags wm;
		wm.storage = static_cast<buint8>(OpenMode::Write) |
		             static_cast<buint8>(OpenMode::Binary) |
		             static_cast<buint8>(OpenMode::Truncate);
		BLIB_TEST_REQUIRE(fs.open(path.c_str(), wm) == FileStatus::OK);
		BLIB_TEST_CHECK(fs.write("0123456789", 10) == 10);

		// Перенос открытого файла: источник закрыт, приёмник открыт
		FileStream moved(std::move(fs));
		BLIB_TEST_CHECK(!fs.isOpen());
		BLIB_TEST_CHECK(moved.isOpen());
		BLIB_TEST_CHECK(moved.size() == 10);

		// Move-присваивание
		FileStream assigned;
		assigned = std::move(moved);
		BLIB_TEST_CHECK(!moved.isOpen());
		BLIB_TEST_CHECK(assigned.isOpen());
		BLIB_TEST_CHECK(assigned.seek(0, SeekOrigin::Begin));
		BLIB_TEST_CHECK(assigned.write("ZZ", 2) == 2);
	}

	std::remove(path.c_str());
}
