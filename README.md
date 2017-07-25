Fast UI Draw
============

Fast UI Draw in a library that provides a higher performance Canvas interface.
It is designed so that it always draws using a GPU.

In contrast to many common implementations of Canvas drawing, Fast UI Draw
has that changes in clipping are very cheap and optimized for GPU's. In
addition, Fast UI Draw has, with the GL backend, very few pipeline states.
Indeed an API trace of an application using Fast UI Draw will see only a
handful of draw calls per frame, even under high Canvas state trashing,
and high clip state changes. Indeed, for the GL backend, only one Canvas
state change invokes a pipeline state change: changing the Porter-Duff blend
mode.

In addition, Fast UI Draw gives an application the ability to make their
own shaders for custom drawing.

Documentation
=============
  Fast UI Draw uses doxygen for documentation, build and view documentation by:
  - make docs
  - xdg-open docs/doxy/html/index.html

GL requirements
=====================
  The GL backend requires GL version 3.3. The GL extension GL_ARB_texture_view
  is not required but strongly recommended; this extension is core starting in
  GL version 4.3.

  The GLES backend requires GLES version 3.0. If the GLES version is 3.0 or 3.1,
  it is strongly recommended that one of the extension GL_OES_texture_buffer or
  GL_EXT_texture_buffer is present. For GLES 3.0, 3.1 and 3.2, the extensions
  GL_EXT_blend_func_extended, GL_OES_texture_view and (GL_APPLE_clip_distance
  or GL_EXT_clip_cull_distance) are not required but strongly recommended.

Building requirements
=====================
 - GNU Make
 - g++ (clang should be fine too)
 - freetype
 - flex
 - perl
 - up to date GL (and GLES) headers
   - You need
      - for GL, from https://www.opengl.org/registry/: GL/glcorearb.h
      - for GLES, from https://www.khronos.org/registry/gles/: GLES2/gl2.h, GLES2/gl2ext.h, GLES3/gl3.h, GLES3/gl31.h, GLES3/gl32.h, KHR/khrplatform.h
   - The expected place of those headers is set by setting the
     environmental variable GL_INCLUDEPATH; if the value is not set,
     the build system will guess a value. The name of the header
     files is controlled by the environmental variables
     GL_RAW_HEADER_FILES for GL and GLES_RAW_HEADER_FILES for
     GLES. If a value is not set, reasonable defaults are used. 
 - SDL2 (demos only)
 - doxygen (for documentation)

Building
========
  "make targets" to see all build targets and the list of environmental
  variables that control what is built and how.

Installing
==========
  Doing "make INSTALL_LOCATION=/path/to/install/to install"
  will install fastuidraw to the named path. Demos are NOT
  installed! The default value of INSTALL_LOCATION is
  /usr/local, change as one sees fit. Placement is as follows:
   - INSTALL_LOCATION/lib: libraries
   - INSTALL_LOCATION/include: header files
   - INSTALL_LOCATION/bin: fastuidraw-config script (and .dll's for MinGW)
   - INSTALL_LOCATION/share/doc/fastuidraw: documentation


Using project
=============
  After installation, the script, fastuidraw-config, is available
  and copied to INSTALL_LOCATION/bin. Use the script to get
  linker and compile flags for building an application.

Notes
=====
  - FastUIDraw has the main meat in libFastUIDraw.
  - The GL backend is libFastUIDrawGL
  - The GLES backend is libFastUIDrawGLES
  - The debug and release versions of the libraries should not be mixed;
    if you are building for release, then use the release versions and
    the release flags. If you are building for debug use the debug
    libraries and the debug flags. These values are outputted by
    fastuidraw-config.
       -- The release flags include -DNDEBUG to turn off assert.
          A number of the template classes defined in fastuidraw/util
          have calls to assert(). These calls perform bounds checking,
          checking for nullptr and significantly drop performance.
  - All demos when given -help as command line display all options
  - The demos require that the libraries are in the library path

Successfully builds under
=========================
 - Linux
 - MinGW64 and MinGW32 of MSYS2 (https://msys2.github.io/)