/*!
 * \file image_loader.cpp
 * \brief file image_loader.cpp
 *
 * Copyright 2019 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 */

//! [ExampleImage]

#include <iostream>
#include "image_loader.hpp"

static
inline
fastuidraw::u8vec4
GetRGBA(Uint32 pixel, const SDL_PixelFormat *fmt)
{
  Uint8 r, g, b, a;
  SDL_GetRGBA(pixel, fmt, &r, &g, &b, &a);
  return fastuidraw::u8vec4(r, g, b, a);
}

static
inline
fastuidraw::u8vec4
GetRGBA(const SDL_Surface *surface, int x, int y)
{
  const unsigned char *surface_data;
  Uint32 pixel, surface_offset;

  surface_data = reinterpret_cast<const unsigned char*>(surface->pixels);
  surface_offset = y * surface->pitch + x * surface->format->BytesPerPixel;
  pixel = *reinterpret_cast<const Uint32*>(surface_data + surface_offset);
  return GetRGBA(pixel, surface->format);
}

ImageSourceSDL::
ImageSourceSDL(SDL_Surface *surface)
{
  extract_image_data_from_surface(surface);
}

ImageSourceSDL::
ImageSourceSDL(fastuidraw::c_string filename)
{
  SDL_Surface *surface;

  surface = IMG_Load(filename);
  extract_image_data_from_surface(surface);
}

ImageSourceSDL::
~ImageSourceSDL()
{
}

void
ImageSourceSDL::
extract_image_data_from_surface(SDL_Surface *surface)
{
  if (!surface || surface->w == 0 || surface->h == 0)
    {
      std::cerr << "Warning: unable to load image, substituting with an "
                << "image with width and height 1 whose only pixel is "
                << "(255, 255, 0, 255)\n";
      m_image_data.push_back(PerMipmapLevel(1, 1));
      m_image_data.back().pixel(0, 0) = fastuidraw::u8vec4(255, 255, 0, 255);
      return;
    }

  /* First extract the image draw from surface, copying the pixel
   * values to m_image_data[0]
   */
  m_image_data.push_back(std::move(PerMipmapLevel(surface->w, surface->h)));

  SDL_LockSurface(surface);
  for (int y = 0; y < m_image_data.back().height(); ++y)
    {
      for (int x = 0; x < m_image_data.back().width(); ++x)
        {
          m_image_data.back().pixel(x, y) = GetRGBA(surface, x, y);
        }
    }
  SDL_UnlockSurface(surface);
  SDL_FreeSurface(surface);

  /* Now generate the mipmaps iteratively by applying a simple box-filter
   * to the previous level to generate the next level
   */
  while (m_image_data.back().width() > 1 && m_image_data.back().height() > 1)
    {
      int dst_w(m_image_data.back().width() / 2);
      int dst_h(m_image_data.back().height() / 2);

      m_image_data.push_back(std::move(PerMipmapLevel(dst_w, dst_h)));
      const PerMipmapLevel &prev(m_image_data[m_image_data.size() - 2]);

      for (int y = 0; y < dst_h; ++y)
        {
          for (int x = 0; x < dst_w; ++x)
            {
              fastuidraw::vec4 p00, p10, p01, p11, avg;
              int src_x(2 * x), src_y(2 * y);

              p00 = fastuidraw::vec4(prev.pixel(src_x + 0, src_y + 0));
              p10 = fastuidraw::vec4(prev.pixel(src_x + 1, src_y + 0));
              p01 = fastuidraw::vec4(prev.pixel(src_x + 0, src_y + 1));
              p11 = fastuidraw::vec4(prev.pixel(src_x + 1, src_y + 1));
              avg = 0.25f * (p00 + p10 + p10 + p11);
              m_image_data.back().pixel(x, y) = fastuidraw::u8vec4(avg);
            }
        }
    }
}

int
ImageSourceSDL::
width(void) const
{
  return m_image_data.front().width();
}

int
ImageSourceSDL::
height(void) const
{
  return m_image_data.front().height();
}

unsigned int
ImageSourceSDL::
number_levels(void) const
{
  /* fastuidraw::Image objects support mipmapping and the number
   * of LOD's is specified by the fastuidraw::ImageSourceBase
   * that constructs them.
   */
  FASTUIDRAWassert(!m_image_data.empty());
  return m_image_data.size();
}

enum fastuidraw::Image::format_t
ImageSourceSDL::
format(void) const
{
  return fastuidraw::Image::rgba_format;
}

void
ImageSourceSDL::
fetch_texels(unsigned int level, fastuidraw::ivec2 location,
             unsigned int w, unsigned int h,
             fastuidraw::c_array<fastuidraw::u8vec4> dst) const
{
  /* This is the function that provides pixel data; in this simple
   * example code, we just copy the data from m_image_data
   */
  FASTUIDRAWassert(level < m_image_data.size());
  for (int src_y = location.y(), dst_y = 0; dst_y < h; ++dst_y, ++src_y)
    {
      for (int src_x = location.x(), dst_x = 0; dst_x < w; ++dst_x, ++src_x)
        {
          int dst_offset;

          dst_offset = dst_y * w + dst_x;
          dst[dst_offset] = m_image_data[level].pixel(src_x, src_y);
        }
    }
}

bool
ImageSourceSDL::
all_same_color(fastuidraw::ivec2 location, int square_size,
               fastuidraw::u8vec4 *dst) const
{
  bool return_value(true);
  /* fastuidraw::Image has an optimization where if a multiple tiles
   * of an image are all the same constant color, the tile is used
   * multiple times instead of being in memory multiple times; this
   * call back function is what ImageAtlas uses to decide if a region
   * all has all pixels as the same color value.
   */

  *dst = m_image_data.front().pixel(location.x(), location.y());
  for (int src_y = location.y(), dst_y = 0; return_value && dst_y < square_size; ++dst_y, ++src_y)
    {
      for (int src_x = location.x(), dst_x = 0; return_value && dst_x < square_size; ++dst_x, ++src_x)
        {
          if (*dst != m_image_data.front().pixel(src_x, src_y))
            {
              return_value = false;
            }
        }
    }
  return return_value;
}

//! [ExampleImage]
