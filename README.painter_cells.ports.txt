The branch, with_ports_of_painter-cells, includes ports of the painter-cells
demo. The options controlling how the demo behaved (number of cells, virtual
table size, rotations, what is rendered) are common to all the ports.

All demos when run with just -help will print to stdout all options they
support and then immediately exit.

Port specific options
-----------------------------
fastuidraw:
  - all options related to controlling how to realize the fastuidraw::Painter
    and the atlases used by the fastuidraw::Painter
  - options how to realize text (text_renderer and text_renderer_stored_pixel_size_non_scalable)
  - initial window dimensions (width and height)
  - option to control swap interval (swap_interval)

Qt:
 - option to decide to use QGLwidget or not (use_gl_widget)
 - option to initialize using glyph runs to draw text (init_use_glyph_run)
 - options coming from the Qt library
     * use -geometry WxH to control the initial window dimensions
     * use -graphicssystem raster|native|opengl to select rendering backend
       when use_gl_widget is set to false

SKIA:
  - initial window dimensions (width and height)
  - option to control swap interval (swap_interval)

Port Specific notes:
-------------------------------
fastuidraw:
  Build with:
    1. make painter-cells-GL-release
    2. make sure LD_LIBRARY_PATH includes where the .so's of fastuidraw are located

Qt:
  When there are more than a few cells, Qt benefits GREATLY from using
  QGlyphRun objects for issueing the drawing of text. Using this option
  makes the comparison more fair against Fast UI Draw because the painter-cells
  demo for Fast UI Draw performs glyph formatting only once and using QGLyphRun
  objects allows Qt to do the same. The defauls value is for qt-painter-cells
  to use QGlyphRun objects to render text.
  Build with:
    1. cd qt_painter_cells
    2. qmake
    3. make

Skia:
  Build with:
    1. cd skia_painter_cells
    2. Edit Makefile to change SKIA_DIR to where one has built SKIA.
       Makefile specifies a Git version with which the demo is known
       to work)
    3. make skia-painter-cells-release (issuing just make also builds
       debug variant)

Benchmarking Notes:
-----------------------------

The number of cells is specified by the options num_cells_x and num_cells_y which give
the number of cells in each dimension that the table has. Thus, num_cells_x 10 num_cells_y 5
specifies to have 50 cells.

The virtual table size is given by table_width and table_height. Making this smaller than
the window resolution makes the text and images to be scaled larger and making it bigger
make the text and images scaled smaller becuase the demo starts up with the table scaled
to fit within the window.

The options init_table_rotating, init_table_clipped, init_cell_rotating, init_draw_text,
init_draw_image, init_stroke_width and init_antialias_stroking initialize the demo state.

If the option num_frames is set to a positive value, the demo will run in benchmark mode.
In benchmark mode, the animation is frame based instead of timer based and the FPS is NOT
drawn to the screen. Instead, once the frames have all been drawn, the demo will write to
stdout the time for each frame and the average times. In addition, the option num_skip_frames
gives the number of frames to draw to "prime" the system. This is necessary often because
the first frame is the frame where GL resources are created (the most time consuming usually
being the creation of the GLSL program for it to be compiled by a GL driver).

Lastly, prefix commands with vblank_mode=0 for Mesa GL drivers so that vblank is disabled.

See the example script for running benchmarks (example-benchmark.sh) to get started.
