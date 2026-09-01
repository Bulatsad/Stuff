#include <blib/test/src/test.h>

// Сериализация: endian-утилиты, BinaryReader, BinaryWriter
#include <blib/core/endian.h>
#include <blib/core/istream.h>
#include <blib/core/ostream.h>
#include <blib/core/memoryStream.h>
#include <blib/core/stdStreamAdapter.h>
#include <blib/core/binaryReader.h>
#include <blib/core/binaryWriter.h>

#include <string>
#include <sstream>
#include <cstring>

using namespace blib::core;

// ============================================================
// 1. endian-утилиты
// ============================================================

BLIB_TEST_CASE("endian: LE round trips for u16/u32/u64")
{
	buint8 buf[8];

	writeU16LE(buf, 0x1234);
	BLIB_TEST_CHECK(buf[0] == 0x34 && buf[1] == 0x12);
	BLIB_TEST_CHECK(readU16LE(buf) == 0x1234);

	writeU32LE(buf, 0xDEADBEEFu);
	BLIB_TEST_CHECK(buf[0] == 0xEF && buf[1] == 0xBE && buf[2] == 0xAD && buf[3] == 0xDE);
	BLIB_TEST_CHECK(readU32LE(buf) == 0xDEADBEEFu);

	writeU64LE(buf, 0x0102030405060708ull);
	BLIB_TEST_CHECK(readU64LE(buf) == 0x0102030405060708ull);
	BLIB_TEST_CHECK(buf[0] == 0x08 && buf[7] == 0x01);
}

BLIB_TEST_CASE("endian: BE round trips for u16/u32/u64")
{
	buint8 buf[8];

	writeU16BE(buf, 0xABCD);
	BLIB_TEST_CHECK(buf[0] == 0xAB && buf[1] == 0xCD);
	BLIB_TEST_CHECK(readU16BE(buf) == 0xABCD);

	writeU32BE(buf, 0x12345678u);
	BLIB_TEST_CHECK(buf[0] == 0x12 && buf[1] == 0x34 && buf[2] == 0x56 && buf[3] == 0x78);
	BLIB_TEST_CHECK(readU32BE(buf) == 0x12345678u);

	writeU64BE(buf, 0x0102030405060708ull);
	BLIB_TEST_CHECK(readU64BE(buf) == 0x0102030405060708ull);
}

BLIB_TEST_CASE("endian: cross-order reads (LE bytes as BE and vice versa)")
{
	buint8 buf[8];

	writeU64LE(buf, 0x0102030405060708ull);
	BLIB_TEST_CHECK(readU64BE(buf) == 0x0807060504030201ull);

	writeU32BE(buf, 0x11223344u);
	BLIB_TEST_CHECK(readU32LE(buf) == 0x44332211u);
}

BLIB_TEST_CASE("endian: unaligned buffers are safe")
{
	// Буфер со смещением +1: p не выровнен
	char raw[9];
	buint8* p = reinterpret_cast<buint8*>(raw + 1);

	writeU32LE(p, 0xDEADBEEFu);
	BLIB_TEST_CHECK(readU32LE(p) == 0xDEADBEEFu);

	writeU64BE(p, 0x1122334455667788ull);
	BLIB_TEST_CHECK(readU64BE(p) == 0x1122334455667788ull);
}

BLIB_TEST_CASE("endian: swap functions")
{
	BLIB_TEST_CHECK(swapU16(0x0102) == 0x0201);
	BLIB_TEST_CHECK(swapU32(0x01020304u) == 0x04030201u);
	BLIB_TEST_CHECK(swapU64(0x0102030405060708ull) == 0x0807060504030201ull);
}

// ============================================================
// 2. BinaryWriter/BinaryReader: полный round-trip
// ============================================================

