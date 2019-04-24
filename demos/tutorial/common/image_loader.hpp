/*!
 * \file image_loader.hpp
 * \brief file image_loader.hpp
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

#pragma once

//! [ExampleImage]

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
  SDL_Surface *m_surface;
};

//! [ExampleImage]
