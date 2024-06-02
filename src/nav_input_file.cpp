#define _FILE_OFFSET_BITS 64

#include <cstdio>
#include <vector>

/* Needed to convert UTF-8 filename */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "nav_input_file.hpp"
#include "nav_error.hpp"

namespace nav::input::file
{

static void closefile(void **userdata)
{
	FILE **f = (FILE**) userdata;
	fclose(*f);
	*f = nullptr;
}


static size_t readfile(void *userdata, void *dest, size_t size)
{
	return fread(dest, 1, size, (FILE*) userdata);
}

static nav_bool seekfile(void *userdata, uint64_t pos)
{
#ifdef _MSC_VER
	return _fseeki64((FILE*) userdata, (int64_t) pos, SEEK_SET) == 0;
#else
	return fseeko((FILE*) userdata, (off_t) pos, SEEK_SET) == 0;
#endif
}

static uint64_t tellfile(void *userdata)
{
#ifdef _MSC_VER
	return (uint64_t) _ftelli64((FILE*) userdata);
#else
	return (uint64_t) ftello((FILE*) userdata);
#endif
}

bool populate(nav_input *input, const std::string &filename)
{
	FILE *f = nullptr;

#ifdef _WIN32
	static UINT codepage = 0;
	if (!codepage)
		codepage = GetACP();
	
	if (codepage == CP_UTF8)
		// Don't bother converting
		f = fopen(filename.c_str(), "rb");
	else
	{
		// Do conversion from UTF-8 to UTF-16.
		int wsize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename.c_str(), filename.length(), nullptr, 0);
		if (wsize == 0)
		{
			nav::error::set("Invalid filename");
			return false;
		}

		std::vector<wchar_t> wide(wsize + 1, 0);
		int result = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename.c_str(), filename.length(), wide.data(), wsize);
		if (!result)
		{
			nav::error::set("Invalid filename");
			return false;
		}

		f = _wfopen(wide.data(), L"rb");
	}
#else
	f = fopen(filename.c_str(), "rb");
#endif

	if (!f)
	{
		nav::error::set("Cannot open file");
		return false;
	}

	nav::error::set("");
	input->userdata = f;
	input->close = closefile;
	input->read = readfile;
	input->seek = seekfile;
	input->tell = tellfile;
	return true;
}

}