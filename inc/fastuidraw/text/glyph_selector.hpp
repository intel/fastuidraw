/*!
 * \file glyph_selector.hpp
 * \brief file glyph_selector.hpp
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


#pragma once

#include <fastuidraw/text/font.hpp>
#include <fastuidraw/text/glyph.hpp>
#include <fastuidraw/text/glyph_cache.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
  @{
*/

  /*!
    A GlyphSelector performs the act of selecting a glyph
    from a font preference and a character code.
   */
  class GlyphSelector:public reference_counted<GlyphSelector>::default_base
  {
  public:

    /*!
      A FontGroup represents a group of fonts which is selected
      from a FontProperties.
     */
    class FontGroup
    {
    public:
      FontGroup(void):
        m_d(NULL)
      {}

    private:
      friend class GlyphSelector;
      void *m_d;
    };

    /*!
      Ctor
      \param cache GlyphCache to store/fetch glyphs.
     */
    explicit
    GlyphSelector(GlyphCache::handle cache);

    ~GlyphSelector();

    /*!
      Add a font to this GlyphSelector.
      \param h font to add
     */
    void
    add_font(FontBase::const_handle h);

    /*!
      Fetch a font from a FontProperties description. The return
      value will be the closest matched font added with add_font().
      \param props FontProperties by which to search
     */
    FontBase::const_handle
    fetch_font(const FontProperties &props);

    /*!
      Fetch a FontGroup from a FontProperties value
      \param props font properties used to generate group.
     */
    FontGroup
    fetch_group(const FontProperties &props);

    /*!
      Fetch a Glyph (and if necessary generate it and place into GlyphCache)
      with font merging from a glyph rendering type, font properties and character code.
      \param tp glyph rendering type.
      \param props font properties used to fetch font
      \param character_code character code of glyph to fetch
     */
    Glyph
    fetch_glyph(GlyphRender tp, const FontProperties &props, uint32_t character_code);

    /*!
      Fetch a Glyph (and if necessary generate it and place into GlyphCache)
      with font merging from a glyph rendering type, font properties and character code.
      \param tp glyph rendering type.
      \param group FontGroup used to fetch font
      \param character_code character code of glyph to fetch
     */
    Glyph
    fetch_glyph(GlyphRender tp, FontGroup group, uint32_t character_code);

    /*!
      Fetch a Glyph (and if necessary generate it and place into GlyphCache)
      with font merging from a glyph rendering type, font preference and character code.
      \param tp glyph rendering type.
      \param h handle to font from which to fetch the glyph, if the glyph
               is not present in the font attempt to get the glyph from
               a font of similiar properties
      \param character_code character code of glyph to fetch
     */
    Glyph
    fetch_glyph(GlyphRender tp, FontBase::const_handle h, uint32_t character_code);

    /*!
      Fetch a Glyph (and if necessary generate it and place into GlyphCache)
      without font merging from a glyph rendering type, font and character code.
      \param tp glyph rendering type
      \param h handle to font from which to fetch the glyph, if the glyph
               is not present in the font, then return an invalid Glyph.
      \param character_code character code of glyph to fetch
     */
    Glyph
    fetch_glyph_no_merging(GlyphRender tp, FontBase::const_handle h, uint32_t character_code);

    /*!
      Fill Glyph values from an iterator range of character code values.
      \tparam input_iterator read iterator to type that is castable to uint32_t
      \tparam output_iterator write iterator to Glyph
      \param tp glyph rendering type
      \param props font properties used to select glyph
      \param character_codes_begin iterator to 1st character code
      \param character_codes_end iterator to one past last character code
      \param output_begin begin iterator to output
     */
    template<typename input_iterator,
             typename output_iterator>
    void
    create_glyph_sequence(GlyphRender tp, const FontProperties &props,
                          input_iterator character_codes_begin,
                          input_iterator character_codes_end,
                          output_iterator output_begin);

    /*!
      Fill Glyph values from an iterator range of character code values.
      \tparam input_iterator read iterator to type that is castable to uint32_t
      \tparam output_iterator write iterator to Glyph
      \param tp glyph rendering type
      \param h handle to font from which to fetch the glyph, if the glyph
               is not present in the font attempt to get the glyph from
               a font of similiar properties
      \param character_codes_begin iterator to 1st character code
      \param character_codes_end iterator to one past last character code
      \param output_begin begin iterator to output
     */
    template<typename input_iterator,
             typename output_iterator>
    void
    create_glyph_sequence(GlyphRender tp, FontBase::const_handle h,
                          input_iterator character_codes_begin,
                          input_iterator character_codes_end,
                          output_iterator output_begin);

    /*!
      Fill an array of Glyph values from an array of character code values.
      \tparam input_iterator read iterator to type that is castable to uint32_t
      \tparam output_iterator write iterator to Glyph
      \param tp glyph rendering type
      \param h handle to font from which to fetch the glyph, if the glyph
               is not present in the font attempt to get the glyph from
               a font of similiar properties
      \param character_codes_begin iterator to first character code
      \param character_codes_end iterator to one pash last character code
      \param output_begin begin iterator to output
     */
    template<typename input_iterator,
             typename output_iterator>
    void
    create_glyph_sequence_no_merging(GlyphRender tp,
                                     FontBase::const_handle h,
                                     input_iterator character_codes_begin,
                                     input_iterator character_codes_end,
                                     output_iterator output_begin);

  private:
    void *m_d;
  };

  template<typename input_iterator,
           typename output_iterator>
  void
  GlyphSelector::
  create_glyph_sequence(GlyphRender tp, const FontProperties &props,
                        input_iterator character_codes_begin,
                        input_iterator character_codes_end,
                        output_iterator output_begin)
  {
    FontGroup group;

    group = fetch_group(props);
    for(;character_codes_begin != character_codes_end; ++character_codes_begin, ++output_begin)
      {
        uint32_t v;
        v = static_cast<uint32_t>(*character_codes_begin);
        *output_begin = fetch_glyph(tp, group, v);
      }
  }

  template<typename input_iterator,
           typename output_iterator>
  void
  GlyphSelector::
  create_glyph_sequence(GlyphRender tp, FontBase::const_handle h,
                        input_iterator character_codes_begin,
                        input_iterator character_codes_end,
                        output_iterator output_begin)
  {
    for(;character_codes_begin != character_codes_end; ++character_codes_begin, ++output_begin)
      {
        uint32_t v;
        v = static_cast<uint32_t>(*character_codes_begin);
        *output_begin = fetch_glyph(tp, h, v);
      }
  }

  template<typename input_iterator,
           typename output_iterator>
  void
  GlyphSelector::
  create_glyph_sequence_no_merging(GlyphRender tp,
                                   FontBase::const_handle h,
                                   input_iterator character_codes_begin,
                                   input_iterator character_codes_end,
                                   output_iterator output_begin)
  {
    for(;character_codes_begin != character_codes_end; ++character_codes_begin, ++output_begin)
      {
        uint32_t v;
        v = static_cast<uint32_t>(*character_codes_begin);
        *output_begin = fetch_glyph_no_merging(tp, h, v);
      }
  }

/*! @} */
}
