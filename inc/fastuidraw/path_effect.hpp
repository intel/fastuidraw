/*!
 * \file path_effect.hpp
 * \brief file path_effect.hpp
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
 *
 */

#pragma once

#include <fastuidraw/partitioned_tessellated_path.hpp>

namespace fastuidraw
{
/*!\addtogroup Paths
 * @{
 */

  /*!
   * \brief
   * A \ref PathEffect allows one to process a sequence of
   * \ref PartitionedTessellatedPath::segment_chain, \ref
   * TessellatedPath::join and \ref
   * TessellatedPath::cap values to produce a
   * new sequence of such values for the purpose of effecting
   * stroking.
   */
  class PathEffect:noncopyable
  {
  public:
    /*!
     * Conveniance typedef
     */
    typedef PartitionedTessellatedPath::segment_chain segment_chain;

    /*!
     * Conveniance typedef
     */
    typedef TessellatedPath::segment segment;

    /*!
     * Conveniance typedef
     */
    typedef TessellatedPath::join join;

    /*!
     * Conveniance typedef
     */
    typedef TessellatedPath::cap cap;

    /*!
     * \brief
     * A \ref Storage is to where \ref TessellatedPath::segment,
     * \ref TessellatedPath::join and \ref TessellatedPath::cap
     * values are stored from being processed by a \ref PathEffect.
     */
    class Storage:noncopyable
    {
    public:
      Storage(void);
      ~Storage();

      /*!
       * Clear the Storage of all content
       */
      void
      clear(void);

      /*!
       * Begin a \ref segment_chain.
       * \param prev_segment if non-null, then the chain will have
       *                     that \ref segment_chain::m_prev_to_start
       *                     will point to a -COPY- of *prev_segment.
       */
      Storage&
      begin_chain(const TessellatedPath::segment *prev_segment);

      /*!
       * Add a segment to the current chain being built.
       * \param segment segment to add
       */
      Storage&
      add_segment(const TessellatedPath::segment &segment);

      /*!
       * Add a join to the \ref Storage
       * \param join \ref TessellatedPath::join to add
       */
      Storage&
      add_join(const join &join);

      /*!
       * Add a cap to the \ref Storage
       * \param cap \ref TessellatedPath::cap to add
       */
      Storage&
      add_cap(const cap &cap);

      /*!
       * returns the number of \ref segment_chain the
       * \ref Storage has.
       */
      unsigned int
      number_chains(void) const;

      /*!
       * Returns the named \ref segment_chain of the storage.
       * The return value is only guaranteed to be valid until
       * the next call to add_segment() or begin_chain().
       * \param I which \ref segment_chain with 0 <= I <= number_chains().
       */
      segment_chain
      chain(unsigned int I) const;

      /*!
       * Returns the joins of the \ref Storage added by add_join().
       * The return value is only guaranteed to be valid until the
       * next call to add_join().
       */
      c_array<const join>
      joins(void) const;

      /*!
       * Returns the caps of the \ref Storage added by add_cap().
       * The return value is only guaranteed to be valid until the
       * next call to add_cap().
       */
      c_array<const cap>
      caps(void) const;

    private:
      void *m_d;
    };

    virtual
    ~PathEffect()
    {}

    /*!
     * Calls \ref process_chains(), \ref process_joins() and \ref process_caps()
     * on the elements of a \ref PartitionedTessellatedPath::SubsetSelection.
     * \param selection \ref PartitionedTessellatedPath::SubsetSelection to process
     * \param dst \ref Storage on which to place values
     */
    void
    process_selection(const PartitionedTessellatedPath::SubsetSelection &selection,
                      Storage &dst) const;

    /*!
     * Provided as a template conveniance, equivalent to
     * \code
     * for (; begin != end; ++begin)
     *   {
     *     process_chain(*begin, dst);
     *   }
     * \endcode
     * \param begin iterator to first element to process
     * \param end iterator to one past last element to process
     * \param dst \ref Storage on which to place values
     */
    template<typename iterator>
    void
    process_chains(iterator begin, iterator end, Storage &dst) const
    {
      for (; begin != end; ++begin)
        {
          process_chain(*begin, dst);
        }
    }

    /*!
     * Provided as a template conveniance, equivalent to
     * \code
     * for (; begin != end; ++begin)
     *   {
     *     process_join(*begin, dst);
     *   }
     * \endcode
     * \param begin iterator to first element to process
     * \param end iterator to one past last element to process
     * \param dst \ref Storage on which to place values
     */
    template<typename iterator>
    void
    process_joins(iterator begin, iterator end, Storage &dst) const
    {
      for (; begin != end; ++begin)
        {
          process_join(*begin, dst);
        }
    }

    /*!
     * Provided as a template conveniance, equivalent to
     * \code
     * for (; begin != end; ++begin)
     *   {
     *     process_cap(*begin, dst);
     *   }
     * \endcode
     * \param begin iterator to first element to process
     * \param end iterator to one past last element to process
     * \param dst \ref Storage on which to place values
     */
    template<typename iterator>
    void
    process_caps(iterator begin, iterator end, Storage &dst) const
    {
      for (; begin != end; ++begin)
        {
          process_cap(*begin, dst);
        }
    }

    /*!
     * To be implemented by a derived class to process a \ref
     * segment_chain value placing the results onto a \ref
     * PathEffect::Storage
     * \param chain \ref segment_chain value to process
     * \param dst \ref PathEffect::Storage on which to place values
     */
    virtual
    void
    process_chain(const segment_chain &chain, Storage &dst) const = 0;

    /*!
     * To be implemented by a derived class to process a \ref
     * TessellatedPath::join value placing the results onto a
     * \ref PathEffect::Storage
     * \param join \ref TessellatedPath::join value to process
     * \param dst \ref PathEffect::Storage on which to place values
     */
    virtual
    void
    process_join(const TessellatedPath::join &join, Storage &dst) const = 0;

    /*!
     * To be implemented by a derived class to process a \ref
     * TessellatedPath::cap value placing the results onto a
     * \ref PathEffect::Storage
     * \param cap \ref TessellatedPath::cap value to process
     * \param dst \ref PathEffect::Storage on which to place values
     */
    virtual
    void
    process_cap(const TessellatedPath::cap &cap, Storage &dst) const = 0;
  };

/*! @} */
}
