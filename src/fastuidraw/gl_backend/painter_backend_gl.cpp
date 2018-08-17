/*!
 * \file painter_backend_gl.cpp
 * \brief file painter_backend_gl.cpp
 *
 * Copyright 2016 by Intel.
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
 *
 */


#include <list>
#include <map>
#include <sstream>
#include <vector>
#include <iostream>

#include <fastuidraw/gl_backend/painter_backend_gl.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>
#include <fastuidraw/gl_backend/opengl_trait.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>

#include "../private/util_private.hpp"
#include "private/tex_buffer.hpp"
#include "private/texture_gl.hpp"
#include "private/painter_backend_gl_config.hpp"
#include "private/painter_vao_pool.hpp"
#include "private/painter_shader_registrar_gl.hpp"
#include "private/painter_surface_gl_private.hpp"

#ifdef FASTUIDRAW_GL_USE_GLES
#define GL_SRC1_COLOR GL_SRC1_COLOR_EXT
#define GL_SRC1_ALPHA GL_SRC1_ALPHA_EXT
#define GL_ONE_MINUS_SRC1_COLOR GL_ONE_MINUS_SRC1_COLOR_EXT
#define GL_ONE_MINUS_SRC1_ALPHA GL_ONE_MINUS_SRC1_ALPHA_EXT
#define GL_CLIP_DISTANCE0 GL_CLIP_DISTANCE0_EXT
#endif

namespace
{
  class ImageBarrier:public fastuidraw::PainterDraw::Action
  {
  public:
    virtual
    fastuidraw::gpu_dirty_state
    execute(fastuidraw::PainterDraw::APIBase*) const
    {
      glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
      return fastuidraw::gpu_dirty_state();
    }
  };

  class ImageBarrierByRegion:public fastuidraw::PainterDraw::Action
  {
  public:
    virtual
    fastuidraw::gpu_dirty_state
    execute(fastuidraw::PainterDraw::APIBase*) const
    {
      glMemoryBarrierByRegion(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
      return fastuidraw::gpu_dirty_state();
    }
  };

  class PainterBackendGLPrivate
  {
  public:
    explicit
    PainterBackendGLPrivate(fastuidraw::gl::PainterBackendGL *p);

    ~PainterBackendGLPrivate();

    static
    void
    compute_uber_shader_params(const fastuidraw::gl::PainterBackendGL::ConfigurationGL &P,
                               const fastuidraw::gl::ContextProperties &ctx,
                               fastuidraw::gl::PainterBackendGL::UberShaderParams &out_value,
                               fastuidraw::PainterShaderSet &out_shaders);

    void
    set_gl_state(fastuidraw::gpu_dirty_state v,
                 bool clear_depth, bool clear_color);

    fastuidraw::reference_counted_ptr<fastuidraw::gl::detail::PainterShaderRegistrarGL> m_reg_gl;

    GLuint m_nearest_filter_sampler;
    fastuidraw::gl::detail::painter_vao_pool *m_pool;
    fastuidraw::gl::detail::SurfaceGLPrivate *m_surface_gl;
    bool m_uniform_ubo_ready;
    fastuidraw::gl::detail::PainterShaderRegistrarGL::program_set m_cached_programs;
    GLuint m_current_external_texture;

    fastuidraw::gl::PainterBackendGL *m_p;
  };

  class TextureImageBindAction:public fastuidraw::PainterDraw::Action
  {
  public:
    typedef fastuidraw::gl::ImageAtlasGL::TextureImage TextureImage;

    TextureImageBindAction(const fastuidraw::reference_counted_ptr<const fastuidraw::Image> &im,
                           PainterBackendGLPrivate *p):
      m_p(p),
      m_texture_unit(p->m_reg_gl->uber_shader_builder_params().binding_points().external_texture())
    {
      FASTUIDRAWassert(im);
      FASTUIDRAWassert(im.dynamic_cast_ptr<const TextureImage>());
      m_image = im.static_cast_ptr<const TextureImage>();
    }

    virtual
    fastuidraw::gpu_dirty_state
    execute(fastuidraw::PainterDraw::APIBase*) const
    {
      glActiveTexture(GL_TEXTURE0 + m_texture_unit);
      glBindTexture(GL_TEXTURE_2D, m_image->texture());

      /* if the user makes an action that affects texture
       * unit m_texture_unit, we need to give the backend
       * the knowledge of what is the external texture.
       */
      m_p->m_current_external_texture = m_image->texture();

      /* we do not regard changing the texture unit
       * as changing the GPU texture state because the
       * restore of GL state would be all those texture
       * states we did not change
       */
      return fastuidraw::gpu_dirty_state();
    }

  private:
    fastuidraw::reference_counted_ptr<const TextureImage> m_image;
    PainterBackendGLPrivate *m_p;
    unsigned int m_texture_unit;
  };

  class DrawState
  {
  public:
    explicit
    DrawState(fastuidraw::gl::Program *pr):
      m_current_program(pr),
      m_current_blend_mode(nullptr)
    {}

    void
    restore_gl_state(const fastuidraw::gl::detail::painter_vao &vao,
                     PainterBackendGLPrivate *pr,
                     fastuidraw::gpu_dirty_state flags) const;

    fastuidraw::gl::Program *m_current_program;
    const fastuidraw::BlendMode *m_current_blend_mode;

  private:
    static
    GLenum
    convert_blend_op(enum fastuidraw::BlendMode::op_t v);

    static
    GLenum
    convert_blend_func(enum fastuidraw::BlendMode::func_t v);
  };

  class DrawEntry
  {
  public:
    DrawEntry(const fastuidraw::BlendMode &mode,
              PainterBackendGLPrivate *pr,
              unsigned int pz);

    DrawEntry(const fastuidraw::BlendMode &mode);

    DrawEntry(const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> &action);

    void
    add_entry(GLsizei count, const void *offset);

    void
    draw(PainterBackendGLPrivate *pr, const fastuidraw::gl::detail::painter_vao &vao,
         DrawState &st) const;

  private:
    bool m_set_blend;
    fastuidraw::BlendMode m_blend_mode;
    fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> m_action;

    std::vector<GLsizei> m_counts;
    std::vector<const GLvoid*> m_indices;
    fastuidraw::gl::Program *m_new_program;
  };

  class DrawCommand:public fastuidraw::PainterDraw
  {
  public:
    explicit
    DrawCommand(fastuidraw::gl::detail::painter_vao_pool *hnd,
                const fastuidraw::gl::PainterBackendGL::ConfigurationGL &params,
                PainterBackendGLPrivate *pr);

    virtual
    ~DrawCommand()
    {}

    virtual
    void
    draw_break(const fastuidraw::PainterShaderGroup &old_shaders,
               const fastuidraw::PainterShaderGroup &new_shaders,
               unsigned int indices_written) const;

    virtual
    void
    draw_break(const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> &action,
               unsigned int indices_written) const;

    virtual
    void
    draw(void) const;

  protected:

    virtual
    void
    unmap_implement(unsigned int attributes_written,
                    unsigned int indices_written,
                    unsigned int data_store_written) const;

  private:

    void
    add_entry(unsigned int indices_written) const;

    PainterBackendGLPrivate *m_pr;
    fastuidraw::gl::detail::painter_vao m_vao;
    mutable unsigned int m_attributes_written, m_indices_written;
    mutable std::list<DrawEntry> m_draws;
  };

  class SurfacePropertiesPrivate
  {
  public:
    SurfacePropertiesPrivate(void):
      m_dimensions(1, 1),
      m_msaa(0)
    {}

    fastuidraw::ivec2 m_dimensions;
    unsigned int m_msaa;
  };

  class ConfigurationGLPrivate
  {
  public:
    ConfigurationGLPrivate(void):
      m_alignment(4),
      m_attributes_per_buffer(512 * 512),
      m_indices_per_buffer((m_attributes_per_buffer * 6) / 4),
      m_data_blocks_per_store_buffer(1024 * 64),
      m_data_store_backing(fastuidraw::gl::PainterBackendGL::data_store_tbo),
      m_number_pools(3),
      m_break_on_shader_change(false),
      m_clipping_type(fastuidraw::gl::PainterBackendGL::clipping_via_gl_clip_distance),
      /* on Mesa/i965 using switch statement gives much slower
       * performance than using if/else chain.
       */
      m_vert_shader_use_switch(false),
      m_frag_shader_use_switch(false),
      m_composite_shader_use_switch(false),
      m_unpack_header_and_brush_in_frag_shader(false),
      m_assign_layout_to_vertex_shader_inputs(true),
      m_assign_layout_to_varyings(false),
      m_assign_binding_points(true),
      m_separate_program_for_discard(true),
      m_default_stroke_shader_aa_type(fastuidraw::PainterStrokeShader::draws_solid_then_fuzz),
      m_compositing_type(fastuidraw::gl::PainterBackendGL::compositing_dual_src),
      m_provide_auxiliary_image_buffer(fastuidraw::gl::PainterBackendGL::no_auxiliary_buffer)
    {}

