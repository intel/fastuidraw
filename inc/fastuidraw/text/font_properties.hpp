/*!
 * \file font_properties.hpp
 * \brief file font_properties.hpp
 *
 * Adapted from: WRATHFontDatabase.hpp of WRATH:
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
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */

#ifndef FASTUIDRAW_FONT_PROPERTIES_HPP
#define FASTUIDRAW_FONT_PROPERTIES_HPP

#include <fastuidraw/util/util.hpp>

namespace fastuidraw
{
/*!\addtogroup Glyph
 * @{
 */

  /*!
   * \brief
   * Represents defining properties of a font
   * used by FontDatabase to perform font
   * merging.
   */
  class FontProperties
  {
  public:
    /*!
     * Ctor. Initializes bold() as false,
     * italic() as false and all string values
     * as empty strings.
     */
    FontProperties(void);

    /*!
     * Copy ctor.
     * \param obj vaue from which to copy
     */
    FontProperties(const FontProperties &obj);

    ~FontProperties();

    /*!
     * assignment operator
     * \param obj value from which to assign
     */
    FontProperties&
    operator=(const FontProperties &obj);

    /*!
     * Swap operation
     * \param obj object with which to swap
     */
    void
    swap(FontProperties &obj);

    /*!
     * Specifies if the font is to be bold or not;
     * this value is overridden by the value of
     * style() if style() is a non-empty string.
     */
    bool
    bold(void) const;

    /*!
     * Set if the font is to be bold or not.
     * \param b value to which to set bold(void) const
     */
    FontProperties&
    bold(bool b);

    /*!
     * Specifies if the font is to be italic or not;
     * this value is overridden by the value of
     * style() if style() is a non-empty string.
     */
    bool
    italic(void) const;

    /*!
     * Set if the font is to be italic or not.
     * \param b value to which to set italic(void) const
     */
    FontProperties&
    italic(bool b);

    /*!
     * Specifies the style name of the font.
     * Examples are "Bold", "Bold Italic",
     * "Book", "Condensed", "Condensed Bold Obliquie".
     * The value for style is NOT orthogonal to
     * the value of italic() and bold().
     * For example, under a standard GNU/Linux system
     * the style names "Condensed Bold Oblique",
     * "Condensed Oblique", "Condensed Bold"
     * and "Condensed" give different fonts for
     * the family name "DejaVu Serif". If style()
     * is a non-empty string, the then it overrides
     * both italic() and bold().
     */
    c_string
    style(void) const;

    /*!
     * Set the value returned by style(void) const
     * \param s value to which to set style(void) const
     */
    FontProperties&
    style(c_string s);

    /*!
     * Specifies the family name of the font, for example "Sans"
     */
    c_string
    family(void) const;

    /*!
     * Set the value returned by family(void) const
     * \param s value to which to set family(void) const
     */
    FontProperties&
    family(c_string s);

    /*!
     * Specifies the foundry name of the
     * font, i.e. the maker of the font.
     * Some systems (for example those using
     * fontconfig) this value is ignored.
     */
    c_string
    foundry(void) const;

    /*!
     * Set the value returned by foundry(void) const
     * \param s value to which to set foundry(void) const
     */
    FontProperties&
    foundry(c_string s);

    /*!
     * Specifies the source of the font, for those
     * fonts coming from file names should be a string
     * giving the filename and face index with a
     * colon seperating them, for example "foo:0"
     * indicates from file foo and the face index
     * is 0.
     */
    c_string
    source_label(void) const;

    /*!
     * Set the value returned by source_label(void) const
     * \param s value to which to set source_label(void) const
     */
    FontProperties&
    source_label(c_string s);

    /*!
     * Set the value returned by source_label(void) const
     * to refer to a face index of a font file. Equivalent
     * in function to
     * \code
     * std::ostringstream str;
     * str << filename << ":" << face_index;
     * source_label(str.str().c_str());
     * \endcode
     * \param filename name of file holding the font
     * \param face_index index of face within the file.
     */
    FontProperties&
    source_label(c_string filename, int face_index);

  private:
    void *m_d;
  };
/*! @} */
}

#endif
