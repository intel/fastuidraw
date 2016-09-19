#pragma once

#include <istream>
#include <vector>
#include <fastuidraw/painter/painter_dashed_stroke_params.hpp>

float
read_dash_pattern(std::vector<fastuidraw::PainterDashedStrokeParams::DashPatternElement> &pattern_out,
                  std::istream &input_stream);
