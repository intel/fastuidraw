/*!
 * \file painter_packed_value_pool_private.hpp
 * \brief file painter_packed_value_pool_private.hpp
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

#include <vector>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/painter/backend/painter_surface.hpp>
#include <private/util_private.hpp>

namespace fastuidraw
{
  namespace detail
  {
    namespace PackedValuePoolHelper
    {
      template<typename T>
      class FillData
      {
      public:
        static
        void
        fetch_data(const T &src, std::vector<generic_data> *dst)
        {
          dst->resize(src.data_size());
          src.pack_data(fastuidraw::make_c_array(*dst));
        }
      };

      template<typename T>
      class CopyData
      {
      public:
        static
        void
        fetch_data(const T &src,
                   std::vector<generic_data> *dst)
        {
          c_array<const generic_data> src_data(src.packed_data());
          dst->resize(src_data.size());
          std::copy(src_data.begin(), src_data.end(), dst->begin());
        }
      };

      template<typename T>
      class DataType
      {
      public:
        typedef CopyData<T> Type;
      };

      template<>
      class DataType<PainterClipEquations>
      {
      public:
        typedef FillData<PainterClipEquations> Type;
      };

      template<>
      class DataType<PainterItemMatrix>
      {
      public:
        typedef FillData<PainterItemMatrix> Type;
      };

      template<>
      class DataType<PainterBrushAdjust>
      {
      public:
        typedef FillData<PainterBrushAdjust> Type;
      };
    }

    class PackedValuePoolBase:public reference_counted<PackedValuePoolBase>::non_concurrent
    {
    public:
      /* PainterPackedValueBase holds a pointer to an Element, thus
       * we cannot have a vector because resizing the vector would
       * move the Element to another location in memory. Instead
       * we have an array of pointers to vecN of Elements. Which
       * vecN is m_bucket and which element of the Bucket is
       * m_element_of_bucket.
       */
      class Slot
      {
      public:
        Slot(void):
          m_bucket(-1),
          m_element_of_bucket(-1)
        {}

        bool
        operator==(const Slot &rhs) const
        {
          return m_bucket == rhs.m_bucket
            && m_element_of_bucket == rhs.m_element_of_bucket;
        }

        bool
        valid(void) const
        {
          return m_bucket >= 0
            && m_element_of_bucket >= 0;
        }

        int m_bucket;
        int m_element_of_bucket;
      };

      /* A little explanation is in order. A fixed ElementBase always
       * is of the same PackedValuePoolBase; however the reference being
       * non-null indicates that is in use by a PainterPackedValueBase
       * object. Thus, the underlying PackedValuePoolBase cannot go
       * out of scope either. The release() function will release the
       * reference which might trigger the dtor of the PackedValuePoolBase
       * which would then rigger the dtor of the ElementBase object.
       */
      class ElementBase
      {
      public:
        explicit
        ElementBase(void):
          m_painter(nullptr),
          m_raw_data(nullptr), //should be set by derived class
          m_ref_count(0)
        {}

        void
        acquire(void)
        {
          FASTUIDRAWassert(m_pool);
          FASTUIDRAWassert(m_pool_slot.valid());
          FASTUIDRAWassert(m_ref_count >= 0);
          ++m_ref_count;
        }

        static
        void
        release(ElementBase *p)
        {
          FASTUIDRAWassert(p->m_pool);
          FASTUIDRAWassert(p->m_pool_slot.valid());
          FASTUIDRAWassert(p->m_ref_count >= 0);

          --p->m_ref_count;
          if (p->m_ref_count == 0)
            {
              p->m_pool->m_free_slots.push_back(p->m_pool_slot);
              /* Reseting the reference to the pool might trigger its
               * deconstruction which then would trigger p's dtor as
               * well.
               */
              p->m_pool = nullptr;
              p = nullptr;
            }
        }

        int
        ref_count(void) const
        {
          return m_ref_count;
        }

        /* To what PainterPacker and where in data store buffer
         * already packed into PainterDraw::m_store; we track
         * a PainterPacker for each Surface::render_type_t.
         */
        vecN<const void *, PainterSurface::number_buffer_types> m_painter;
        std::vector<generic_data> m_data;
        vecN<unsigned int, PainterSurface::number_buffer_types> m_draw_command_id, m_offset;
        const void *m_raw_data;

      protected:
        void
        initialize_common(Slot slot, PackedValuePoolBase *p)
        {
          FASTUIDRAWassert(slot == m_pool_slot || m_pool_slot.m_bucket == -1);
          m_pool = p;
          m_pool_slot = slot;
          m_draw_command_id.fill(-1);
          m_offset.fill(0);
          m_painter.fill(nullptr);
        }

      private:
        reference_counted_ptr<PackedValuePoolBase> m_pool;
        Slot m_pool_slot;
        int m_ref_count;
      };

    protected:
      std::vector<Slot> m_free_slots;
    };

    /* The intention is that a fixed Pool is for a fixed Painter, which is
     * not thread safe, so PackedValuePool is not thread safe either.
     */
    template<typename T>
    class PackedValuePool:public PackedValuePoolBase
    {
    public:
      class Element:public PackedValuePoolBase::ElementBase
      {
      public:
        Element(void)
        {
          m_raw_data = &m_state;
        }

        void
        initialize(const T &st,
                   PackedValuePoolBase::Slot slot,
                   PackedValuePoolBase *p)
        {
          this->initialize_common(slot, p);
          m_state = st;
          GetData::fetch_data(m_state, &m_data);
        }

      private:
        typedef typename PackedValuePoolHelper::DataType<T>::Type GetData;
        T m_state;
      };

      class Holder
      {
      public:
        Holder(void)
        {
          m_p = FASTUIDRAWnew PackedValuePool();
        }

        Element*
        allocate(const T &st)
        {
          return m_p->allocate(st);
        }

      private:
        reference_counted_ptr<PackedValuePool> m_p;
      };

      ~PackedValuePool()
      {
        FASTUIDRAWassert(m_free_slots.size() == m_data.size() * Bucket::array_size);
        for (Bucket *p : m_data)
          {
            FASTUIDRAWdelete(p);
          }
      }

    private:
      PackedValuePool(void)
      {}

      /* Returning nullptr indicates no free entries left in the pool */
      Element*
      allocate(const T &st)
      {
        Element *return_value(nullptr);
        PackedValuePoolBase::Slot slot;

        if (m_free_slots.empty())
          {
            create_bucket();
          }

        FASTUIDRAWassert(!this->m_free_slots.empty());
        slot = this->m_free_slots.back();
        this->m_free_slots.pop_back();

        Bucket &bucket(*m_data[slot.m_bucket]);

        return_value = &bucket[slot.m_element_of_bucket];
        return_value->initialize(st, slot, this);
        FASTUIDRAWassert(return_value->ref_count() == 0);

        return return_value;
      }

      void
      create_bucket(void)
      {
        PackedValuePoolBase::Slot s;

        s.m_bucket = m_data.size();
        m_data.push_back(FASTUIDRAWnew Bucket());
        for (int i = Bucket::array_size - 1; i >=0; --i)
          {
            s.m_element_of_bucket = i;
            this->m_free_slots.push_back(s);
          }
      }

      typedef vecN<Element, 1024> Bucket;
      std::vector<Bucket*> m_data;
    };
  }
}
