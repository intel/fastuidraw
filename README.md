DISCONTINUATION OF PROJECT

This project will no longer be maintained by Intel.

Intel has ceased development and contributions including, but not limited to, maintenance, bug fixes, new releases, or updates, to this project.  

Intel no longer accepts patches to this project.

If you have an ongoing need to use this project, are interested in independently developing it, or would like to maintain patches for the open source software community, please create your own fork of this project.  

Contact: webadmin@linux.intel.com
Fast UI Draw
============

Fast UI Draw is a library that provides a higher performance Canvas interface.
It is designed so that it always draws using a GPU.

In contrast to many common implementations of Canvas drawing, Fast UI Draw
has that changes in clipping are very cheap and optimized for GPU's. In
addition, Fast UI Draw has, with the GL backend, very few pipeline states.
Indeed an API trace of an application using Fast UI Draw will see only a
handful of draw calls per frame, even under high Canvas state trashing,
and high clip state changes. Indeed, for the GL backend, only one Canvas
state change invokes a pipeline state change: changing the blend mode.

In addition, Fast UI Draw gives an application the ability to make their
own shaders for custom drawing.

Documentation
=============
  Fast UI Draw uses doxygen for documentation, build and view documentation by:
  - make docs
  - xdg-open docs/html/index.html

The documentation is available online [here](https://intel.github.io/fastuidraw/docs/html/index.html).

GL requirements
=====================
  The GL backend requires GL version 3.3. To support the blend modes beyond
  those of Porter-Duff blend modes either GL_EXT_shader_framebuffer_fetch or
  GL_ARB_shader_image_load_store with one of GL_INTEL_fragment_shader_ordering,
  GL_ARB_fragment_shader_interlock or GL_NV_fragment_shader_interlock is
  required.

  The GLES backend requires GLES version 3.0. If the GLES version is 3.0 or 3.1,
  it is strongly recommended that one of the extension GL_OES_texture_buffer or
  GL_EXT_texture_buffer is present. For GLES 3.0, 3.1 and 3.2, it is strongly
  recommended for performance to have GL_APPLE_clip_distance or GL_EXT_clip_cull_distance.
  To support the blend modes beyond those of Porter-Duff blend modes either
  GL_EXT_shader_framebuffer_fetch or GLES 3.1 with GL_NV_fragment_shader_interlock
  is required. The Porter-Duff composition modes do not require any extensions though,
  but the extension GL_EXT_blend_func_extended will improve performance.

  Intel GPU's starting in IvyBridge have the extensions support for optimal
  performance with all blend modes in both GL and GLES for recent enough versions
  of Mesa. For MS-Windows, the Intel drivers for Intel GPU's also have support
  for optimal performance with all blend modes.

Building requirements
=====================
 - GNU Make
 - g++ (clang should be fine too)
 - freetype
 - flex
 - perl
 - up to date GL (and GLES) headers
   - You need
      - for GL, from https://www.opengl.org/registry/: GL/glcorearb.h, KHR/khrplatform.h
      - for GLES, from https://www.khronos.org/registry/gles/: GLES2/gl2.h, GLES2/gl2ext.h, GLES3/gl3.h, GLES3/gl31.h, GLES3/gl32.h, KHR/khrplatform.h
   - The expected place of those headers is set by setting the
     environmental variable GL_INCLUDEPATH; if the value is not set,
     the build system will guess a value. The name of the header
     files is controlled by the environmental variables
     GL_RAW_HEADER_FILES for GL and GLES_RAW_HEADER_FILES for
     GLES. If a value is not set, reasonable defaults are used. 
 - SDL 2.0 and SDL Image 2.0 (demos only)
 - doxygen (for documentation)

Building
========
  "make targets" to see all build targets and the list of environmental
  variables that control what is built and how. On MS-Windows, the helper
  library NEGL is NOT built by default and on other platforms it is.

  MacOS build is possible, but is NOT done with the GL headers that
  are included in MacOS. Instead copy the needed Khronos headers
  to /usr/local/include with the tree intact, i.e. copy GL/glcorearb.h
  from the Khronos registry to /usr/local/include/GL/glcorearb.h and
  copy KHR/khrplatform.h to /usr/local/include/KHR/khrplatform.h.
  Then the build process will work (the build system defaults
  GL_INCLUDEPATH to /usr/local/include on OS-X). Yes, this is a hack.

Running Demos
=============
  The demos (naturally) link against the FastUIDraw libraries, thus they
  need to be in the library path. For Unix platforms, this is done by
  appending the path where the libraries are located to LD_LIBRARY_PATH.
  A simple quick hack is to do `export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH`.
  All demos have options which can be see by passing `--help` as the one
  and only command line option to the demo.

Installing
==========
  Doing "make INSTALL_LOCATION=/path/to/install/to install"
  will install fastuidraw to the named path. Demos are NOT
  installed! The default value of INSTALL_LOCATION is
  /usr/local, change as one sees fit. Placement is as follows:
   - INSTALL_LOCATION/lib: libraries
   - INSTALL_LOCATION/lib/pkgconfig: pkg-config files
   - INSTALL_LOCATION/include: header files
   - INSTALL_LOCATION/bin: fastuidraw-config script (and .dll's for MinGW)
   - INSTALL_LOCATION/share/doc/fastuidraw: documentation

Using project
=============
  After installation, the script, fastuidraw-config, is available
  and copied to INSTALL_LOCATION/bin. Use the script to get
  linker and compile flags for building an application. Alternatively,
  one can also use pkg-config.

Notes
=====
  - FastUIDraw has the main meat in libFastUIDraw, there are two
    variants the release and debug version whose pkg-config module
    names are fastuidraw-release and fastuidraw-debug.
  - The GL backend of FastUIDraw is libFastUIDrawGL, there are two
    variants the release and debug version whose pkg-config module
    names are fastuidrawGL-release and fastuidrawGL-debug.
  - The GLES backend of FastUIDraw is libFastUIDrawGLES, there are two
    variants the release and debug version whose pkg-config module
    names are fastuidrawGLES-release and fastuidrawGLES-debug.
  - The debug and release versions of the libraries should not be mixed;
    if you are building for release, then use the release versions and
    the release flags. If you are building for debug use the debug
    libraries and the debug flags. One can get the flag values by
    using either pkg-config or the script fastuidraw-config.

Successfully builds under
=========================
 - Linux
 - MinGW64 and MinGW32 of MSYS2 (https://msys2.github.io/)