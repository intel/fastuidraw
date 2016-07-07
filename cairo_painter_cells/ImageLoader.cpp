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
  ivec2
  load_image_worker(SDL_Surface *img, std::vector<uint8_t> &bits_data, bool flip)
  {

    // Store some useful variables
    SDL_LockSurface(img);

    int w(img->w), h(img->h);
    int pitch(img->pitch);
    int bytes_per_pixel(img->format->BytesPerPixel);

    const unsigned char *surface_data;
    surface_data=reinterpret_cast<const unsigned char*>(img->pixels);

    // Resize the vector holding the bit data
    bits_data.resize(w * h * 4);

    for(int y=0; y<h; ++y)
      {
	int source_y = (flip) ? h-1-y : y;

	for(int x=0; x<w; ++x)
	  {
	    int src_L, dest_L, channel;

	    src_L = source_y * pitch + x * bytes_per_pixel;
	    dest_L = 4 * (y * w + x);

            if(img->format->BytesPerPixel == 4)
              {
                /* pre-multiply by alpha ...
                 */
                float alpha;
                alpha = static_cast<float>(surface_data[src_L + 3]) / 255.0f;
                bits_data[dest_L + 3] = surface_data[src_L + 3];
                for(int i = 0; i < 3; ++i)
                  {
                    int v;
                    float f;

                    f = static_cast<float>(surface_data[src_L + i]) / 255.0f;
                    f *= alpha;
                    v = static_cast<int>(f);
                    v = std::max(0, std::min(v, 255));
                    bits_data[dest_L + i] = static_cast<uint8_t>(v);
                  }
              }
            else
              {
                // Convert the pixel from surface data to Uint32
                for(channel = 0; channel < img->format->BytesPerPixel && channel < 4; ++channel)
                  {
                    bits_data[dest_L + channel] = surface_data[src_L + channel];
                  }

                for(; channel < 4; ++channel)
                  {
                    bits_data[dest_L + channel] = 255;
                  }
              }
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
      converted = SDL_ConvertSurface(const_cast<SDL_Surface*>(img), &tgt_fmt, 0);
      q=converted;
    }
  else
    {
      q=const_cast<SDL_Surface*>(img);
    }

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
