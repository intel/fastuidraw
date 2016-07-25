#/bin/bash

# destination directory to place results
RESULTS="."

# how many frames to run the benchmark
NUM_FRAMES="100"

#can be true or false
FULLSCREEN="true"

#screen/window resolution
WIDTH="1920"
HEIGHT="1080"

# 0 means tell SDL (or Qt) to disable vsync
SWAP_INTERVAL="0"

# number of cells in each dimension to draw
NUM_CELLS_X="25"
NUM_CELLS_Y="12"

# virtual size of table, display is zoomed out to show
# entire table.
TABLE_WIDTH="4000"
TABLE_HEIGHT="2000"


# for Qt's -geometry bits
WIDTH_HEIGHT="$WIDTH""x""$HEIGHT"

common_options="fullscreen $FULLSCREEN swap_interval $SWAP_INTERVAL add_image /usr/share/games/blobwars/gfx/ num_cells_x $NUM_CELLS_X num_cells_y $NUM_CELLS_Y table_width $TABLE_WIDTH table_height $TABLE_HEIGHT num_frames $NUM_FRAMES num_skip_frames 3 init_table_rotating true init_table_clipped true init_cell_rotating true num_background_colors 100"


function run_benchmark {
    echo "vblank_mode=0 $1 $common_options $2 > $RESULTS/$3"
    vblank_mode=0 $1 $common_options $2 > $RESULTS/$3
}

run_benchmark qt_painter_cells/qt-painter-cells "-geometry $WIDTH_HEIGHT" qt-glwidget-results.txt
run_benchmark qt_painter_cells/qt-painter-cells "-geometry $WIDTH_HEIGHT use_gl_widget false -graphicssystem raster" qt-nonglwidget-raster-results.txt
run_benchmark qt_painter_cells/qt-painter-cells "-geometry $WIDTH_HEIGHT use_gl_widget false -graphicssystem native" qt-nonglwidget-native-results.txt
run_benchmark qt_painter_cells/qt-painter-cells "-geometry $WIDTH_HEIGHT use_gl_widget false -graphicssystem opengl" qt-nonglwidget-gl-results.txt
run_benchmark cairo_painter_cells/cairo-painter-cells-release "width $WIDTH height $HEIGHT cairo_backend xlib_offscreen" cairo-x-offscreen-results.txt
run_benchmark cairo_painter_cells/cairo-painter-cells-release "width $WIDTH height $HEIGHT cairo_backend xlib_onscreen" cairo-x-onscreen-results.txt
run_benchmark cairo_painter_cells/cairo-painter-cells-release "width $WIDTH height $HEIGHT cairo_backend offscreen_data" cairo-cpu-offscreen-results.txt
run_benchmark cairo_painter_cells/cairo-painter-cells-release "width $WIDTH height $HEIGHT cairo_backend gl" cairo-gl-results.txt
run_benchmark skia_painter_cells/skia-painter-cells-release "width $WIDTH height $HEIGHT" skia-results.txt
run_benchmark ./painter-cells-GL-release "width $WIDTH height $HEIGHT" fastuidraw-results.txt