    int m_alignment;
    unsigned int m_attributes_per_buffer;
    unsigned int m_indices_per_buffer;
    unsigned int m_data_blocks_per_store_buffer;
    enum fastuidraw::gl::PainterBackendGL::data_store_backing_t m_data_store_backing;
    unsigned int m_number_pools;
    bool m_break_on_shader_change;
    fastuidraw::reference_counted_ptr<fastuidraw::gl::ImageAtlasGL> m_image_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::gl::ColorStopAtlasGL> m_colorstop_atlas;
    fastuidraw::reference_counted_ptr<fastuidraw::gl::GlyphAtlasGL> m_glyph_atlas;
    enum fastuidraw::gl::PainterBackendGL::clipping_type_t m_clipping_type;
    bool m_vert_shader_use_switch;
    bool m_frag_shader_use_switch;
    bool m_composite_shader_use_switch;
    bool m_unpack_header_and_brush_in_frag_shader;
    bool m_assign_layout_to_vertex_shader_inputs;
    bool m_assign_layout_to_varyings;
    bool m_assign_binding_points;
    bool m_separate_program_for_discard;
    enum fastuidraw::PainterStrokeShader::type_t m_default_stroke_shader_aa_type;
    enum fastuidraw::gl::PainterBackendGL::compositing_type_t m_compositing_type;
    enum fastuidraw::gl::PainterBackendGL::auxiliary_buffer_t m_provide_auxiliary_image_buffer;

    std::string m_glsl_version_override;
  };

}

////////////////////////////////////////////
// DrawState methods
void
DrawState::
restore_gl_state(const fastuidraw::gl::detail::painter_vao &vao,
                 PainterBackendGLPrivate *pr,
                 fastuidraw::gpu_dirty_state flags) const
{
  using namespace fastuidraw;
  using namespace fastuidraw::gl;

  if (flags & gpu_dirty_state::shader)
    {
      FASTUIDRAWassert(m_current_program);
      m_current_program->use_program();
    }

  pr->set_gl_state(flags, false, false);

  /* If necessary, restore the UBO or TBO assoicated to the data
   * store binding point.
   */
  switch(vao.m_data_store_backing)
    {
    case PainterBackendGL::data_store_tbo:
      if (flags & gpu_dirty_state::textures)
        {
          glActiveTexture(GL_TEXTURE0 + vao.m_data_store_binding_point);
          glBindTexture(GL_TEXTURE_BUFFER, vao.m_data_tbo);
        }
      break;

    case PainterBackendGL::data_store_ubo:
      if (flags & gpu_dirty_state::constant_buffers)
        {
          glBindBufferBase(GL_UNIFORM_BUFFER, vao.m_data_store_binding_point, vao.m_data_bo);
        }
      break;

    case PainterBackendGL::data_store_ssbo:
      if (flags & gpu_dirty_state::storage_buffers)
        {
          glBindBufferBase(GL_SHADER_STORAGE_BUFFER, vao.m_data_store_binding_point, vao.m_data_bo);
        }
      break;

    default:
      FASTUIDRAWassert(!"Bad value for vao.m_data_store_backing");
    }

  if (flags & gpu_dirty_state::blend_mode)
    {
      FASTUIDRAWassert(m_current_blend_mode);
      if (m_current_blend_mode->blending_on())
        {
          glEnable(GL_BLEND);
          glBlendEquationSeparate(convert_blend_op(m_current_blend_mode->equation_rgb()),
                                  convert_blend_op(m_current_blend_mode->equation_alpha()));
          glBlendFuncSeparate(convert_blend_func(m_current_blend_mode->func_src_rgb()),
                              convert_blend_func(m_current_blend_mode->func_dst_rgb()),
                              convert_blend_func(m_current_blend_mode->func_src_alpha()),
                              convert_blend_func(m_current_blend_mode->func_dst_alpha()));
        }
      else
        {
          glDisable(GL_BLEND);
        }
    }
}

GLenum
DrawState::
convert_blend_op(enum fastuidraw::BlendMode::op_t v)
{
#define C(X) case fastuidraw::BlendMode::X: return GL_FUNC_##X
#define D(X) case fastuidraw::BlendMode::X: return GL_##X

  switch(v)
    {
      C(ADD);
      C(SUBTRACT);
      C(REVERSE_SUBTRACT);
      D(MIN);
      D(MAX);

    case fastuidraw::BlendMode::NUMBER_OPS:
    default:
      FASTUIDRAWassert(!"Bad fastuidraw::BlendMode::op_t v");
    }
#undef C
#undef D

  FASTUIDRAWassert("Invalid composite_op_t");
  return GL_INVALID_ENUM;
}

GLenum
DrawState::
convert_blend_func(enum fastuidraw::BlendMode::func_t v)
{
#define C(X) case fastuidraw::BlendMode::X: return GL_##X
  switch(v)
    {
      C(ZERO);
      C(ONE);
      C(SRC_COLOR);
      C(ONE_MINUS_SRC_COLOR);
      C(SRC_ALPHA);
      C(ONE_MINUS_SRC_ALPHA);
      C(DST_COLOR);
      C(ONE_MINUS_DST_COLOR);
      C(DST_ALPHA);
      C(ONE_MINUS_DST_ALPHA);
      C(CONSTANT_COLOR);
      C(ONE_MINUS_CONSTANT_COLOR);
      C(CONSTANT_ALPHA);
      C(ONE_MINUS_CONSTANT_ALPHA);
      C(SRC_ALPHA_SATURATE);
      C(SRC1_COLOR);
      C(ONE_MINUS_SRC1_COLOR);
      C(SRC1_ALPHA);
      C(ONE_MINUS_SRC1_ALPHA);

    case fastuidraw::BlendMode::NUMBER_FUNCS:
    default:
      FASTUIDRAWassert(!"Bad fastuidraw::BlendMode::func_t v");
    }
#undef C
  FASTUIDRAWassert("Invalid composite_t");
  return GL_INVALID_ENUM;
}

///////////////////////////////////////////////
// DrawEntry methods
DrawEntry::
DrawEntry(const fastuidraw::BlendMode &mode,
          PainterBackendGLPrivate *pr,
          unsigned int pz):
  m_set_blend(true),
  m_blend_mode(mode),
  m_new_program(pr->m_cached_programs[pz].get())
{}

DrawEntry::
DrawEntry(const fastuidraw::BlendMode &mode):
  m_set_blend(true),
  m_blend_mode(mode),
  m_new_program(nullptr)
{}

DrawEntry::
DrawEntry(const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> &action):
  m_set_blend(false),
  m_action(action),
  m_new_program(nullptr)
{}

void
DrawEntry::
add_entry(GLsizei count, const void *offset)
{
  m_counts.push_back(count);
  m_indices.push_back(offset);
}

void
DrawEntry::
draw(PainterBackendGLPrivate *pr,
     const fastuidraw::gl::detail::painter_vao &vao,
     DrawState &st) const
{
  using namespace fastuidraw;
  using namespace fastuidraw::gl;

  uint32_t flags(0);

  if (m_action)
    {
      /* Rather than having something delicate to restore
       * the currently bound VAO, instead we unbind it
       * and rebind it after the action.
       */
      glBindVertexArray(0);
      flags |= m_action->execute(nullptr);
      glBindVertexArray(vao.m_vao);
    }

  if (m_set_blend)
    {
      st.m_current_blend_mode = &m_blend_mode;
      flags |= gpu_dirty_state::blend_mode;
    }

  if (m_new_program && st.m_current_program != m_new_program)
    {
      st.m_current_program = m_new_program;
      flags |= gpu_dirty_state::shader;
    }

  st.restore_gl_state(vao, pr, flags);

  if (m_counts.empty())
    {
      return;
    }

  FASTUIDRAWassert(m_counts.size() == m_indices.size());

  #ifndef FASTUIDRAW_GL_USE_GLES
    {
      glMultiDrawElements(GL_TRIANGLES, &m_counts[0],
                          opengl_trait<PainterIndex>::type,
                          &m_indices[0], m_counts.size());
    }
  #else
    {
      if (pr->m_reg_gl->has_multi_draw_elements())
        {
          glMultiDrawElementsEXT(GL_TRIANGLES, &m_counts[0],
                                 opengl_trait<PainterIndex>::type,
                                 &m_indices[0], m_counts.size());
        }
      else
        {
          for(unsigned int i = 0, endi = m_counts.size(); i < endi; ++i)
            {
              glDrawElements(GL_TRIANGLES, m_counts[i],
                             opengl_trait<PainterIndex>::type,
                             m_indices[i]);
            }
        }
    }
  #endif
}

////////////////////////////////////
// DrawCommand methods
DrawCommand::
DrawCommand(fastuidraw::gl::detail::painter_vao_pool *hnd,
            const fastuidraw::gl::PainterBackendGL::ConfigurationGL &params,
            PainterBackendGLPrivate *pr):
  m_pr(pr),
  m_vao(hnd->request_vao()),
  m_attributes_written(0),
  m_indices_written(0)
{
  /* map the buffers and set to the c_array<> fields of
   * fastuidraw::PainterDraw to the mapping location.
   */
  void *attr_bo, *index_bo, *data_bo, *header_bo;
  uint32_t flags;

  flags = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_FLUSH_EXPLICIT_BIT;

  glBindBuffer(GL_ARRAY_BUFFER, m_vao.m_attribute_bo);
  attr_bo = glMapBufferRange(GL_ARRAY_BUFFER, 0, hnd->attribute_buffer_size(), flags);
  FASTUIDRAWassert(attr_bo != nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, m_vao.m_header_bo);
  header_bo = glMapBufferRange(GL_ARRAY_BUFFER, 0, hnd->header_buffer_size(), flags);
  FASTUIDRAWassert(header_bo != nullptr);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_vao.m_index_bo);
  index_bo = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, hnd->index_buffer_size(), flags);
  FASTUIDRAWassert(index_bo != nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, m_vao.m_data_bo);
  data_bo = glMapBufferRange(GL_ARRAY_BUFFER, 0, hnd->data_buffer_size(), flags);
  FASTUIDRAWassert(data_bo != nullptr);

  m_attributes = fastuidraw::c_array<fastuidraw::PainterAttribute>(static_cast<fastuidraw::PainterAttribute*>(attr_bo),
                                                                 params.attributes_per_buffer());
  m_indices = fastuidraw::c_array<fastuidraw::PainterIndex>(static_cast<fastuidraw::PainterIndex*>(index_bo),
                                                          params.indices_per_buffer());
  m_store = fastuidraw::c_array<fastuidraw::generic_data>(static_cast<fastuidraw::generic_data*>(data_bo),
                                                          hnd->data_buffer_size() / sizeof(fastuidraw::generic_data));

  m_header_attributes = fastuidraw::c_array<uint32_t>(static_cast<uint32_t*>(header_bo),
                                                     params.attributes_per_buffer());

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void
DrawCommand::
draw_break(const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> &action,
           unsigned int indices_written) const
{
  if (action)
    {
      if (!m_draws.empty())
        {
          add_entry(indices_written);
        }
      m_draws.push_back(action);
    }
}

