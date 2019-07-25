/*!
 * \file bindless.hpp
 * \brief file bindless.hpp
 *
 * Copyright 2018 by Intel.
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
 *
 */


#ifndef FASTUIDRAW_BINDLESS_HPP
#define FASTUIDRAW_BINDLESS_HPP

#include <fastuidraw/gl_backend/ngl_header.hpp>

namespace fastuidraw { namespace gl { namespace detail {

class Bindless
{
public:
  Bindless(void);

  GLuint64
  get_texture_handle(GLuint tex) const;

  void
  make_texture_handle_resident(GLuint64 h) const;

  void
  make_texture_handle_non_resident(GLuint64 h) const;

  bool
  not_supported(void) const
  {
    return m_type == no_bindless_texture;
  }

private:
  enum type_t
    {
      no_bindless_texture,
      arb_bindless_texture,
      nv_bindless_texture,
    };

  type_t m_type;
};

const Bindless&
bindless(void);

} //namespace detail
} //namespace gl
} //namespace fastuidraw

#endif
