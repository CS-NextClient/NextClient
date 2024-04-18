Alure
=====
[![AppVeyor][AppVeyor Badge]][AppVeyor URL]
[![Travis][Travis Badge]][Travis URL]

[AppVeyor Badge]: https://ci.appveyor.com/api/projects/status/github/kcat/alure?branch=master&svg=true
[AppVeyor URL]: https://ci.appveyor.com/project/ChrisRobinson/alure/branch/master
[Travis Badge]: https://api.travis-ci.org/kcat/alure.svg
[Travis URL]: https://travis-ci.org/kcat/alure

Alure is a C++ 3D audio API. It uses OpenAL for audio rendering, and provides
common higher-level features such as file loading and decoding, buffer caching,
background streaming, and source management for virtually unlimited sound
source handles.

Features
--------
Alure supports 3D sound rendering based on standard OpenAL features. Extra 3D
sound features may be available depending on the extensions supported by OpenAL
(newer versions of OpenAL Soft provide the necessary extensions and is thus
recommended, but is not required if those features aren't of interest).

Environmental audio effects are supported with the ALC_EXT_EFX extension. This
provides multi-zone reverb, sound occlusion and obstruction (simulating sounds
being behind doors or walls), and atmospheric air absorption.

Binaural (HRTF) rendering is provided with the ALC_SOFT_HRTF extension. When
used with headphones, this provides an unparalleled sense of 3D sound
positioning. Multiple and custom profiles can also be select from to get a
closer match for different people.

Alure supports decoding audio files using external libraries: VorbisFile,
OpusFile, and libsndfile. A built-in wave file reader is also available to
support basic PCM formats, as well as built-in decoders for FLAC (dr_flac) and
MP3 (minimp3). Application-defined decoders are also supported in case the
default set are insufficient.

And much more…

Building
--------
### Dependencies
Before even building, Alure requires the OpenAL development files installed,
for example, through Creative's OpenAL SDK (available from openal.org) or from
OpenAL Soft. Additionally you will need a C++14 or above compliant compiler to
be able to build Alure.

These following dependencies are only needed to *automatically* support the
formats they handle:

* [ogg](https://xiph.org/ogg/): ogg playback
* [vorbis](https://xiph.org/vorbis/): ogg vorbis playback
* [opusfile](http://opus-codec.org/): opus playback
* [SndFile](http://www.mega-nerd.com/libsndfile/): various multi-format playback

Two of the packaged examples require the following dependencies to be built.

* [PhysFS](https://icculus.org/physfs/): alure-physfs
* [dumb](https://github.com/kode54/dumb): alure-dumb

If any dependency isn't found at build time the relevant decoders or examples
will be disabled and skipped during build.

### On Windows
If your are using [MinGW-w64](https://mingw-w64.org/doku.php), the easiest way
to get all of the dependencies above is to use [MSYS2](http://www.msys2.org/),
which has up-to-date binaries for all of the optional and required dependencies
above (so you don't need to build each from scratch).

Follow the MSYS2 installation guide and then look for each dependency on MSYS2
package repo and `pacman -S [packagename]` each package to acquire all
dependencies.

After acquiring all dependencies, you will need to make sure that the includes,
libraries, and binaries for each file are in your path. For most dependencies
this isn't a big deal, if you are using msys these directories are simply
`msys/mingw64/bin`, `msys/mingw64/lib` and `msys/mingw64/include`. However the
cmake file for Alure requires you to use the direct directory where OpenAL Soft
headers are located (so instead of `msys/mingw64/include`, it's
`msys/mingw64/include/AL`).

Inside the `build` directory, run `cmake ..`. After CMake generation
you should have something that looks like the following output
if you have every single dependency:

    -- Found OpenAL: C:/msys64/mingw64/lib/libopenal.dll.a
    -- Found Threads: TRUE
    -- Found OGG: C:/msys64/mingw64/lib/libogg.dll.a
    -- Found VORBIS: C:/msys64/mingw64/lib/libvorbisfile.dll.a
    -- Found OPUS: C:/msys64/mingw64/lib/libopusfile.dll.a
    -- Found SndFile: C:/msys64/mingw64/lib/libsndfile.dll.a
    -- Found PhysFS: C:/msys64/mingw64/lib/libphysfs.dll.a
    -- Found DUMB: C:/msys64/mingw64/lib/libdumb.dll.a
    -- Configuring done
    -- Generating done
    -- Build files have been written to: .../alure/build

Now you may compile the library and examples by running `cmake --build .`.
Use `cmake --install .`, which probably requires administrative privilege,
to install Alure library in `C:\Program Files (x86)` for it to be available
on your system.

### On GNU/Linux
On Debian-based systems, the library's dependencies can be installed using

    apt install libopenal-dev libvorbis-dev libopusfile-dev libsndfile1-dev

Optional dependencies for examples might be install via

    apt install libphysfs-dev libdumb1-dev

On other distributions, the packages names can be adapted similarly.

Then inside `build`, the output of `cmake ..` should contains lines similar to
the following

    -- Found OpenAL: /usr/lib/x86_64-linux-gnu/libopenal.so
    -- Found Threads: TRUE
    -- Found OGG: /usr/lib/x86_64-linux-gnu/libogg.so
    -- Found VORBIS: /usr/lib/x86_64-linux-gnu/libvorbisfile.so
    -- Found OPUS: /usr/lib/libopusfile.so
    -- Found SndFile: /usr/lib/x86_64-linux-gnu/libsndfile.so
    -- Found PhysFS: /usr/lib/x86_64-linux-gnu/libphysfs.so
    -- Found DUMB: /usr/lib/x86_64-linux-gnu/libdumb.so
    -- Configuring done
    -- Generating done
    -- Build files have been written to: .../alure/build

To build the library and each example you have the dependencies for,
run `cmake --build . --parallel $(nproc)`.  Use `sudo cmake --install .`
to install Alure library on your system.

### On macOS
OpenAL is provided by Apple via [Core Audio], while optional codecs and
examples' dependencies are available on [Homebrew](https://brew.sh/):

    brew install libvorbis opusfile libsndfile
    brew install phyfs dumb

Then inside `build`, the output of `cmake ..` should contains lines similar to
the following

    -- Found OpenAL: /System/Library/Frameworks/OpenAL.framework
    -- Found Threads: TRUE
    -- Found OGG: /usr/local/lib/libogg.dylib
    -- Found VORBIS: /usr/local/lib/libvorbisfile.dylib
    -- Found OPUS: /usr/local/lib/libopusfile.dylib
    -- Found SndFile: /usr/local/lib/libsndfile.dylib
    -- Found PhysFS: /usr/local/lib/libphysfs.dylib
    -- Found DUMB: /usr/local/lib/libdumb.a
    -- Configuring done
    -- Generating done
    -- Build files have been written to: .../alure/build

If OpenAL Soft is preferred, one may install it from Homebrew and specify
CMake prefix path accordingly

    brew install openal-soft
    OPENALDIR=`brew --prefix openal-soft` cmake -DCMAKE_FIND_FRAMEWORK=NEVER ..

and get

    -- Found OpenAL: /usr/local/opt/openal-soft/lib/libopenal.dylib

To build the library and each example you have the dependencies for,
run `cmake --build . --parallel $(sysctl -n hw.ncpu)`.
Use `sudo cmake --install .` to install Alure library on your system.

[Core Audio]: https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/CoreAudioOverview/WhatsinCoreAudio/WhatsinCoreAudio.html
