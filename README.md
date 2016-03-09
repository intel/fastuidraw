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
  The GL backend requires GL version 3.3. The GL extension GL_ARB_texture_view is not required, but strongly recommended, this extension is core in starting in GL version 4.3.

  The GLES backend requires GLES version 3.1 with the extensions GL_OES_texture_buffer and GL_EXT_blend_func_extended OR GLES version 3.2 with the extension GL_EXT_blend_func_extended. The extensions GL_APPLE_clip_distance and GL_OES_texture_view are not required, but strongly recommended.
  

Building requirements
=====================
 - GNU Make
 - g++ (clang should be fine too)
 - boost
 - freetype
 - flex
 - up to date GL (and GLES) headers
   - You need
      - for GL, from https://www.opengl.org/registry/: GL/glcorearb.h
      - for GLES, from https://www.khronos.org/registry/gles/: GLES2/gl2.h, GLES2/gl2ext.h, GLES3/gl3.h, GLES3/gl31.h, GLES3/gl32.h, KHR/khrplatform.h
   - The expected place of those headers is hard-coded to be system
     headers in the typical location. For Linux this is /usr/include
     and for MinGW this is /mingw/include. If the headers are located
     elsewhere on your system, edit src/fastuidraw/gl_backend/ngl/Rules.mk
     and change GL_INCLUDEPATH as required by your system.
 - SDL2 (demos only)
 - doxygen (for documentation)

Building
========
  "make targets" to see all build targets.
  The following variables control what is built:
   - BUILD_GL (default value 1). If 1, build libFastUIDrawGL (and demos for GL)
   - BUILD_GLES (default value 0). If 1, build libFastUIDrawGLES (and demos for GLES)

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
  - All demos when given -help as command line display all options
  - The demos require that the libraries are in the library path

Successfully builds under
=========================
 - Linux
 - MinGW