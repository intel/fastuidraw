#/bin/bash

common_options="fullscreen true add_image /usr/share/games/blobwars/gfx/ num_cells_x 25 num_cells_y 12 table_width 4000 table_height 2000 num_frames 100 num_skip_frames 3 init_table_rotating true init_table_clipped true init_cell_rotating true num_background_colors 100"


function run_benchmark {
    echo "vblank_mode=0 $1 $common_options $2 > $3"
    vblank_mode=0 $1 $common_options $2 > $3
}

run_benchmark qt_painter_cells/qt-painter-cells "-geometry 1920x1200" qt-results.txt
run_benchmark skia_painter_cells/skia-painter-cells-release "width 1920 height 1200" skia-results.txt
run_benchmark ./painter-cells-GL-release "width 1920 height 1200" fastuidraw-results.txt
