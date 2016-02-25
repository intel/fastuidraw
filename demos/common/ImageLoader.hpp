#pragma once

#include <SDL_image.h>
#include <vector>
#include <stdint.h>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/image.hpp>

#include "cast_c_array.hpp"

fastuidraw::ivec2
load_image_to_array(const SDL_Surface *img,
		    std::vector<fastuidraw::u8vec4> &out_bytes,
		    bool flip=false);

fastuidraw::ivec2
load_image_to_array(const std::string &pfilename,
		    std::vector<fastuidraw::u8vec4> &out_bytes,
		    bool flip=false);