BLIB_TEST_CASE("Binary: full LE/BE round trip through MemoryStream")
{
	MemoryStream mem;
	BinaryWriter w(mem);

	BLIB_TEST_CHECK(w.writeU8(0xAB));
	BLIB_TEST_CHECK(w.writeU16LE(0x1234));
	BLIB_TEST_CHECK(w.writeU32LE(0x12345678u));
	BLIB_TEST_CHECK(w.writeU64LE(0x0102030405060708ull));
	BLIB_TEST_CHECK(w.writeU16BE(0xABCD));
	BLIB_TEST_CHECK(w.writeU32BE(0xDEADBEEFu));
	BLIB_TEST_CHECK(w.writeU64BE(0x8877665544332211ull));
	BLIB_TEST_CHECK(w.writeString("hello"));

	BLIB_TEST_CHECK(mem.size() == 1 + 2 + 4 + 8 + 2 + 4 + 8 + 6);
	BLIB_TEST_CHECK(mem.seek(0, SeekOrigin::Begin));

	BinaryReader r(mem);
	buint8 u8;
	buint16 u16;
	buint32 u32;
	buint64 u64;
	std::string s;

	BLIB_TEST_CHECK(r.readU8(u8) && u8 == 0xAB);
	BLIB_TEST_CHECK(r.readU16LE(u16) && u16 == 0x1234);
	BLIB_TEST_CHECK(r.readU32LE(u32) && u32 == 0x12345678u);
	BLIB_TEST_CHECK(r.readU64LE(u64) && u64 == 0x0102030405060708ull);
	BLIB_TEST_CHECK(r.readU16BE(u16) && u16 == 0xABCD);
	BLIB_TEST_CHECK(r.readU32BE(u32) && u32 == 0xDEADBEEFu);
	BLIB_TEST_CHECK(r.readU64BE(u64) && u64 == 0x8877665544332211ull);
	BLIB_TEST_CHECK(r.readString(s, 32) && s == "hello");
}

BLIB_TEST_CASE("Binary: EOF after all data returns false")
{
	MemoryStream mem;
	BinaryWriter w(mem);
	BLIB_TEST_CHECK(w.writeU32LE(0x42));
	BLIB_TEST_CHECK(mem.seek(0, SeekOrigin::Begin));

	BinaryReader r(mem);
	buint32 v;
	BLIB_TEST_CHECK(r.readU32LE(v) && v == 0x42);

	// Данные кончились: все чтения - false
	buint8 u8;
	buint64 u64;
	BLIB_TEST_CHECK(!r.readU8(u8));
	BLIB_TEST_CHECK(!r.readU32LE(v));
	BLIB_TEST_CHECK(!r.readU64BE(u64));
}

BLIB_TEST_CASE("Binary: strict readBytes (partial data is an error)")
{
	MemoryStream mem;
	mem.write("abcde", 5);
	BLIB_TEST_CHECK(mem.seek(0, SeekOrigin::Begin));

	BinaryReader r(mem);

	char buf[8];
	BLIB_TEST_CHECK(r.readBytes(buf, 5));
	BLIB_TEST_CHECK(std::memcmp(buf, "abcde", 5) == 0);

	// Больше 5 байт нет - false, читать дальше нельзя
	char big[64];
	BLIB_TEST_CHECK(!r.readBytes(big, sizeof(big)));
}

BLIB_TEST_CASE("Binary: readString with and without terminator")
{
	MemoryStream mem;
	BinaryWriter w(mem);
	BLIB_TEST_CHECK(w.writeString("abc"));
	BLIB_TEST_CHECK(w.writeBytes("no-terminator", 13));
	BLIB_TEST_CHECK(mem.seek(0, SeekOrigin::Begin));

	BinaryReader r(mem);
	std::string s;

	// Корректная строка
	BLIB_TEST_CHECK(r.readString(s, 16) && s == "abc");

	// Строка без терминатора: лимит достигнут - false
	BLIB_TEST_CHECK(!r.readString(s, 13));
	BLIB_TEST_CHECK(!r.readString(s, 100));
}

BLIB_TEST_CASE("Binary: writeBytes/writeU8 with null and zero size")
{
	MemoryStream mem;
	BinaryWriter w(mem);

	// Пустые записи - без ошибок и без данных
	BLIB_TEST_CHECK(w.writeBytes(nullptr, 0));
	BLIB_TEST_CHECK(w.writeBytes(nullptr, 4));
	BLIB_TEST_CHECK(mem.size() == 0);

	BLIB_TEST_CHECK(w.writeU8(7));
	BLIB_TEST_CHECK(mem.size() == 1);
}

// ============================================================
// 3. Сценарий: сериализация в std::stringstream и обратно
//    (мотивация всего stream-API: писать не в файл, а в строку)
// ============================================================