void
DrawCommand::
draw_break(const fastuidraw::PainterShaderGroup &old_shaders,
           const fastuidraw::PainterShaderGroup &new_shaders,
           unsigned int indices_written) const
{
  using namespace fastuidraw;
  using namespace fastuidraw::gl;
  using namespace fastuidraw::gl::detail;

  /* if the composite mode changes, then we need to start a new DrawEntry */
  BlendMode::packed_value old_mode, new_mode;
  uint32_t new_disc, old_disc;

  old_mode = old_shaders.packed_composite_mode();
  new_mode = new_shaders.packed_composite_mode();

  old_disc = old_shaders.item_group() & PainterShaderRegistrarGL::shader_group_discard_mask;
  new_disc = new_shaders.item_group() & PainterShaderRegistrarGL::shader_group_discard_mask;

  if (old_disc != new_disc)
    {
      unsigned int pz;
      pz = (new_disc != 0u) ?
        PainterBackendGL::program_with_discard :
        PainterBackendGL::program_without_discard;

      if (!m_draws.empty())
        {
          add_entry(indices_written);
        }
      m_draws.push_back(DrawEntry(fastuidraw::BlendMode(new_mode), m_pr, pz));
    }
  else if (old_mode != new_mode)
    {
      if (!m_draws.empty())
        {
          add_entry(indices_written);
        }
      m_draws.push_back(fastuidraw::BlendMode(new_mode));
    }
  else
    {
      /* any other state changes means that we just need to add an
       * entry to the current draw entry.
       */
      add_entry(indices_written);
    }
}

void
DrawCommand::
draw(void) const
{
  using namespace fastuidraw::gl;

  glBindVertexArray(m_vao.m_vao);
  switch(m_vao.m_data_store_backing)
    {
    case PainterBackendGL::data_store_tbo:
      {
        glActiveTexture(GL_TEXTURE0 + m_vao.m_data_store_binding_point);
        glBindTexture(GL_TEXTURE_BUFFER, m_vao.m_data_tbo);
      }
      break;

    case PainterBackendGL::data_store_ubo:
      {
        glBindBufferBase(GL_UNIFORM_BUFFER, m_vao.m_data_store_binding_point, m_vao.m_data_bo);
      }
      break;

    case PainterBackendGL::data_store_ssbo:
      {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_vao.m_data_store_binding_point, m_vao.m_data_bo);
      }
      break;

    default:
      FASTUIDRAWassert(!"Bad value for m_vao.m_data_store_backing");
    }

  unsigned int choice;
  choice = m_pr->m_reg_gl->params().separate_program_for_discard() ?
    PainterBackendGL::program_without_discard :
    PainterBackendGL::program_all;

  DrawState st(m_pr->m_reg_gl->programs()[choice].get());
  st.m_current_program->use_program();

  for(const DrawEntry &entry : m_draws)
    {
      entry.draw(m_pr, m_vao, st);
    }
  glBindVertexArray(0);
}

void
DrawCommand::
unmap_implement(unsigned int attributes_written,
                unsigned int indices_written,
                unsigned int data_store_written) const
{
  m_attributes_written = attributes_written;
  add_entry(indices_written);
  FASTUIDRAWassert(m_indices_written == indices_written);

  glBindBuffer(GL_ARRAY_BUFFER, m_vao.m_attribute_bo);
  glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, attributes_written * sizeof(fastuidraw::PainterAttribute));
  glUnmapBuffer(GL_ARRAY_BUFFER);

  glBindBuffer(GL_ARRAY_BUFFER, m_vao.m_header_bo);
  glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, attributes_written * sizeof(uint32_t));
  glUnmapBuffer(GL_ARRAY_BUFFER);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_vao.m_index_bo);
  glFlushMappedBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, indices_written * sizeof(fastuidraw::PainterIndex));
  glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

  glBindBuffer(GL_ARRAY_BUFFER, m_vao.m_data_bo);
  glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, data_store_written * sizeof(fastuidraw::generic_data));
  glUnmapBuffer(GL_ARRAY_BUFFER);
}

void
DrawCommand::
add_entry(unsigned int indices_written) const
{
  unsigned int count;
  const fastuidraw::PainterIndex *offset(nullptr);

  if (m_draws.empty())
    {
      m_draws.push_back(fastuidraw::BlendMode());
    }
  FASTUIDRAWassert(indices_written >= m_indices_written);
  count = indices_written - m_indices_written;
  offset += m_indices_written;
  m_draws.back().add_entry(count, offset);
  m_indices_written = indices_written;
}

///////////////////////////////////
// PainterBackendGLPrivate methods
PainterBackendGLPrivate::
PainterBackendGLPrivate(fastuidraw::gl::PainterBackendGL *p):
  m_nearest_filter_sampler(0),
  m_pool(nullptr),
  m_surface_gl(nullptr),
  m_p(p)
{
  using namespace fastuidraw;
  using namespace fastuidraw::gl;
  using namespace fastuidraw::gl::detail;

  reference_counted_ptr<PainterShaderRegistrar> reg_base(m_p->painter_shader_registrar());

  FASTUIDRAWassert(reg_base.dynamic_cast_ptr<PainterShaderRegistrarGL>());
  m_reg_gl = reg_base.static_cast_ptr<PainterShaderRegistrarGL>();

  m_pool = FASTUIDRAWnew painter_vao_pool(m_reg_gl->params(), m_p->configuration_base(),
                                          m_reg_gl->tex_buffer_support(),
                                          m_reg_gl->uber_shader_builder_params().binding_points());
}

