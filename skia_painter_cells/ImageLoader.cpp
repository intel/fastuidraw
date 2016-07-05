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
#include "ImageLoader.hpp"

namespace
{
  SkISize
  load_image_worker(SDL_Surface *img, std::vector<SkColor> &bits_data, bool flip)
  {
    // Store some useful variables
    SDL_LockSurface(img);

    int w(img->w), h(img->h);
    int pitch(img->pitch);
    int bytes_per_pixel(img->format->BytesPerPixel);

    const unsigned char *surface_data;
    surface_data=reinterpret_cast<const unsigned char*>(img->pixels);

    // Resize the vector holding the bit data
    bits_data.resize(w * h);

    for(int y=0; y<h; ++y)
      {
	int source_y = (flip) ? h-1-y : y;

	for(int x=0; x<w; ++x)
	  {
	    int src_L, dest_L, channel;
            uint8_t rgba[4];

	    src_L = source_y * pitch + x * bytes_per_pixel;
	    dest_L = y * w + x;

	    // Convert the pixel from surface data to Uint32
            for(channel = 0; channel < img->format->BytesPerPixel && channel < 4; ++channel)
              {
                rgba[channel] = surface_data[src_L + channel];
              }

            for(; channel < 4; ++channel)
              {
                rgba[channel] = 255;
              }

            bits_data[dest_L] = SkColorSetARGB(rgba[3], rgba[0], rgba[1], rgba[2]);
	  }
      }
    SDL_UnlockSurface(img);
    return SkISize::Make(w, h);
  }
}


SkISize
load_image_to_array(const SDL_Surface *img,
		    std::vector<SkColor> &out_bytes,
		    bool flip)
{

  SDL_Surface *converted(NULL);
  SDL_Surface *q;
  SkISize R;

  if(img==NULL)
    {
      return SkISize::Make(0, 0);
    }

  if( (img->format->BytesPerPixel != 4 ) || // Different bpp
      (img->format->Rshift > img->format->Bshift)) // ABGR/BGR
    {
      SDL_PixelFormat tgt_fmt;

      std::memset(&tgt_fmt, 0, sizeof(tgt_fmt));
      tgt_fmt.palette = NULL;
      tgt_fmt.BytesPerPixel = 4;
      tgt_fmt.BitsPerPixel =  tgt_fmt.BytesPerPixel * 8;

      tgt_fmt.Rshift = 0;
      tgt_fmt.Gshift = 8;
      tgt_fmt.Bshift = 16;
      tgt_fmt.Ashift = 24;
      tgt_fmt.Rmask = 0x000000FF;
      tgt_fmt.Gmask = 0x0000FF00;
      tgt_fmt.Bmask = 0x00FF0000;
      tgt_fmt.Amask = 0xFF000000;
      converted = SDL_ConvertSurface(const_cast<SDL_Surface*>(img), &tgt_fmt, SDL_SWSURFACE);
      q = converted;
    }
  else
    {
      q = const_cast<SDL_Surface*>(img);
    }

  R = load_image_worker(q, out_bytes, flip);

  if(converted!=NULL)
    {
      SDL_FreeSurface(converted);
    }

  return R;
}


SkISize
load_image_to_array(const std::string &pfilename,
		    std::vector<SkColor> &out_bytes,
		    bool flip)
{
  SkISize R;

  SDL_Surface *img;
  img = IMG_Load(pfilename.c_str());
  R = load_image_to_array(img, out_bytes, flip);
  SDL_FreeSurface(img);
  return R;
}
