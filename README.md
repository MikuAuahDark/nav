NAV
=====

NPad Audio Video decoding library.

Performs audio and video decoding using platform-specific backends.

Backends
-----

NAV relies on various backends to perform audio and video decoding.

| Backend           | Kind      | OS Availability         | `nav_backend_name()` | Disablement Env. Var\* | Additional Notes                                         |
|-------------------|-----------|-------------------------|----------------------|------------------------|----------------------------------------------------------|
| [NdkMedia]        | OS API    | Android                 | `"android"`          | `ANDROIDNDK`           | Due to API limitations, Android 9 is required.           |
| [FFmpeg] 4        | 3rd-Party | Windows, Linux, Android | `"ffmpeg4"`          | `FFMPEG4`              | Requires the appropriate header files to be present.\*\* |
| [FFmpeg] 5        | 3rd-Party | Windows, Linux, Android | `"ffmpeg5"`          | `FFMPEG5`              | Requires the appropriate header files to be present.\*\* |
| [FFmpeg] 6        | 3rd-Party | Windows, Linux, Android | `"ffmpeg6"`          | `FFMPEG6`              | Requires the appropriate header files to be present.\*\* |
| [FFmpeg] 7        | 3rd-Party | Windows, Linux, Android | `"ffmpeg7"`          | `FFMPEG7`              | Requires the appropriate header files to be present.\*\* |
| [GStreamer]       | 3rd-Party | Linux                   | `"gstreamer"`        | `GSTREAMER`            | Requires the appropriate header files to be present.     |
| [MediaFoundation] | OS API    | Windows                 | `"mediafoundation"`  | `MEDIAFOUNDATION`      | UWP target is not supported.                             |

\*: Prepend `NAV_DISABLE_` to the environment variable name. It is shortened due to readability reasons.

\*\*: By default, only one FFmpeg version is picked based on what's the compiler can find. To compile with support for multiple FFmpeg versions, see below.

[FFmpeg]: https://ffmpeg.org/
[MediaFoundation]: https://learn.microsoft.com/en-us/windows/win32/medfound/microsoft-media-foundation-sdk
[NdkMedia]: https://developer.android.com/ndk/reference/group/media
[GStreamer]: https://gstreamer.freedesktop.org/

Multiple FFmpeg Version Support
-----

NAV since 0.3.0 can be compiled to support multiple FFmpeg versions.

To compile with multiple FFmpeg versions, pass `-DFFMPEGn_DIR` (where `n` is between 4-7) in CMake command-line to
location where FFmpeg header files are placed. Note that the directory should be one directory above the `include`.
For example, if you have FFmpeg 7 includes in `/path/to/ffmpeg7/include`, then do `-DFFMPEG7_DIR=/path/to/ffmpeg7`.

Example command-line for CMake:
```
cmake -Bbuild -S. --install-prefix $PWD/installdir -DFFMPEG6_DIR=/path/to/ffmpeg6 -DFFMPEG7_DIR=/path/to/ffmpeg7
```

For best compatibility, it's recommended to use FFmpeg n.0 header files. Note that NAV only needs functional header
files to compile as library files will be loaded at runtime.

License
-----

zlib/libpng license.