PainterBackendGLPrivate::
~PainterBackendGLPrivate()
{
  if (m_nearest_filter_sampler != 0)
    {
      glDeleteSamplers(1, &m_nearest_filter_sampler);
    }

  if (m_pool)
    {
      FASTUIDRAWdelete(m_pool);
    }
}

void
PainterBackendGLPrivate::
compute_uber_shader_params(const fastuidraw::gl::PainterBackendGL::ConfigurationGL &params,
                           const fastuidraw::gl::ContextProperties &ctx,
                           fastuidraw::gl::PainterBackendGL::UberShaderParams &out_params,
                           fastuidraw::PainterShaderSet &out_shaders)
{
  using namespace fastuidraw;
  using namespace fastuidraw::gl;
  using namespace fastuidraw::gl::detail;

  enum PainterBackendGL::auxiliary_buffer_t aux_type;
  bool supports_bindless;

  supports_bindless = ctx.has_extension("GL_ARB_bindless_texture")
    || ctx.has_extension("GL_NV_bindless_texture");

  ColorStopAtlasGL *color;
  FASTUIDRAWassert(dynamic_cast<ColorStopAtlasGL*>(params.colorstop_atlas().get()));
  color = static_cast<ColorStopAtlasGL*>(params.colorstop_atlas().get());
  enum PainterBackendGL::colorstop_backing_t colorstop_tp;
  if (color->texture_bind_target() == GL_TEXTURE_2D_ARRAY)
    {
      colorstop_tp = PainterBackendGL::colorstop_texture_2d_array;
    }
  else
    {
      colorstop_tp = PainterBackendGL::colorstop_texture_1d_array;
    }

  out_params
    .compositing_type(params.compositing_type())
    .supports_bindless_texturing(supports_bindless)
    .assign_layout_to_vertex_shader_inputs(params.assign_layout_to_vertex_shader_inputs())
    .assign_layout_to_varyings(params.assign_layout_to_varyings())
    .assign_binding_points(params.assign_binding_points())
    .use_ubo_for_uniforms(true)
    .clipping_type(params.clipping_type())
    .z_coordinate_convention(PainterBackendGL::z_minus_1_to_1)
    .negate_normalized_y_coordinate(false)
    .vert_shader_use_switch(params.vert_shader_use_switch())
    .frag_shader_use_switch(params.frag_shader_use_switch())
    .composite_shader_use_switch(params.composite_shader_use_switch())
    .unpack_header_and_brush_in_frag_shader(params.unpack_header_and_brush_in_frag_shader())
    .data_store_backing(params.data_store_backing())
    .data_blocks_per_store_buffer(params.data_blocks_per_store_buffer())
    .glyph_geometry_backing(params.glyph_atlas()->param_values().glyph_geometry_backing_store_type())
    .glyph_geometry_backing_log2_dims(params.glyph_atlas()->param_values().texture_2d_array_geometry_store_log2_dims())
    .have_float_glyph_texture_atlas(params.glyph_atlas()->texel_texture(false) != 0)
    .colorstop_atlas_backing(colorstop_tp)
    .provide_auxiliary_image_buffer(params.provide_auxiliary_image_buffer())
    .use_uvec2_for_bindless_handle(ctx.has_extension("GL_ARB_bindless_texture"));

  reference_counted_ptr<const PainterDraw::Action> q;

  aux_type = params.provide_auxiliary_image_buffer();
  if (params.default_stroke_shader_aa_type() == PainterStrokeShader::cover_then_draw
      && aux_type == PainterBackendGL::auxiliary_buffer_atomic)
    {
      bool use_by_region;

      #ifdef FASTUIDRAW_GL_USE_GLES
        {
          use_by_region = true;
        }
      #else
        {
          use_by_region = ctx.version() >= ivec2(4, 5)
            || ctx.has_extension("GL_ARB_ES3_1_compatibility");
        }
      #endif

      if (use_by_region)
        {
          q = FASTUIDRAWnew ImageBarrierByRegion();
        }
      else
        {
          q = FASTUIDRAWnew ImageBarrier();
        }
    }

  out_shaders = out_params.default_shaders(params.default_stroke_shader_aa_type(),
                                           q, q);
}

