#/bin/bash

#can be true or false
FULLSCREEN="true"

WIDTH="1920"
HEIGHT="1080"
WIDTH_HEIGHT="$WIDTH""x""$HEIGHT"

common_options="fullscreen $FULLSCREEN add_image /usr/share/games/blobwars/gfx/ num_cells_x 25 num_cells_y 12 table_width 4000 table_height 2000 num_frames 100 num_skip_frames 3 init_table_rotating true init_table_clipped true init_cell_rotating true num_background_colors 100"


function run_benchmark {
    echo "vblank_mode=0 $1 $common_options $2 > $3"
    vblank_mode=0 $1 $common_options $2 > $3
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