BLIB_TEST_CASE("Binary: round trip through std::stringstream")
{
	std::stringstream ss;
	StdOutputStreamAdapter outAdapter(&ss);
	BinaryWriter w{outAdapter};

	BLIB_TEST_CHECK(w.writeU32BE(0xCAFEBABEu));
	BLIB_TEST_CHECK(w.writeString("compressed!"));

	std::string bytes = ss.str();
	BLIB_TEST_CHECK(bytes.size() == 4 + 11 + 1);
	BLIB_TEST_CHECK(static_cast<buint8>(bytes[0]) == 0xCA);
	BLIB_TEST_CHECK(static_cast<buint8>(bytes[1]) == 0xFE);
	BLIB_TEST_CHECK(static_cast<buint8>(bytes[2]) == 0xBA);
	BLIB_TEST_CHECK(static_cast<buint8>(bytes[3]) == 0xBE);

	// Чтение обратно из stringstream
	std::stringstream ss2(bytes);
	StdInputStreamAdapter inAdapter(&ss2);
	BinaryReader r{inAdapter};

	buint32 magic;
	std::string s;
	BLIB_TEST_CHECK(r.readU32BE(magic) && magic == 0xCAFEBABEu);
	BLIB_TEST_CHECK(r.readString(s, 64) && s == "compressed!");
}

// ============================================================
// 4. Владение потоками: BinaryReader/Writer от type-erased обёрток
// ============================================================

BLIB_TEST_CASE("Binary: reader/writer own moved type-erased streams")
{
	auto& ga = blib::memory::GlobalAllocator::instance();
	size_t countBefore = ga.getAllocationCount();

	{
		MemoryStream mem;
		mem.write("abcd", 4);
		mem.seek(0, SeekOrigin::Begin);

		InputStream in(mem);
		BinaryReader r(std::move(in));

		char buf[4];
		BLIB_TEST_CHECK(r.readBytes(buf, 4));
		BLIB_TEST_CHECK(std::memcmp(buf, "abcd", 4) == 0);
	}

	{
		MemoryStream outMem;
		OutputStream out = OutputStream::borrow(outMem);
		BinaryWriter w(std::move(out));

		BLIB_TEST_CHECK(w.writeU16LE(0x7777));
		BLIB_TEST_CHECK(outMem.size() == 2);
	}

	BLIB_TEST_CHECK(ga.getAllocationCount() == countBefore);
}

// ============================================================
// 5. Контракт интерфейса: BinaryReader поверх произвольного IInputStream
// ============================================================

namespace
{
	/**
	 * Probe: IInputStream, отдающий байты вручную (без MemoryStream),
	 * чтобы проверить что BinaryReader работает чисто через интерфейс.
	 */
	class RawBytesInput : public IInputStream
	{
	public:
		explicit RawBytesInput(const buint8* bytes, size_t count)
			: bytes(bytes)
			, count(count)
			, pos(0)
		{
		}

		size_t read(_Out void* buffer, size_t size) __blib_override
		{
			size_t available = this->count - this->pos;
			size_t n = size < available ? size : available;
			if (n)
				std::memcpy(buffer, this->bytes + this->pos, n);
			this->pos += n;
			return n;
		}

		bool canSeek() const __blib_override { return false; }
		bool seek(bint64 offset, SeekOrigin origin) __blib_override { (void)offset; (void)origin; return false; }
		buint64 tell() const __blib_override { return static_cast<buint64>(this->pos); }
		buint64 size() const __blib_override { return static_cast<buint64>(this->count); }

	private:
		const buint8* bytes;
		size_t count;
		size_t pos;
	};
}

BLIB_TEST_CASE("Binary: BinaryReader works with any custom IInputStream")
{
	buint8 raw[] = {0x01, 0x02, 0x03, 0x04, 0xFF, 0xEE, 'x', 'y', 0x00};
	RawBytesInput probe(raw, sizeof(raw));

	BinaryReader r(probe);

	buint32 v;
	BLIB_TEST_CHECK(r.readU32BE(v) && v == 0x01020304u);

	buint16 w;
	BLIB_TEST_CHECK(r.readU16LE(w) && w == 0xEEFF);

	std::string s;
	BLIB_TEST_CHECK(r.readString(s, 8) && s == "xy");
}