void
PainterBackendGLPrivate::
set_gl_state(fastuidraw::gpu_dirty_state v, bool clear_depth, bool clear_color_buffer)
{
  using namespace fastuidraw;
  using namespace fastuidraw::gl;
  using namespace fastuidraw::glsl;

  const PainterBackendGL::UberShaderParams &uber_params(m_reg_gl->uber_shader_builder_params());
  const PainterBackendGL::BindingPoints &binding_points(uber_params.binding_points());
  enum PainterBackendGL::auxiliary_buffer_t aux_type;
  enum PainterBackendGL::compositing_type_t compositing_type;
  const PainterBackend::Surface::Viewport &vwp(m_surface_gl->m_viewport);
  ivec2 dimensions(m_surface_gl->m_properties.dimensions());
  bool has_images;

  aux_type = uber_params.provide_auxiliary_image_buffer();
  compositing_type = m_reg_gl->params().compositing_type();
  has_images = (aux_type != PainterBackendGL::no_auxiliary_buffer
                && aux_type != PainterBackendGL::auxiliary_buffer_framebuffer_fetch)
    || (compositing_type == PainterBackendGL::compositing_interlock);

  if (v & gpu_dirty_state::render_target)
    {
      GLuint last_fbo(0), fbo(0);
      c_array<const GLenum> draw_buffers;

      if (clear_depth)
        {
          GLbitfield mask(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

          fbo = m_surface_gl->fbo(aux_type, compositing_type);
          draw_buffers = m_surface_gl->draw_buffers(aux_type, compositing_type);

          if (clear_color_buffer)
            {
              glClearColor(m_surface_gl->m_clear_color.x(),
                           m_surface_gl->m_clear_color.y(),
                           m_surface_gl->m_clear_color.z(),
                           m_surface_gl->m_clear_color.w());
              mask |= GL_COLOR_BUFFER_BIT;

              /* Compositeing interlock does not have the color buffer as
               * part of the FBO, so to clear the color buffer we need
               * bind an FBO that has it; note that if the aux type
               * is framebuffer_fetch it also gets cleared which is
               * a good thing for tiled-based renderers.
               */
              if (compositing_type == PainterBackendGL::compositing_interlock)
                {
                  enum PainterBackendGL::compositing_type_t bl(PainterBackendGL::compositing_dual_src);
                  fbo = m_surface_gl->fbo(aux_type, bl);
                  draw_buffers = m_surface_gl->draw_buffers(aux_type, bl);
                }
            }

          glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
          glDrawBuffers(draw_buffers.size(), draw_buffers.c_ptr());
          glClear(mask);

          last_fbo = fbo;
        }
      else
        {
          FASTUIDRAWassert(!clear_color_buffer);
        }

      fbo = m_surface_gl->fbo(aux_type, compositing_type);
      if (fbo != last_fbo)
        {
          draw_buffers = m_surface_gl->draw_buffers(aux_type, compositing_type);
          glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
          glDrawBuffers(draw_buffers.size(), draw_buffers.c_ptr());
        }

      v |= gpu_dirty_state::viewport_scissor;
    }
  else
    {
      FASTUIDRAWassert(!clear_color_buffer);
      FASTUIDRAWassert(!clear_depth);
    }

  if ((v & gpu_dirty_state::images) != 0 && has_images)
    {
      detail::SurfaceGLPrivate::auxiliary_buffer_t tp;

      if (aux_type != PainterBackendGL::no_auxiliary_buffer
          && aux_type != PainterBackendGL::auxiliary_buffer_framebuffer_fetch)
        {
          tp = (aux_type == gl::PainterBackendGL::auxiliary_buffer_atomic) ?
            detail::SurfaceGLPrivate::auxiliary_buffer_u32 :
            detail::SurfaceGLPrivate::auxiliary_buffer_u8;

          glBindImageTexture(binding_points.auxiliary_image_buffer(),
                             m_surface_gl->auxiliary_buffer(tp), //texture
                             0, //level
                             GL_FALSE, //layered
                             0, //layer
                             GL_READ_WRITE, //access
                             detail::SurfaceGLPrivate::auxiliaryBufferInternalFmt(tp));
        }

      if (compositing_type == PainterBackendGL::compositing_interlock)
        {
          glBindImageTexture(binding_points.color_interlock_image_buffer(),
                             m_surface_gl->color_buffer(), //texture
                             0, //level
                             GL_FALSE, //layered
                             0, //layer
                             GL_READ_WRITE, //access
                             GL_RGBA8);
        }
    }

  if (v & gpu_dirty_state::depth_stencil)
    {
      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_GEQUAL);
      glDisable(GL_STENCIL_TEST);

      #ifdef FASTUIDRAW_GL_USE_GLES
        {
          glClearDepthf(0.0f);
        }
      #else
        {
          glClearDepth(0.0f);
        }
      #endif
    }

  if (v & gpu_dirty_state::buffer_masks)
    {
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glDepthMask(GL_TRUE);
    }

  if (v & gpu_dirty_state::viewport_scissor)
    {
      if (dimensions.x() > vwp.m_dimensions.x()
          || dimensions.y() > vwp.m_dimensions.y()
          || vwp.m_origin.x() != 0
          || vwp.m_origin.y() != 0)
        {
          glEnable(GL_SCISSOR_TEST);
          glScissor(vwp.m_origin.x(), vwp.m_origin.y(),
                    vwp.m_dimensions.x(), vwp.m_dimensions.y());
        }
      else
        {
          glDisable(GL_SCISSOR_TEST);
        }

      glViewport(vwp.m_origin.x(), vwp.m_origin.y(),
                 vwp.m_dimensions.x(), vwp.m_dimensions.y());
    }

  if ((v & gpu_dirty_state::hw_clip) && m_reg_gl->number_clip_planes() > 0)
    {
      if (m_reg_gl->params().clipping_type() == PainterBackendGL::clipping_via_gl_clip_distance)
        {
          for (int i = 0; i < 4; ++i)
            {
              glEnable(GL_CLIP_DISTANCE0 + i);
            }
        }
      else
        {
          for (int i = 0; i < 4; ++i)
            {
              glDisable(GL_CLIP_DISTANCE0 + i);
            }
        }

      for(unsigned int i = 4; i < m_reg_gl->number_clip_planes(); ++i)
        {
          glDisable(GL_CLIP_DISTANCE0 + i);
        }
    }

  GlyphAtlasGL *glyphs;
  FASTUIDRAWassert(dynamic_cast<GlyphAtlasGL*>(m_p->glyph_atlas().get()));
  glyphs = static_cast<GlyphAtlasGL*>(m_p->glyph_atlas().get());

  if (v & gpu_dirty_state::textures)
    {
      ImageAtlasGL *image;
      FASTUIDRAWassert(dynamic_cast<ImageAtlasGL*>(m_p->image_atlas().get()));
      image = static_cast<ImageAtlasGL*>(m_p->image_atlas().get());

      ColorStopAtlasGL *color;
      FASTUIDRAWassert(dynamic_cast<ColorStopAtlasGL*>(m_p->colorstop_atlas().get()));
      color = static_cast<ColorStopAtlasGL*>(m_p->colorstop_atlas().get());

      glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_color_tiles_nearest());
      glBindSampler(binding_points.image_atlas_color_tiles_nearest(), m_nearest_filter_sampler);
      glBindTexture(GL_TEXTURE_2D_ARRAY, image->color_texture());

      glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_color_tiles_linear());
      glBindSampler(binding_points.image_atlas_color_tiles_linear(), 0);
      glBindTexture(GL_TEXTURE_2D_ARRAY, image->color_texture());

      glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_index_tiles());
      glBindSampler(binding_points.image_atlas_index_tiles(), 0);
      glBindTexture(GL_TEXTURE_2D_ARRAY, image->index_texture());

      glActiveTexture(GL_TEXTURE0 + binding_points.glyph_atlas_texel_store_uint());
      glBindSampler(binding_points.glyph_atlas_texel_store_uint(), 0);
      glBindTexture(GL_TEXTURE_2D_ARRAY, glyphs->texel_texture(true));

      glActiveTexture(GL_TEXTURE0 + binding_points.glyph_atlas_texel_store_float());
      glBindSampler(binding_points.glyph_atlas_texel_store_float(), 0);
      glBindTexture(GL_TEXTURE_2D_ARRAY, glyphs->texel_texture(false));

      if (glyphs->geometry_backed_by_texture())
        {
          glActiveTexture(GL_TEXTURE0 + binding_points.glyph_atlas_geometry_store_texture());
          glBindSampler(binding_points.glyph_atlas_geometry_store_texture(), 0);
          glBindTexture(glyphs->geometry_binding_point(), glyphs->geometry_backing());
        }

      glActiveTexture(GL_TEXTURE0 + binding_points.colorstop_atlas());
      glBindSampler(binding_points.colorstop_atlas(), 0);
      glBindTexture(ColorStopAtlasGL::texture_bind_target(), color->texture());

      glActiveTexture(GL_TEXTURE0 + binding_points.external_texture());
      glBindTexture(GL_TEXTURE_2D, m_current_external_texture);
      glBindSampler(binding_points.external_texture(), 0);
    }

  if (v & gpu_dirty_state::constant_buffers)
    {
      GLuint ubo;
      unsigned int size_generics(PainterShaderRegistrarGLSL::ubo_size());
      unsigned int size_bytes(sizeof(generic_data) * size_generics);
      void *ubo_mapped;

      /* Grabs and binds the buffer */
      ubo = m_pool->uniform_ubo(size_bytes, GL_UNIFORM_BUFFER);
      FASTUIDRAWassert(ubo != 0);

      if (!m_uniform_ubo_ready)
        {
          c_array<generic_data> ubo_mapped_ptr;
          ubo_mapped = glMapBufferRange(GL_UNIFORM_BUFFER, 0, size_bytes,
                                        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
          ubo_mapped_ptr = c_array<generic_data>(static_cast<generic_data*>(ubo_mapped),
                                                 size_generics);

          m_reg_gl->fill_uniform_buffer(m_surface_gl->m_viewport, ubo_mapped_ptr);
          glFlushMappedBufferRange(GL_UNIFORM_BUFFER, 0, size_bytes);
          glUnmapBuffer(GL_UNIFORM_BUFFER);
          m_uniform_ubo_ready = true;
        }

      glBindBufferBase(GL_UNIFORM_BUFFER, binding_points.uniforms_ubo(), ubo);
    }

  if (v & gpu_dirty_state::storage_buffers)
    {
      if (!glyphs->geometry_backed_by_texture())
        {
          glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                           binding_points.glyph_atlas_geometry_store_ssbo(),
                           glyphs->geometry_backing());
        }
    }
}

/////////////////////////////////////////////////////////////////
// fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties methods
fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties::
Properties(void)
{
  m_d = FASTUIDRAWnew SurfacePropertiesPrivate();
}

fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties::
Properties(const Properties &obj)
{
  SurfacePropertiesPrivate *d;
  d = static_cast<SurfacePropertiesPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew SurfacePropertiesPrivate(*d);
}

fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties::
~Properties()
{
  SurfacePropertiesPrivate *d;
  d = static_cast<SurfacePropertiesPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

assign_swap_implement(fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties)

setget_implement(fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties,
                 SurfacePropertiesPrivate,
                 fastuidraw::ivec2, dimensions);

setget_implement(fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties,
                 SurfacePropertiesPrivate,
                 unsigned int, msaa);
///////////////////////////////////////////////
// fastuidraw::gl::PainterBackendGL::SurfaceGL methods
fastuidraw::gl::PainterBackendGL::SurfaceGL::
SurfaceGL(const Properties &props)
{
  m_d = FASTUIDRAWnew detail::SurfaceGLPrivate(0u, props);
}

fastuidraw::gl::PainterBackendGL::SurfaceGL::
SurfaceGL(const Properties &prop, GLuint color_buffer_texture)
{
  m_d = FASTUIDRAWnew detail::SurfaceGLPrivate(color_buffer_texture, prop);
}

fastuidraw::gl::PainterBackendGL::SurfaceGL::
~SurfaceGL()
{
  detail::SurfaceGLPrivate *d;
  d = static_cast<detail::SurfaceGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

GLuint
fastuidraw::gl::PainterBackendGL::SurfaceGL::
texture(void) const
{
  detail::SurfaceGLPrivate *d;
  d = static_cast<detail::SurfaceGLPrivate*>(m_d);
  return d->color_buffer();
}

void
fastuidraw::gl::PainterBackendGL::SurfaceGL::
blit_surface(const Viewport &src,
             const Viewport &dst,
             GLenum filter) const
{
  detail::SurfaceGLPrivate *d;
  d = static_cast<detail::SurfaceGLPrivate*>(m_d);
  GLint old_fbo;

  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_fbo);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, d->fbo(detail::SurfaceGLPrivate::fbo_color_buffer));
  glBlitFramebuffer(src.m_origin.x(),
                    src.m_origin.y(),
                    src.m_origin.x() + src.m_dimensions.x(),
                    src.m_origin.y() + src.m_dimensions.y(),
                    dst.m_origin.x(),
                    dst.m_origin.y(),
                    dst.m_origin.x() + dst.m_dimensions.x(),
                    dst.m_origin.y() + dst.m_dimensions.y(),
                    GL_COLOR_BUFFER_BIT, filter);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, old_fbo);
}

