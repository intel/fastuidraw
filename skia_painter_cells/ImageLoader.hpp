#pragma once

#include <SDL_image.h>
#include <vector>
#include <stdint.h>
#include <string>
#include "SkSize.h"
#include "SkColor.h"

SkISize
load_image_to_array(const SDL_Surface *img,
		    std::vector<SkColor> &out_bytes,
		    bool flip=false);

SkISize
load_image_to_array(const std::string &pfilename,
		    std::vector<SkColor> &out_bytes,
		    bool flip=false);
