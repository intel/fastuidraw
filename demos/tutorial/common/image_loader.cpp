/*!
 * \file image_loader.cpp
 * \brief file image_loader.cpp
 *
 * Copyright 2019 by Intel.
 *
 * Contact: kevin.rogovin@intel.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 */

//! [ExampleImage]

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
ImageSourceSDL(SDL_Surface *surface):
  m_surface(surface)
{
}

ImageSourceSDL::
ImageSourceSDL(fastuidraw::c_string filename):
  m_surface(IMG_Load(filename))
{
}

ImageSourceSDL::
~ImageSourceSDL()
{
  if (m_surface)
    {
      SDL_FreeSurface(m_surface);
    }
}

int
ImageSourceSDL::
width(void) const
{
  return m_surface ? m_surface->w : 1;
}

int
ImageSourceSDL::
height(void) const
{
  return m_surface ? m_surface->h : 1;
}

unsigned int
ImageSourceSDL::
number_levels(void) const
{
  /* fastuidraw::Image objects support mipmapping and the number
   * of LOD's is specified by the fastuidraw::ImageSourceBase
   * that constructs them. In this simple example, we do not support
   * mipmapping and so the number of levels is 1.
   */
  return 1;
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
   * example code, we will rely on SDL utility functions to extract
   * RGBA values one texel at a time.
   */

  /* Because we return number_levels() as 1, the LOD level should be 0 */
  FASTUIDRAWassert(level == 0);
  FASTUIDRAWunused(level);

  /* No underlying SDL-surface, just give them black. */
  if (!m_surface)
    {
      std::fill(dst.begin(), dst.end(), fastuidraw::u8vec4(0, 0, 0, 0));
      return;
    }

  SDL_LockSurface(m_surface);
  for (int src_y = location.y(), dst_y = 0; dst_y < h; ++dst_y, ++src_y)
    {
      for (int src_x = location.x(), dst_x = 0; dst_x < w; ++dst_x, ++src_x)
        {
          int dst_offset;

          dst_offset = dst_y * w + dst_x;
          dst[dst_offset] = GetRGBA(m_surface, src_x, src_y);
        }
    }
  SDL_UnlockSurface(m_surface);
}

bool
ImageSourceSDL::
all_same_color(fastuidraw::ivec2 location, int square_size,
               fastuidraw::u8vec4 *dst) const
{
  bool return_value(true);
  /* fastuidraw::Image has an optimization where if a multiple tiles
   * of an image are all the same constant color, the tile is used
   * multiple times instead of being in memory multiple times.
   */
  if (!m_surface)
    {
      *dst = fastuidraw::u8vec4(0, 0, 0, 0);
      return return_value;
    }

  SDL_LockSurface(m_surface);
  *dst = GetRGBA(m_surface, location.x(), location.y());
  for (int src_y = location.y(), dst_y = 0; return_value && dst_y < square_size; ++dst_y, ++src_y)
    {
      for (int src_x = location.x(), dst_x = 0; return_value && dst_x < square_size; ++dst_x, ++src_x)
        {
          if (*dst != GetRGBA(m_surface, src_x, src_y))
            {
              return_value = false;
            }
        }
    }
  SDL_UnlockSurface(m_surface);
  return return_value;
}

//! [ExampleImage]