void
fastuidraw::gl::PainterBackendGL::SurfaceGL::
blit_surface(GLenum filter) const
{
  ivec2 dims(dimensions());
  Viewport vwp(0, 0, dims.x(), dims.y());
  blit_surface(vwp, vwp, filter);
}

fastuidraw::ivec2
fastuidraw::gl::PainterBackendGL::SurfaceGL::
dimensions(void) const
{
  return properties().dimensions();
}

get_implement(fastuidraw::gl::PainterBackendGL::SurfaceGL,
              fastuidraw::gl::detail::SurfaceGLPrivate,
              const fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties&,
              properties)
setget_implement(fastuidraw::gl::PainterBackendGL::SurfaceGL,
                 fastuidraw::gl::detail::SurfaceGLPrivate,
                 fastuidraw::PainterBackend::Surface::Viewport, viewport);
setget_implement(fastuidraw::gl::PainterBackendGL::SurfaceGL,
                 fastuidraw::gl::detail::SurfaceGLPrivate,
                 const fastuidraw::vec4&, clear_color)

///////////////////////////////////////////////
// fastuidraw::gl::PainterBackendGL::ConfigurationGL methods
fastuidraw::gl::PainterBackendGL::ConfigurationGL::
ConfigurationGL(void)
{
  m_d = FASTUIDRAWnew ConfigurationGLPrivate();
}

fastuidraw::gl::PainterBackendGL::ConfigurationGL::
ConfigurationGL(const ConfigurationGL &obj)
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ConfigurationGLPrivate(*d);
}

fastuidraw::gl::PainterBackendGL::ConfigurationGL::
~ConfigurationGL()
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::c_string
fastuidraw::gl::PainterBackendGL::ConfigurationGL::
glsl_version_override(void) const
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(m_d);
  return d->m_glsl_version_override.c_str();
}

fastuidraw::gl::PainterBackendGL::ConfigurationGL&
fastuidraw::gl::PainterBackendGL::ConfigurationGL::
glsl_version_override(c_string v)
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(m_d);
  d->m_glsl_version_override = (v) ? v : "";
  return *this;
}

fastuidraw::gl::PainterBackendGL::ConfigurationGL&
fastuidraw::gl::PainterBackendGL::ConfigurationGL::
configure_from_context(bool for_msaa, const ContextProperties &ctx)
{
  using namespace fastuidraw::gl::detail;

  ConfigurationGLPrivate *d;
  enum interlock_type_t interlock_type;
  bool have_ffb;

  d = static_cast<ConfigurationGLPrivate*>(m_d);
  interlock_type = compute_interlock_type(ctx);
  have_ffb = ctx.has_extension("GL_EXT_shader_framebuffer_fetch");

  /* First set ideal parameters */
  d->m_alignment = 4;

  d->m_break_on_shader_change = false;
  d->m_clipping_type = clipping_via_gl_clip_distance;

  /* unpacking oodles of data in frag-shader is way
   * more expensive than having oddles of varyings.
   */
  d->m_unpack_header_and_brush_in_frag_shader = false;

  /* These do not impact performance, but they make
   * cleaner initialization.
   */
  d->m_assign_layout_to_vertex_shader_inputs = true;
  d->m_assign_layout_to_varyings = true;
  d->m_assign_binding_points = true;
  d->m_separate_program_for_discard = true;

  if (!for_msaa)
    {
      d->m_compositing_type = compositing_framebuffer_fetch;

      /* We are prefering interlock over framebuffer fetch
       * for the auxiliary buffer because the auxiliary buffer
       * is not always written (or read).
       */
      if (interlock_type == no_interlock)
        {
          d->m_provide_auxiliary_image_buffer = have_ffb ?
            auxiliary_buffer_framebuffer_fetch :
            no_auxiliary_buffer;
        }
      else
        {
          d->m_provide_auxiliary_image_buffer =
            compute_provide_auxiliary_buffer(auxiliary_buffer_interlock, ctx);
        }
    }
  else
    {
      /* if one is drawing with MSAA, one should NOT use shader based anti-aliasing
       * when stroking or filling anways.
       */
      d->m_provide_auxiliary_image_buffer = no_auxiliary_buffer;

      /* compositing_interlock does NOT work with MSAA and compositing_framebuffer_fetch would
       * force the fragment shader to work per-sample.
       */
      d->m_compositing_type = compositing_dual_src;
    }

  if (d->m_provide_auxiliary_image_buffer == no_auxiliary_buffer)
    {
      d->m_default_stroke_shader_aa_type = PainterStrokeShader::draws_solid_then_fuzz;
    }
  else
    {
      d->m_default_stroke_shader_aa_type = PainterStrokeShader::cover_then_draw;
    }

  /* Adjust compositing type from GL context properties */
  d->m_compositing_type = compute_compositing_type(d->m_provide_auxiliary_image_buffer,
                                             interlock_type, d->m_compositing_type,
                                             ctx);

  /* Adjust clipping type from GL context properties */
  d->m_clipping_type = compute_clipping_type(d->m_compositing_type,
                                             d->m_clipping_type,
                                             ctx);

  /* pay attention to the context for m_data_store_backing.
   * Generally speaking, for caching UBO > SSBO > TBO,
   * but max UBO size might be too tiny, we are arbitrarily
   * guessing that the data store buffer should be 64K blocks
   * (which is 256KB); what size is good is really depends on
   * how much data each frame will have which mostly depends
   * on how often the brush and transformations and clipping
   * change.
   */
  d->m_data_blocks_per_store_buffer = 1024 * 64;
  d->m_data_store_backing = data_store_ubo;

  unsigned int max_ubo_size, max_num_blocks, block_size;
  block_size = d->m_alignment * sizeof(generic_data);
  max_ubo_size = context_get<GLint>(GL_MAX_UNIFORM_BLOCK_SIZE);
  max_num_blocks = max_ubo_size / block_size;
  if (max_num_blocks < d->m_data_blocks_per_store_buffer)
    {
      if (shader_storage_buffers_supported(ctx))
        {
          d->m_data_store_backing = data_store_ssbo;
        }
      else if (detail::compute_tex_buffer_support(ctx) != detail::tex_buffer_not_supported)
        {
          d->m_data_store_backing = data_store_tbo;
        }
    }

  /* NVIDIA's GPU's (alteast up to 700 series) gl_ClipDistance is not
   * robust enough to work  with FastUIDraw regardless of driver
   * (NVIDIA proprietary or Nouveau open source). We try to detect
   * either in the version or renderer and if so, mark gl_ClipDistance
   * as NOT supported.
   */
  bool NVIDIA_detected;
  std::string gl_version, gl_renderer;

  gl_version = (const char*)glGetString(GL_VERSION);
  gl_renderer = (const char*)glGetString(GL_RENDERER);
  NVIDIA_detected = gl_version.find("NVIDIA") != std::string::npos
    || gl_renderer.find("GeForce") != std::string::npos
    || gl_version.find("nouveau") != std::string::npos
    || gl_renderer.find("nouveau") != std::string::npos;

  d->m_clipping_type = compute_clipping_type(d->m_compositing_type,
                                             d->m_clipping_type,
                                             ctx,
                                             !NVIDIA_detected);

  /* likely shader compilers like if/ese chains more than
   * switches, atleast Mesa really prefers if/else chains
   */
  d->m_vert_shader_use_switch = false;
  d->m_frag_shader_use_switch = false;
  d->m_composite_shader_use_switch = false;

  /* UI rendering is often dominated by drawing quads which
   * means for every 6 indices there are 4 attributes. However,
   * how many quads per draw-call, we just guess at 512 * 512
   * attributes
   */
  d->m_attributes_per_buffer = 512 * 512;
  d->m_indices_per_buffer = (d->m_attributes_per_buffer * 6) / 4;

  /* Very often drivers will have the previous frame still
   * in flight when a new frame is started, so we do not want
   * to modify buffers in use, so that puts the minumum number
   * of pools to 2. Also, often enough there is triple buffering
   * so we play it safe and make it 3.
   */
  d->m_number_pools = 3;

  return *this;
}

