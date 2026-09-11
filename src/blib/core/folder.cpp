#include <blib/core/folder.h>

#ifdef __blib_compile_platform_windows
#include <Windows.h>
#include <fileapi.h>
#endif // __blib_compile_platform_windows

#include <blib/core/string.h>

blib::core::Folder::Folder(const std::string& path)
{
	this->currentPath = path;
	blib::core::replace(this->currentPath, "\\", "/");
	if (!(this->isFolder()))
	{
		this->up();
	}
}

std::string blib::core::Folder::getCurrentPath() const
{
    return this->currentPath;
}

__blib_platform_depended std::vector<std::string> blib::core::Folder::getAllEntries() const
{
#ifdef __blib_compile_platform_windows
	std::string windowsDependedPath = this->currentPath;
	blib::core::replace(windowsDependedPath, "/", "\\");
	windowsDependedPath += "*";

	std::vector<std::string> res;
	std::wstring path = std::wstring(windowsDependedPath.begin(), windowsDependedPath.end());
	WIN32_FIND_DATAW wfd;
	HANDLE hFind = FindFirstFileW(path.c_str(), &wfd);
	if (INVALID_HANDLE_VALUE != hFind)
	{
		do
		{
			// BUG-FIX: раньше WCHAR-имя копировалось побайтово в char
			// (каждый второй байт — 0), из-за чего std::string
			// обрезался до первого символа и Folder::down никогда
			// не находил файлы. Правильная конвертация — через
			// WideCharToMultiByte
			int length = WideCharToMultiByte(
				CP_UTF8, 0,
				wfd.cFileName, -1,
				nullptr, 0,
				nullptr, nullptr);
			if (length <= 0)
			{
				continue;
			}

			std::string entry(static_cast<size_t>(length), '\0');
			WideCharToMultiByte(
				CP_UTF8, 0,
				wfd.cFileName, -1,
				&entry[0], length,
				nullptr, nullptr);
			// Обрезать завершающий '\0' (length включает его)
			entry.resize(static_cast<size_t>(length) - 1);

			res.push_back(std::move(entry));
		} while (NULL != FindNextFileW(hFind, &wfd));
		FindClose(hFind);
	}
	
	DWORD errorMessageID = ::GetLastError();

	LPSTR messageBuffer = nullptr;

	//Ask Win32 to give us the string version of that message ID.
	//The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	return res;
#else
	// stub: перечисление каталога для не-Windows платформ пока не реализовано
	return std::vector<std::string>();
#endif // __blib_compile_platform_windows
}

bool blib::core::Folder::isFolder()
{
#ifdef __blib_compile_platform_windows
	std::wstring path = std::wstring(this->currentPath.begin(), this->currentPath.end());
	WIN32_FIND_DATAW wfd;
	HANDLE hFind = FindFirstFileW(path.c_str(), &wfd);
	if (INVALID_HANDLE_VALUE != hFind)
	{
		return wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
	}
	return false;
#else
	// stub: проверка существования каталога для не-Windows платформ пока не реализована
	return false;
#endif // __blib_compile_platform_windows
}

bool blib::core::Folder::up()
{
	size_t pos = this->currentPath.rfind("/");
	if (pos == std::string::npos)
		return false;

	this->currentPath.resize(pos + 1);

	return true;
}

bool blib::core::Folder::down(const std::string& foldername)
{
	auto entries = this->getAllEntries();

	if (blib::core::contains(entries, foldername))
	{
		this->currentPath = this->currentPath + "/" + foldername;
		return true;
	}

	return false;
}
