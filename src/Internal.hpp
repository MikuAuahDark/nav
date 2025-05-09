#ifndef _NAV_INTERNAL_HPP_
#define _NAV_INTERNAL_HPP_

#ifndef _NAV_IMPLEMENTATION_
#define _NAV_IMPLEMENTATION_
#endif

#define NAV_STRINGIZE(x) _NAV_STRINGIZE0_(x)
#define _NAV_STRINGIZE0_(x) #x

#ifndef NAV_CAT
#define NAV_CAT(x, y) x##y
#endif

#include <cstdint>

#include "nav/audioformat.h"
#include "nav/types.h"

#include "Backend.hpp"

namespace nav
{

typedef nav_t State;
typedef nav_streaminfo_t StreamInfo;
typedef nav_frame_t Frame;

class Backend;

}

struct nav_t
{
	virtual ~nav_t();
	virtual nav::Backend *getBackend() const noexcept = 0;
	virtual size_t getStreamCount() const noexcept = 0;
	virtual const nav_streaminfo_t *getStreamInfo(size_t index) const noexcept = 0;
	virtual bool isStreamEnabled(size_t index) const noexcept = 0;
	virtual bool setStreamEnabled(size_t index, bool enabled) = 0;
	virtual double getDuration() noexcept = 0;
	virtual double getPosition() noexcept = 0;
	virtual double setPosition(double off) = 0;
	virtual nav_frame_t *read() = 0;
};

struct nav_streaminfo_t
{
	struct AudioStreamInfo
	{
		uint32_t nchannels;
		uint32_t sample_rate;
		nav_audioformat format;

		inline size_t size() const
		{
			return NAV_AUDIOFORMAT_BYTESIZE(format) * (size_t) nchannels;
		}
	};

	struct VideoStreamInfo
	{
		double fps;
		uint32_t width, height;
		nav_pixelformat format;

		inline size_t size() const
		{
			size_t dimensions = (size_t) width * (size_t) height;

			switch (format)
			{
				case NAV_PIXELFORMAT_UNKNOWN:
				default:
					return 0;
				case NAV_PIXELFORMAT_RGB8:
				case NAV_PIXELFORMAT_YUV444:
					return 3 * dimensions;
				case NAV_PIXELFORMAT_YUV420:
				case NAV_PIXELFORMAT_NV12:
					return dimensions + 2 * ((width + 1) / 2) * ((height + 1) / 2);
			}
		}

		// For YUV, only the Y plane is considered.
		inline size_t stride() const
		{
			switch (format)
			{
				case NAV_PIXELFORMAT_UNKNOWN:
				case NAV_PIXELFORMAT_YUV420:
				case NAV_PIXELFORMAT_YUV444:
				case NAV_PIXELFORMAT_NV12:
				default:
					return width;
				case NAV_PIXELFORMAT_RGB8:
					return ((size_t) width) * 3;
			}
		}
	};

	inline size_t plane_width(size_t index) const
	{
		switch (type)
		{
			case NAV_STREAMTYPE_UNKNOWN:
			default:
				return 0;
			case NAV_STREAMTYPE_VIDEO:
			{
				switch (video.format)
				{
					case NAV_PIXELFORMAT_UNKNOWN:
					default:
						return 0;
					case NAV_PIXELFORMAT_YUV420:
					{
						switch (index)
						{
							case 0:
								return video.width;
							case 1:
							case 2:
								return (video.width + 1) / 2;
							default:
								return 0;
						}

						return 0;
					}
					case NAV_PIXELFORMAT_YUV444:
						return (index >= 0 && index < 3) * (size_t) video.width;
					case NAV_PIXELFORMAT_NV12:
						switch (index)
						{
							case 0:
								return video.width;
							case 1:
								return ((video.width + 1) / 2) * 2;
							default:
								return 0;
						}

						return 0;
					case NAV_PIXELFORMAT_RGB8:
						return video.width * (size_t) 3;
				}

				return 0;
			}
			case NAV_STREAMTYPE_AUDIO:
				return (index == 0) * audio.size();
		}

		return 0;
	}

	inline size_t plane_height(size_t index) const
	{
		switch (type)
		{
			case NAV_STREAMTYPE_UNKNOWN:
			default:
				return 0;
			case NAV_STREAMTYPE_VIDEO:
			{
				switch (video.format)
				{
					case NAV_PIXELFORMAT_UNKNOWN:
					default:
						return 0;
					case NAV_PIXELFORMAT_YUV420:
					{
						switch (index)
						{
							case 0:
								return video.height;
							case 1:
							case 2:
								return (video.height + 1) / 2;
							default:
								return 0;
						}

						return 0;
					}
					case NAV_PIXELFORMAT_YUV444:
						return (index >= 0 && index < 3) * (size_t) video.height;
					case NAV_PIXELFORMAT_NV12:
						switch (index)
						{
							case 0:
								return video.height;
							case 1:
								return (video.height + 1) / 2;
							default:
								return 0;
						}

						return 0;
					case NAV_PIXELFORMAT_RGB8:
						return video.height;
				}

				return 0;
			}
			case NAV_STREAMTYPE_AUDIO:
				return (size_t) (index == 0);
		}

		return 0;
	}

	nav_streamtype type;
	union
	{
		AudioStreamInfo audio;
		VideoStreamInfo video;
	};
};

struct nav_frame_t
{
	virtual ~nav_frame_t();
	virtual size_t getStreamIndex() const noexcept = 0;
	virtual const nav_streaminfo_t *getStreamInfo() const noexcept = 0;
	virtual double tell() const noexcept = 0;
	virtual const uint8_t *const *acquire(ptrdiff_t **strides, size_t *nplanes) = 0;
	virtual void release() noexcept = 0;

	inline bool operator<(const nav_frame_t &other) const noexcept
	{
		return tell() < other.tell();
	}
};

#endif /* _NAV_INTERNAL_HPP_ */
