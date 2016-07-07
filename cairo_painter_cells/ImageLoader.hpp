#pragma once


#include <vector>
#include <string>
#include <stdint.h>
#include <cairo.h>
#include <SDL_image.h>
#include "vec2.hpp"

// format is ARGB
// stride of returned image is width * height * 4
// with alpha pre-multiplied already.

ivec2
load_image_to_array(const std::string &pfilename,
		    std::vector<uint8_t> &out_bytes,
		    bool flip=false);

ivec2
load_image_to_array(const SDL_Surface *img,
		    std::vector<uint8_t> &out_bytes,
		    bool flip=false);

cairo_surface_t*
create_image_from_array(const std::vector<uint8_t> &out_bytes,
                        ivec2 dimensions);

cairo_surface_t*
create_image_from_sdl_surface(const SDL_Surface *img,
                              bool flip=false);

cairo_surface_t*
create_image_from_file(const std::string &pfilename,
                       bool flip=false);
