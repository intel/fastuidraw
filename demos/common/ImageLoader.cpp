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

#include <iostream>
#include <cstring>
#include "ImageLoader.hpp"

namespace
{
  fastuidraw::ivec2
  load_image_worker(SDL_Surface *img, std::vector<fastuidraw::u8vec4> &bits_data, bool flip)
  {
    SDL_PixelFormat *fmt;

    fmt = img->format;
    // Store some useful variables
    SDL_LockSurface(img);

    int w(img->w), h(img->h);
    int pitch(img->pitch);
    int bytes_per_pixel(img->format->BytesPerPixel);

    const unsigned char *surface_data;
    surface_data = reinterpret_cast<const unsigned char*>(img->pixels);

    // Resize the vector holding the bit data
    bits_data.resize(w * h);

    for(int y=0; y<h; ++y)
      {
        int source_y = (flip) ? h-1-y : y;

        for(int x =0 ; x < w; ++x)
          {
            int src_L, dest_L;
            Uint8 red, green, blue, alpha;
            Uint32 temp, pixel;

            src_L = source_y * pitch + x * bytes_per_pixel;
            dest_L = y * w + x;

            pixel = *reinterpret_cast<const Uint32*>(surface_data + src_L);

            /* Get Alpha component */
            temp = pixel & fmt->Amask;
            temp = temp >> fmt->Ashift;
            temp = temp << fmt->Aloss;
            alpha = (Uint8)temp;

            temp = pixel & fmt->Rmask;
            temp = temp >> fmt->Rshift;
            temp = temp << fmt->Rloss;
            red = temp;

            temp = pixel & fmt->Gmask;
            temp = temp >> fmt->Gshift;
            temp = temp << fmt->Gloss;
            green = temp;

            temp = pixel & fmt->Bmask;
            temp = temp >> fmt->Bshift;
            temp = temp << fmt->Bloss;
            blue = temp;

            bits_data[dest_L][0] = red;
            bits_data[dest_L][1] = green;
            bits_data[dest_L][2] = blue;
            bits_data[dest_L][3] = alpha;
          }
      }
    SDL_UnlockSurface(img);
    return fastuidraw::ivec2(w, h);
  }
}

fastuidraw::ivec2
load_image_to_array(const SDL_Surface *img,
                    std::vector<fastuidraw::u8vec4> &out_bytes,
                    bool flip)
{
  SDL_Surface *q;
  fastuidraw::ivec2 R;

  if (!img)
    {
      return fastuidraw::ivec2(0,0);
    }
  q = SDL_ConvertSurfaceFormat(const_cast<SDL_Surface*>(img), SDL_PIXELFORMAT_RGBA8888, 0);
  R = load_image_worker(q, out_bytes, flip);
  SDL_FreeSurface(q);
  return R;
}


fastuidraw::ivec2
load_image_to_array(const std::string &pfilename,
                    std::vector<fastuidraw::u8vec4> &out_bytes,
                    bool flip)
{
  fastuidraw::ivec2 R;

  SDL_Surface *img;
  img = IMG_Load(pfilename.c_str());
  R = load_image_to_array(img, out_bytes, flip);
  SDL_FreeSurface(img);
  return R;
}

void
create_mipmap_level(fastuidraw::ivec2 sz,
                    fastuidraw::c_array<const fastuidraw::u8vec4> in_data,
                    std::vector<fastuidraw::u8vec4> &out_data)
{
  int w, h;

  w = fastuidraw::t_max(1, sz.x() / 2);
  h = fastuidraw::t_max(1, sz.y() / 2);
  out_data.resize(w * h);

  for (int dst_y = 0; dst_y < h; ++dst_y)
    {
      int sy0, sy1;

      sy0 = fastuidraw::t_min(2 * dst_y, sz.y() - 1);
      sy1 = fastuidraw::t_min(2 * dst_y + 1, sz.y() - 1);
      for (int dst_x = 0; dst_x < w; ++dst_x)
        {
          int sx0, sx1;
          fastuidraw::vec4 p00, p01, p10, p11, p;

          sx0 = fastuidraw::t_min(2 * dst_x, sz.x() - 1);
          sx1 = fastuidraw::t_min(2 * dst_x + 1, sz.x() - 1);

          p00 = fastuidraw::vec4(in_data[sx0 + sy0 * sz.x()]);
          p01 = fastuidraw::vec4(in_data[sx0 + sy1 * sz.x()]);
          p10 = fastuidraw::vec4(in_data[sx1 + sy0 * sz.x()]);
          p11 = fastuidraw::vec4(in_data[sx1 + sy1 * sz.x()]);

          p = 0.25f * (p00 + p01 + p10 + p11);

          for (unsigned int c = 0; c < 4; ++c)
            {
              float q;
              q = fastuidraw::t_min(p[c], 255.0f);
              q = fastuidraw::t_max(q, 0.0f);
              out_data[dst_x + dst_y * w][c] = static_cast<unsigned int>(q);
            }
        }
    }
}

ImageLoaderData::
ImageLoaderData(const std::string &pfilename, bool flip):
  m_dimensions(0, 0)
{
  fastuidraw::ivec2 dims;
  std::vector<fastuidraw::u8vec4> data;

  dims = load_image_to_array(pfilename, data, flip);
  if (dims.x() <= 0 || dims.y() <= 0)
    {
      return;
    }

  m_dimensions = fastuidraw::uvec2(dims);
  m_mipmap_levels.push_back(std::vector<fastuidraw::u8vec4>());
  m_mipmap_levels.back().swap(data);

  while(dims.x() >= 2 || dims.y() >= 2)
    {
      fastuidraw::ivec2 wh;

      wh.x() = fastuidraw::t_max(dims.x(), 1);
      wh.y() = fastuidraw::t_max(dims.y(), 1);
      create_mipmap_level(wh,
                          cast_c_array(m_mipmap_levels.back()),
                          data);
      m_mipmap_levels.push_back(std::vector<fastuidraw::u8vec4>());
      m_mipmap_levels.back().swap(data);
      dims /= 2;
    }

  m_data_as_arrays.resize(m_mipmap_levels.size());
  for (unsigned int i = 0; i < m_data_as_arrays.size(); ++i)
    {
      m_data_as_arrays[i] = cast_c_array(m_mipmap_levels[i]);
    }
}
