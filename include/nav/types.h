#ifndef _NAV_TYPES_H_
#define _NAV_TYPES_H_

#include <stdint.h>

typedef struct nav_t nav_t;
typedef struct nav_streaminfo_t nav_streaminfo_t;
typedef struct nav_packet_t nav_packet_t;
typedef uint8_t nav_bool;

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
} nav_pixelformat;

typedef enum nav_streamtype
{
	/* Unknown stream type */
	NAV_STREAMTYPE_UNKNOWN = -1,
	/* Audio stream */
	NAV_STREAMTYPE_AUDIO,
	/* Video stream */
	NAV_STREAMTYPE_VIDEO
} nav_streamtype;

#endif /* _NAV_TYPES_H_ */
