/*!
 * Adapted from: WRATHSDLImageSupport.cpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 */

#include <cstring>
#include <algorithm>
#include <assert.h>
#include "ImageLoader.hpp"

namespace
{
  inline
  Uint8
  pre_multiply_alpha(Uint8 alpha, Uint8 p)
  {
    float fa, fp;
    fa = static_cast<float>(alpha) / 255.0;
    fp = static_cast<float>(p);
    fp *= fa;
    return std::min(Uint32(255), static_cast<Uint32>(fp));
  }

  ivec2
  load_image_worker(SDL_Surface *img, std::vector<uint8_t> &bits_data, bool flip)
  {
    SDL_PixelFormat *fmt;

    fmt = img->format;
    // Store some useful variables
    SDL_LockSurface(img);

    int w(img->w), h(img->h);
    int pitch(img->pitch);
    int bytes_per_pixel(img->format->BytesPerPixel);

    const unsigned char *surface_data;
    surface_data=reinterpret_cast<const unsigned char*>(img->pixels);

    // Resize the vector holding the bit data
    bits_data.resize(w * h * 4);
    assert(img->format->BytesPerPixel == 4);

    for(int y=0; y<h; ++y)
      {
	int source_y = (flip) ? h-1-y : y;

	for(int x=0; x<w; ++x)
	  {
	    int src_L, dest_L;
            Uint8 red, green, blue, alpha;
            Uint32 temp, pixel;

	    src_L = source_y * pitch + x * bytes_per_pixel;
	    dest_L = 4 * (y * w + x);

            pixel = *reinterpret_cast<const Uint32*>(surface_data + src_L);

            /* Get Alpha component */
            temp = pixel & fmt->Amask;
            temp = temp >> fmt->Ashift;
            temp = temp << fmt->Aloss;
            alpha = (Uint8)temp;

            temp = pixel & fmt->Rmask;
            temp = temp >> fmt->Rshift;
            temp = temp << fmt->Rloss;
            red = pre_multiply_alpha(alpha, (Uint8)temp);

            temp = pixel & fmt->Gmask;
            temp = temp >> fmt->Gshift;
            temp = temp << fmt->Gloss;
            green = pre_multiply_alpha(alpha, (Uint8)temp);

            temp = pixel & fmt->Bmask;
            temp = temp >> fmt->Bshift;
            temp = temp << fmt->Bloss;
            blue = pre_multiply_alpha(alpha, (Uint8)temp);

            bits_data[dest_L + 3] = alpha;
            bits_data[dest_L + 2] = red;
            bits_data[dest_L + 1] = green;
            bits_data[dest_L + 0] = blue;
	  }
      }
    SDL_UnlockSurface(img);
    return ivec2(w, h);
  }
}


ivec2
load_image_to_array(const SDL_Surface *img,
		    std::vector<uint8_t> &out_bytes,
		    bool flip)
{

  SDL_Surface *converted(NULL);
  SDL_Surface *q;
  ivec2 R;

  if(img==NULL)
    {
      return ivec2(0,0);
    }

  converted = SDL_ConvertSurfaceFormat(const_cast<SDL_Surface*>(img), SDL_PIXELFORMAT_ARGB8888, 0);
  q=converted;

  R=load_image_worker(q, out_bytes, flip);

  if(converted!=NULL)
    {
      SDL_FreeSurface(converted);
    }

  return R;
}


ivec2
load_image_to_array(const std::string &pfilename,
		    std::vector<unsigned char> &out_bytes,
		    bool flip)
{
  ivec2 R;

  SDL_Surface *img;
  img=IMG_Load(pfilename.c_str());
  R=load_image_to_array(img, out_bytes, flip);
  SDL_FreeSurface(img);
  return R;
}

cairo_surface_t*
create_image_from_array(const std::vector<uint8_t> &in_bytes,
                        ivec2 dimensions)
{
  cairo_surface_t *tmp, *R;
  cairo_t *cr;

  if(in_bytes.empty())
    {
      return NULL;
    }

  assert(in_bytes.size() == 4 * dimensions.x() * dimensions.y());
  tmp = cairo_image_surface_create_for_data(const_cast<uint8_t*>(&in_bytes[0]),
                                            CAIRO_FORMAT_ARGB32,
                                            dimensions.x(), dimensions.y(),
                                            dimensions.x() * 4);

  /* The actual image we return is so that Cairo choosed the pitch
     to let it optimize its drawing.
   */
  R = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                 dimensions.x(), dimensions.y());
  cr = cairo_create(R);
  cairo_set_source_surface(cr, tmp, 0, 0);
  cairo_paint(cr);
  cairo_surface_flush(R);
  cairo_destroy(cr);

  cairo_surface_destroy(tmp);
  return R;
}

cairo_surface_t*
create_image_from_sdl_surface(const SDL_Surface *img,
                              bool flip)
{
  if(!img)
    {
      return NULL;
    }

  std::vector<uint8_t> tmp;
  load_image_to_array(img, tmp, flip);
  return create_image_from_array(tmp, ivec2(img->w, img->h));
}


cairo_surface_t*
create_image_from_file(const std::string &pfilename,
                       bool flip)
{
  std::vector<uint8_t> tmp;
  ivec2 dims;

  dims = load_image_to_array(pfilename, tmp, flip);
  return create_image_from_array(tmp, dims);
}
