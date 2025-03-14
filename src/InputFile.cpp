// See nav_input_file_android.cpp for Android handling instead.
#ifndef __ANDROID__
#define _FILE_OFFSET_BITS 64

#include <cstdio>

/* Needed to convert UTF-8 filename */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "InputFile.hpp"
#include "Common.hpp"
#include "Error.hpp"

// Good thing MSVC provide us 64-bit file seek/tell anywhere.
#ifdef _MSC_VER
// Use preprocessor because typedef error if off_t is already defined somewhere.
#define off_t int64_t
#define fseeko _fseeki64
#define ftello _ftelli64
#endif

namespace nav::input::file
{

static void closefile(void **userdata)
{
	FILE **f = (FILE**) userdata;
	fclose(*f);
	*f = nullptr;
}

inline int seekimpl(FILE *f, int64_t pos, int origin)
{
	return fseeko(f, (off_t) pos, origin);
}

inline uint64_t tellimpl(FILE *f)
{
	return (uint64_t) ftello(f);
}

static size_t readfile(void *userdata, void *dest, size_t size)
{
	return fread(dest, 1, size, (FILE*) userdata);
}

static nav_bool seekfile(void *userdata, uint64_t pos)
{
	return seekimpl((FILE*) userdata, (int64_t) pos, SEEK_SET) == 0;
}

static uint64_t tellfile(void *userdata)
{
	return tellimpl((FILE*) userdata);
}

static uint64_t sizefile(void *userdata)
{
	FILE *f = (FILE*) userdata;
	uint64_t curpos = tellimpl(f);
	if (seekimpl(f, 0, SEEK_END) != 0)
		return 0;
	
	uint64_t size = tellimpl(f);
	seekimpl(f, curpos, SEEK_SET);
	return size;
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
		try
		{
			std::wstring wide = nav::fromUTF8(filename);
			f = _wfopen(wide.c_str(), L"rb");
		}
		catch (const std::exception &e)
		{
			nav::error::set(e);
			return false;
		}
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
	input->size = sizefile;
	return true;
}

}

#endif // #ifndef __ANDROID__
