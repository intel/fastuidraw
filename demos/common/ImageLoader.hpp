#pragma once

#include <SDL_image.h>
#include <vector>
#include <stdint.h>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/image.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/image_gl.hpp>

#include "cast_c_array.hpp"

fastuidraw::ivec2
load_image_to_array(const SDL_Surface *img,
                    std::vector<fastuidraw::u8vec4> &out_bytes,
                    bool flip = false);

fastuidraw::ivec2
load_image_to_array(const std::string &pfilename,
                    std::vector<fastuidraw::u8vec4> &out_bytes,
                    bool flip = false);

void
create_mipmap_level(fastuidraw::ivec2 sz,
                    fastuidraw::c_array<const fastuidraw::u8vec4> in_data,
                    std::vector<fastuidraw::u8vec4> &out_data);

class ImageLoaderData
{
public:
  ImageLoaderData(const std::string &pfilename, bool flip = false);

  fastuidraw::uvec2
  dimensions(void) const
  {
    return m_dimensions;
  }

  int
  width(void) const
  {
    return m_dimensions.x();
  }

  int
  height(void) const
  {
    return m_dimensions.y();
  }

  bool
  non_empty(void)
  {
    return m_dimensions.x() > 0u
      && m_dimensions.y() > 0u;
  }

  fastuidraw::c_array<const fastuidraw::c_array<const fastuidraw::u8vec4> >
  data(void) const
  {
    return cast_c_array(m_data_as_arrays);
  }

private:
  fastuidraw::uvec2 m_dimensions;
  std::vector<std::vector<fastuidraw::u8vec4> > m_mipmap_levels;
  std::vector<fastuidraw::c_array<const fastuidraw::u8vec4> > m_data_as_arrays;
};

class ImageLoader:
  public ImageLoaderData,
  public fastuidraw::ImageSourceCArray
{
public:
  explicit
  ImageLoader(const std::string &pfilename, bool flip = false):
    ImageLoaderData(pfilename, flip),
    fastuidraw::ImageSourceCArray(dimensions(), data(), fastuidraw::Image::rgba_format)
  {}
};

fastuidraw::reference_counted_ptr<fastuidraw::gl::TextureImage>
create_texture_image(const fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas> &patlas,
                     int w, int h, unsigned int m,
                     const fastuidraw::ImageSourceBase &image,
                     GLenum min_filter = GL_LINEAR_MIPMAP_LINEAR,
                     GLenum mag_filter = GL_LINEAR,
                     bool object_owns_texture = true);
