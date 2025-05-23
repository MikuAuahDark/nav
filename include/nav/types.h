#ifndef _NAV_TYPES_H_
#define _NAV_TYPES_H_

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Opaque structure that contains a loaded instance of media file.
 * @sa nav_open
 */
typedef struct nav_t nav_t;

/**
 * @brief Opaque structure that contains stream information.
 * @sa nav_stream_info
 * @sa nav_frame_streaminfo
 */
typedef struct nav_streaminfo_t nav_streaminfo_t;

/**
 * @brief Opaque structure that contains decoded stream data.
 * @sa nav_read
 */
typedef struct nav_frame_t nav_frame_t;

#ifdef __cplusplus
typedef bool nav_bool;
#else
#include <stdbool.h>
typedef _Bool nav_bool;
#endif

/**
 * @brief Possible pixel format of decoded video frames.
 * @sa nav_streaminfo_t
 */
typedef enum nav_pixelformat
{
	/* Unknown pixel format (may denote errors) */
	NAV_PIXELFORMAT_UNKNOWN = -1,
	/* RGB, 24 bits per pixel, packed. */
	NAV_PIXELFORMAT_RGB8,
	/* YUV 4:2:0 subsampling, planar. */
	NAV_PIXELFORMAT_YUV420,
	/* YUV 4:4:4 subsampling, planar. */
	NAV_PIXELFORMAT_YUV444,
	/* YUV 4:2:0 subsampling, Y is planar, UV is packed. */
	NAV_PIXELFORMAT_NV12
} nav_pixelformat;

/**
 * @brief Possible stream type.
 * @sa nav_streaminfo_t
 */
typedef enum nav_streamtype
{
	/* Unknown stream type */
	NAV_STREAMTYPE_UNKNOWN = -1,
	/* Audio stream */
	NAV_STREAMTYPE_AUDIO,
	/* Video stream */
	NAV_STREAMTYPE_VIDEO
} nav_streamtype;

typedef enum nav_backendtype
{
	/* Unknown backend type. */
	NAV_BACKENDTYPE_UNKNOWN = -1,
	/* This backend leverages OS-specific API to work (almost guaranteed to exist in particular OS). */
	NAV_BACKENDTYPE_OS_API,
	/* This backend leverages 3rd-party library to work (depends if the user installed the library). */
	NAV_BACKENDTYPE_3RD_PARTY
} nav_backendtype;

#define NAV_SETTINGS_VERSION 0

typedef struct nav_settings
{
	/* nav_settings struct version. Must be initialize to NAV_SETTINGS_VERSION */
	uint64_t version;
	/* 0-terminated **1-based** backend index to try in order. Example: if `{2, 1, 0}` is specified, then it
	 * will try to load using 2nd backend first, then trying the 1st backend. This can be NULL to use default
	 * order (which is `{1, 2, 3, ..., nav_backend_count(), 0}`). */
	const size_t *backend_order;
	/* Hint to backend on how many threads to use. If this is 0, it defaults to value of NAV_THREAD_COUNT environment
	 * variable, or amount of threads in the current user system.
	 * **Note**: NAV won't check if the specified `max_threads` exceeded the amount of the system CPU thread count,
	 * but it ensure the value is at least 1.
	 */
	uint32_t max_threads;
	/* If true, this hints backends to prefer CPU decoding. */
	bool disable_hwaccel;
} nav_settings;

#endif /* _NAV_TYPES_H_ */