fastuidraw::gl::PainterBackendGL::ConfigurationGL&
fastuidraw::gl::PainterBackendGL::ConfigurationGL::
adjust_for_context(const ContextProperties &ctx)
{
  using namespace fastuidraw::gl::detail;

  ConfigurationGLPrivate *d;
  enum detail::tex_buffer_support_t tex_buffer_support;
  enum interlock_type_t interlock_type;
  enum clipping_type_t clipping_type;

  d = static_cast<ConfigurationGLPrivate*>(m_d);
  interlock_type = compute_interlock_type(ctx);
  tex_buffer_support = detail::compute_tex_buffer_support(ctx);

  if (d->m_data_store_backing == data_store_tbo
     && tex_buffer_support == detail::tex_buffer_not_supported)
    {
      // TBO's not supported, fall back to using SSBO's.
      d->m_data_store_backing = data_store_ssbo;
    }

  if (d->m_data_store_backing == data_store_ssbo
      && !shader_storage_buffers_supported(ctx))
    {
      // SSBO's not supported, fall back to using UBO's.
      d->m_data_store_backing = data_store_ubo;
    }

  /* Query GL what is good size for data store buffer. Size is dependent
   * how the data store is backed.
   */
  switch(d->m_data_store_backing)
    {
    case data_store_tbo:
      {
        unsigned int max_texture_buffer_size(0);
        max_texture_buffer_size = context_get<GLint>(GL_MAX_TEXTURE_BUFFER_SIZE);
        d->m_data_blocks_per_store_buffer = t_min(max_texture_buffer_size,
                                                  d->m_data_blocks_per_store_buffer);
      }
      break;

    case data_store_ubo:
      {
        unsigned int max_ubo_size_bytes, max_num_blocks, block_size_bytes;
        d->m_alignment = 4;
        block_size_bytes = d->m_alignment * sizeof(generic_data);
        max_ubo_size_bytes = context_get<GLint>(GL_MAX_UNIFORM_BLOCK_SIZE);
        max_num_blocks = max_ubo_size_bytes / block_size_bytes;
        d->m_data_blocks_per_store_buffer = t_min(max_num_blocks,
                                                  d->m_data_blocks_per_store_buffer);
      }
      break;

    case data_store_ssbo:
      {
        unsigned int max_ssbo_size_bytes, max_num_blocks, block_size_bytes;
        d->m_alignment = 4;
        block_size_bytes = d->m_alignment * sizeof(generic_data);
        max_ssbo_size_bytes = context_get<GLint>(GL_MAX_SHADER_STORAGE_BLOCK_SIZE);
        max_num_blocks = max_ssbo_size_bytes / block_size_bytes;
        d->m_data_blocks_per_store_buffer = t_min(max_num_blocks,
                                                  d->m_data_blocks_per_store_buffer);
      }
      break;
    }

  clipping_type = compute_clipping_type(d->m_compositing_type, d->m_clipping_type, ctx);
  d->m_clipping_type= clipping_type;

  /* if have to use discard for clipping, then there is zero point to
   * separate the discarding and non-discarding item shaders.
   */
  if (d->m_clipping_type == clipping_via_discard)
    {
      d->m_separate_program_for_discard = false;
    }

  /* Some shader features require new version of GL or
   * specific extensions.
   */
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      if (ctx.version() < ivec2(3, 2))
        {
          d->m_assign_layout_to_varyings = d->m_assign_layout_to_varyings
            && ctx.has_extension("GL_EXT_separate_shader_objects");
        }

      if (ctx.version() <= ivec2(3, 0))
        {
          /* GL ES 3.0 does not support layout(binding=) and
           * does not support image-load-store either
           */
          d->m_assign_binding_points = false;
        }
    }
  #else
    {
      if (ctx.version() < ivec2(4, 2))
        {
          d->m_assign_layout_to_varyings = d->m_assign_layout_to_varyings
            && ctx.has_extension("GL_ARB_separate_shader_objects");

          d->m_assign_binding_points = d->m_assign_binding_points
            && ctx.has_extension("GL_ARB_shading_language_420pack");
        }
    }
  #endif

  interlock_type = compute_interlock_type(ctx);
  d->m_provide_auxiliary_image_buffer =
    compute_provide_auxiliary_buffer(d->m_provide_auxiliary_image_buffer, ctx);

  d->m_compositing_type = compute_compositing_type(d->m_provide_auxiliary_image_buffer,
                                             interlock_type,
                                             d->m_compositing_type, ctx);

  if (d->m_provide_auxiliary_image_buffer == no_auxiliary_buffer)
    {
      d->m_default_stroke_shader_aa_type = PainterStrokeShader::draws_solid_then_fuzz;
    }

  /* if have to use discard for clipping, then there is zero point to
   * separate the discarding and non-discarding item shaders.
   */
  if (d->m_clipping_type == PainterBackendGL::clipping_via_discard)
    {
      d->m_separate_program_for_discard = false;
    }

  return *this;
}

fastuidraw::gl::PainterBackendGL::ConfigurationGL&
fastuidraw::gl::PainterBackendGL::ConfigurationGL::
create_missing_atlases(const ContextProperties &ctx)
{
  ConfigurationGLPrivate *d;
  d = static_cast<ConfigurationGLPrivate*>(m_d);

  FASTUIDRAWunused(ctx);
  if (!d->m_image_atlas)
    {
      ImageAtlasGL::params params;
      d->m_image_atlas = FASTUIDRAWnew ImageAtlasGL(params);
    }

  if (!d->m_glyph_atlas)
    {
      GlyphAtlasGL::params params;
      params.use_optimal_geometry_store_backing();
      d->m_glyph_atlas = FASTUIDRAWnew GlyphAtlasGL(params);
    }

  if (!d->m_colorstop_atlas)
    {
      ColorStopAtlasGL::params params;
      params.optimal_width();
      d->m_colorstop_atlas = FASTUIDRAWnew ColorStopAtlasGL(params);
    }

  return *this;
}

assign_swap_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL)

setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 int, alignment)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 unsigned int, attributes_per_buffer)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 unsigned int, indices_per_buffer)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 unsigned int, data_blocks_per_store_buffer)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 unsigned int, number_pools)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, break_on_shader_change)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::gl::ImageAtlasGL>&, image_atlas)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::gl::ColorStopAtlasGL>&, colorstop_atlas)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 const fastuidraw::reference_counted_ptr<fastuidraw::gl::GlyphAtlasGL>&, glyph_atlas)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 enum fastuidraw::gl::PainterBackendGL::clipping_type_t, clipping_type)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, vert_shader_use_switch)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, frag_shader_use_switch)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, composite_shader_use_switch)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, unpack_header_and_brush_in_frag_shader)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 enum fastuidraw::gl::PainterBackendGL::data_store_backing_t, data_store_backing)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, assign_layout_to_vertex_shader_inputs)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, assign_layout_to_varyings)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, assign_binding_points)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 bool, separate_program_for_discard)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 enum fastuidraw::PainterStrokeShader::type_t, default_stroke_shader_aa_type)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 enum fastuidraw::gl::PainterBackendGL::compositing_type_t, compositing_type)
setget_implement(fastuidraw::gl::PainterBackendGL::ConfigurationGL, ConfigurationGLPrivate,
                 enum fastuidraw::gl::PainterBackendGL::auxiliary_buffer_t, provide_auxiliary_image_buffer)

///////////////////////////////////////////////
// fastuidraw::gl::PainterBackendGL methods
fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterBackendGL>
fastuidraw::gl::PainterBackendGL::
create(ConfigurationGL config_gl, const ContextProperties &ctx)
{
  UberShaderParams uber_params;
  PainterShaderSet shaders;
  reference_counted_ptr<glsl::PainterShaderRegistrarGLSL> reg;

  config_gl
    .adjust_for_context(ctx)
    .create_missing_atlases(ctx);

  PainterBackendGLPrivate::compute_uber_shader_params(config_gl, ctx, uber_params, shaders);
  return FASTUIDRAWnew PainterBackendGL(config_gl, uber_params, shaders);
}

fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterBackendGL>
fastuidraw::gl::PainterBackendGL::
create(bool for_msaa, const ContextProperties &ctx)
{
  ConfigurationGL config_gl;
  config_gl
    .configure_from_context(for_msaa, ctx);

  return create(config_gl, ctx);
}

fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterBackendGL>
fastuidraw::gl::PainterBackendGL::
create_sharing_shaders(void)
{
  PainterBackendGLPrivate *d;
  d = static_cast<PainterBackendGLPrivate*>(m_d);
  return FASTUIDRAWnew PainterBackendGL(d->m_reg_gl->params(),
                                        d->m_reg_gl->uber_shader_builder_params(),
                                        this);
}

fastuidraw::gl::PainterBackendGL::
PainterBackendGL(const ConfigurationGL &config_gl,
                 const UberShaderParams &uber_params,
                 const PainterShaderSet &shaders):
  PainterBackend(config_gl.glyph_atlas(),
                 config_gl.image_atlas(),
                 config_gl.colorstop_atlas(),
                 FASTUIDRAWnew detail::PainterShaderRegistrarGL(config_gl, uber_params),
                 ConfigurationBase()
                 .alignment(config_gl.alignment())
                 .composite_type(uber_params.composite_type())
                 .supports_bindless_texturing(uber_params.supports_bindless_texturing()),
                 shaders)
{
  PainterBackendGLPrivate *d;
  m_d = d = FASTUIDRAWnew PainterBackendGLPrivate(this);
  d->m_reg_gl->set_hints(set_hints());
}

fastuidraw::gl::PainterBackendGL::
PainterBackendGL(const ConfigurationGL &config_gl,
                 const UberShaderParams &uber_params,
                 PainterBackendGL *p):
  PainterBackend(config_gl.glyph_atlas(),
                 config_gl.image_atlas(),
                 config_gl.colorstop_atlas(),
                 p->painter_shader_registrar(),
                 ConfigurationBase()
                 .alignment(config_gl.alignment())
                 .composite_type(uber_params.composite_type())
                 .supports_bindless_texturing(uber_params.supports_bindless_texturing()),
                 p->default_shaders())
{
  PainterBackendGLPrivate *d;
  m_d = d = FASTUIDRAWnew PainterBackendGLPrivate(this);
  d->m_reg_gl->set_hints(set_hints());
}

fastuidraw::gl::PainterBackendGL::
~PainterBackendGL()
{
  PainterBackendGLPrivate *d;
  d = static_cast<PainterBackendGLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::reference_counted_ptr<fastuidraw::gl::Program>
fastuidraw::gl::PainterBackendGL::
program(enum program_type_t tp)
{
  PainterBackendGLPrivate *d;
  d = static_cast<PainterBackendGLPrivate*>(m_d);
  return d->m_reg_gl->programs()[tp];
}

const fastuidraw::gl::PainterBackendGL::ConfigurationGL&
fastuidraw::gl::PainterBackendGL::
configuration_gl(void) const
{
  PainterBackendGLPrivate *d;
  d = static_cast<PainterBackendGLPrivate*>(m_d);
  return d->m_reg_gl->params();
}

unsigned int
fastuidraw::gl::PainterBackendGL::
attribs_per_mapping(void) const
{
  return configuration_gl().attributes_per_buffer();
}

unsigned int
fastuidraw::gl::PainterBackendGL::
indices_per_mapping(void) const
{
  return configuration_gl().indices_per_buffer();
}

void
fastuidraw::gl::PainterBackendGL::
on_pre_draw(const reference_counted_ptr<Surface> &surface,
            bool clear_color_buffer)
{
  PainterBackendGLPrivate *d;

  d = static_cast<PainterBackendGLPrivate*>(m_d);
  d->m_surface_gl = static_cast<detail::SurfaceGLPrivate*>(detail::SurfaceGLPrivate::surface_gl(surface)->m_d);

  if (d->m_nearest_filter_sampler == 0)
    {
      glGenSamplers(1, &d->m_nearest_filter_sampler);
      FASTUIDRAWassert(d->m_nearest_filter_sampler != 0);
      glSamplerParameteri(d->m_nearest_filter_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glSamplerParameteri(d->m_nearest_filter_sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    }

  d->m_uniform_ubo_ready = false;
  d->m_current_external_texture = 0;
  d->set_gl_state(gpu_dirty_state::all, true, clear_color_buffer);

  //cache the GLSL programs for use.
  d->m_cached_programs = d->m_reg_gl->programs();
}

void
fastuidraw::gl::PainterBackendGL::
on_post_draw(void)
{
  PainterBackendGLPrivate *d;
  d = static_cast<PainterBackendGLPrivate*>(m_d);

  /* this is somewhat paranoid to make sure that
   * the GL objects do not leak...
   */
  glUseProgram(0);
  glBindVertexArray(0);

  const UberShaderParams &uber_params(d->m_reg_gl->uber_shader_builder_params());
  const BindingPoints &binding_points(uber_params.binding_points());
  const ConfigurationGL &params(d->m_reg_gl->params());

  glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_color_tiles_nearest());
  glBindSampler(binding_points.image_atlas_color_tiles_nearest(), 0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_color_tiles_linear());
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + binding_points.image_atlas_index_tiles());
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + binding_points.glyph_atlas_texel_store_uint());
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glActiveTexture(GL_TEXTURE0 + binding_points.glyph_atlas_texel_store_float());
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  GlyphAtlasGL *glyphs;
  FASTUIDRAWassert(dynamic_cast<GlyphAtlasGL*>(glyph_atlas().get()));
  glyphs = static_cast<GlyphAtlasGL*>(glyph_atlas().get());

  if (glyphs->geometry_backed_by_texture())
    {
      glActiveTexture(GL_TEXTURE0 + binding_points.glyph_atlas_geometry_store_texture());
      glBindTexture(glyphs->geometry_binding_point(), 0);
    }
  else
    {
      glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                       binding_points.glyph_atlas_geometry_store_ssbo(),
                       0);
    }

  glActiveTexture(GL_TEXTURE0 + binding_points.colorstop_atlas());
  glBindTexture(ColorStopAtlasGL::texture_bind_target(), 0);

  enum auxiliary_buffer_t aux_type;
  aux_type = uber_params.provide_auxiliary_image_buffer();
  if (aux_type != PainterBackendGL::no_auxiliary_buffer
      && aux_type != PainterBackendGL::auxiliary_buffer_framebuffer_fetch)
    {
      glBindImageTexture(binding_points.auxiliary_image_buffer(), 0,
                         0, GL_FALSE, 0, GL_READ_ONLY, GL_R8UI);
    }

  if (params.compositing_type() == compositing_interlock)
    {
      glBindImageTexture(binding_points.color_interlock_image_buffer(), 0,
                         0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    }

  switch(params.data_store_backing())
    {
    case data_store_tbo:
      {
        glActiveTexture(GL_TEXTURE0 + binding_points.data_store_buffer_tbo());
        glBindTexture(GL_TEXTURE_BUFFER, 0);
      }
      break;

    case data_store_ubo:
      {
        glBindBufferBase(GL_UNIFORM_BUFFER, binding_points.data_store_buffer_ubo(), 0);
      }
      break;

    case data_store_ssbo:
      {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding_points.data_store_buffer_ssbo(), 0);
      }
      break;

    default:
      FASTUIDRAWassert(!"Bad value for params.data_store_backing()");
    }
  glBindBufferBase(GL_UNIFORM_BUFFER, binding_points.uniforms_ubo(), 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glDisable(GL_SCISSOR_TEST);
  d->m_pool->next_pool();
}

fastuidraw::reference_counted_ptr<fastuidraw::PainterDraw::Action>
fastuidraw::gl::PainterBackendGL::
bind_image(const reference_counted_ptr<const Image> &im)
{
  PainterBackendGLPrivate *d;

  d = static_cast<PainterBackendGLPrivate*>(m_d);
  return FASTUIDRAWnew TextureImageBindAction(im, d);
}

fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw>
fastuidraw::gl::PainterBackendGL::
map_draw(void)
{
  PainterBackendGLPrivate *d;
  d = static_cast<PainterBackendGLPrivate*>(m_d);

  return FASTUIDRAWnew DrawCommand(d->m_pool, d->m_reg_gl->params(), d);
}

const fastuidraw::gl::PainterBackendGL::BindingPoints&
fastuidraw::gl::PainterBackendGL::
binding_points(void) const
{
  PainterBackendGLPrivate *d;
  d = static_cast<PainterBackendGLPrivate*>(m_d);
  return d->m_reg_gl->uber_shader_builder_params().binding_points();
}
