/*!
 * \file font.hpp
 * \brief file font.hpp
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

#include <fastuidraw/util/util.hpp>

namespace fastuidraw
{
/*!\addtogroup Glyph
 * @{
 */

/*!\def FASTUIDRAW_CHARACTER_ENCODING_VALUE
 * Conveniance macro to define a 32-bit character encoding scheme
 * from four unsigned 8-bit values.
 */
#define FASTUIDRAW_CHARACTER_ENCODING_VALUE(a, b, c, d) \
  (uint32_t(a) << 24u) | (uint32_t(b) << 16u) | (uint32_t(c) << 8u) | uint32_t(d)

  /*!\brief
   * A CharacterEncoding is used to decide how to interpret
   * characters. The value itself is just a 32-bit value that
   * mirrors the FT_Encoding of FreeType.
   */
  namespace CharacterEncoding
  {
    /*!
     * \brief
     * Enumeration type to define character encodings.
     */
    enum encoding_value_t:uint32_t
      {
        /*!
         * Unicode character set to cover all version of Unicode.
         */
        unicode = FASTUIDRAW_CHARACTER_ENCODING_VALUE('u', 'n', 'i', 'c'),

        /*!
         * Miscrosoft Symbol Encoding; uses the character codes
         * 0xF020 - 0xF0FF.
         */
        ms_symbol = FASTUIDRAW_CHARACTER_ENCODING_VALUE('s', 'y', 'm', 'b'),

        /*!
         * Shift JIS encoding for Japanese characters.
         */
        sjis = FASTUIDRAW_CHARACTER_ENCODING_VALUE('s', 'j', 'i', 's'),

        /*!
         * Character encoding for the Simplified Chinese of the
         * People's Republic of China.
         */
        prc  = FASTUIDRAW_CHARACTER_ENCODING_VALUE('g', 'b', ' ', ' '),

        /*!
         * Characted encoding for Traditional Chinese of Taiwan
         * and Hong Kong
         */
        big5 = FASTUIDRAW_CHARACTER_ENCODING_VALUE('b', 'i', 'g', '5'),

        /*!
         * Encoding of the Korean characters as Extended Wansung
         * (MS Windows code page 949). See also
         * https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WindowsBestFit/bestfit949.txt
         */
        wansung = FASTUIDRAW_CHARACTER_ENCODING_VALUE('w', 'a', 'n', 's'),

        /*!
         * The Korean standard character set (KS C 5601-1992).
         * This corrresponds to MS Windows code page 1361.
         */
        johab = FASTUIDRAW_CHARACTER_ENCODING_VALUE('j', 'o', 'h', 'a'),

        /*!
         * Latin-1 character encoding as defined by Type 1 PostScript font,
         * limited to 256 character codes.
         */
        adobe_latin1 = FASTUIDRAW_CHARACTER_ENCODING_VALUE('l', 'a', 't', '1'),

        /*!
         * Adobe Standard character encoding found in Type 1, CFF and
         * OpenType/CFF fonts, limited to 256 character codes.
         */
        adobe_standard = FASTUIDRAW_CHARACTER_ENCODING_VALUE('A', 'D', 'O', 'B'),

        /*!
         * Adobe Expert character encoding found in Type 1, CFF and
         * OpenType/CFF fonts, limited to 256 character codes.
         */
        adobe_expert = FASTUIDRAW_CHARACTER_ENCODING_VALUE('A', 'D', 'B', 'E'),

        /*!
         * Custom character encoding found in Type 1, CFF and
         * OpenType/CFF fonts, limited to 256 character codes.
         */
        adobe_custom = FASTUIDRAW_CHARACTER_ENCODING_VALUE('A', 'D', 'B', 'C'),

        /*!
         * Apply Roman character encoding; a number of TrueType and
         * OpenType fonts have this 8-bit encoding because quite
         * older version of Mac OS support it.
         */
        apple_roman = FASTUIDRAW_CHARACTER_ENCODING_VALUE('a', 'r', 'm', 'n'),
    };

    /*!
     * Conveniance functions to create a CharacterEncoding value
     * from an arbitary 4-tuple of uint8_t values.
     */
    inline
    enum encoding_value_t
    chracter_encoding(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
    {
      uint32_t v;
      v = FASTUIDRAW_CHARACTER_ENCODING_VALUE(a, b, c, d);
      return static_cast<enum encoding_value_t>(v);
    }
  }

}
