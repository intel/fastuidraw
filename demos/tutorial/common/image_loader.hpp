/*!
 * \file image_loader.hpp
 * \brief file image_loader.hpp
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

#pragma once

//! [ExampleImage]

#include <vector>
#include <fastuidraw/image.hpp>
#include <SDL_image.h>

/* An example implementation of fastuidraw::ImageSourceBase that
 * reads image data from an SDL_Surface.
 */
class ImageSourceSDL:public fastuidraw::ImageSourceBase
{
public:
  /* Ctor from an SDL_Surface. After construction, the
   * ImagerSourceSDL -owns- the SDL_Surface and will free
   * it at its dtor.
   */
  explicit
  ImageSourceSDL(SDL_Surface *surface);

  /* Ctor. Will load the image from the named file using SDL_image2. */
  explicit
  ImageSourceSDL(fastuidraw::c_string filename);

  ~ImageSourceSDL();

  int
  width(void) const;

  int
  height(void) const;

  virtual
  bool
  all_same_color(fastuidraw::ivec2 location, int square_size,
                 fastuidraw::u8vec4 *dst) const override;

  virtual
  unsigned int
  number_levels(void) const override;

  virtual
  void
  fetch_texels(unsigned int level, fastuidraw::ivec2 location,
               unsigned int w, unsigned int h,
               fastuidraw::c_array<fastuidraw::u8vec4> dst) const override;

  virtual
  enum fastuidraw::Image::format_t
  format(void) const override;

private:

  /* Dead simple class that stores pixels in a linear array. */
  class PerMipmapLevel
  {
  public:
    PerMipmapLevel(int w, int h):
      m_pixels(w * h),
      m_width(w),
      m_height(h),
      m_stride(w)
    {}

    /* use C++ move semantics to avoid potentially
     * expensive copy of pixel data.
     */
    PerMipmapLevel(PerMipmapLevel &&src) noexcept:
      m_pixels(std::move(src.m_pixels)),
      m_width(src.m_width),
      m_height(src.m_height),
      m_stride(src.m_stride)
    {}

    PerMipmapLevel(const PerMipmapLevel &) = delete;

    fastuidraw::u8vec4&
    pixel(int x, int y)
    {
      clamp(x, y);
      return m_pixels[x + y * m_stride];
    }

    fastuidraw::u8vec4
    pixel(int x, int y) const
    {
      clamp(x, y);
      return m_pixels[x + y * m_stride];
    }

    int
    width(void) const
    {
      return m_width;
    }

    int
    height(void) const
    {
      return m_height;
    }

  private:
    void
    clamp(int &x, int &y) const
    {
      x = fastuidraw::t_max(0, fastuidraw::t_min(x, m_width - 1));
      y = fastuidraw::t_max(0, fastuidraw::t_min(y, m_height - 1));
    }

    std::vector<fastuidraw::u8vec4> m_pixels;
    int m_width, m_height, m_stride;
  };

  void
  extract_image_data_from_surface(SDL_Surface *surface);

  std::vector<PerMipmapLevel> m_image_data;
};

//! [ExampleImage]
