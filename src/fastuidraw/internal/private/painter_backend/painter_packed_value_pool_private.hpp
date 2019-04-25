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

#pragma once

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
      class NoResources
      {
      public:
        static
        void
        fetch_resources(const T &,
                        std::vector<reference_counted_ptr<const resource_base> > *dst)
        {
          dst->clear();
        }
      };

      template<typename T>
      class HasResources
      {
      public:
        static
        void
        fetch_resources(const T &src,
                        std::vector<reference_counted_ptr<const resource_base> > *dst)
        {
          dst->resize(src.number_resources());
          src.save_resources(make_c_array(*dst));
        }
      };

      template<typename T>
      class NoBindImages
      {
      public:
        static
        void
        fetch_bind_images(const T &,
                          std::vector<reference_counted_ptr<const Image> > *dst)
        {
          dst->clear();
        }
      };

      template<typename T>
      class HasBindImages
      {
      public:
        static
        void
        fetch_bind_images(const T &src,
                          std::vector<reference_counted_ptr<const Image> > *dst)
        {
          c_array<const reference_counted_ptr<const Image> > src_array(src.bind_images());
          dst->resize(src_array.size());
          std::copy(src_array.begin(), src_array.end(), dst->begin());
        }
      };

      template<typename T>
      class NoLocalCopy
      {
      public:
        void
        copy_value(const T&)
        {
        }
      };

      template<typename T>
      class HasLocalCopy
      {
      public:
        T m_v;

        void
        copy_value(const T &src)
        {
          m_v = src;
        }
      };

      template<typename T>
      class DataType
      {};

      template<>
      class DataType<PainterClipEquations>
      {
      public:
        typedef NoBindImages<PainterClipEquations> BindImageType;
        typedef NoResources<PainterClipEquations> ResourceType;
        typedef HasLocalCopy<PainterClipEquations> CopyType;
      };

      template<>
      class DataType<PainterItemMatrix>
      {
      public:
        typedef NoBindImages<PainterItemMatrix> BindImageType;
        typedef NoResources<PainterItemMatrix> ResourceType;
        typedef HasLocalCopy<PainterItemMatrix> CopyType;
      };

      template<>
      class DataType<PainterBrushAdjust>
      {
      public:
        typedef NoBindImages<PainterBrushAdjust> BindImageType;
        typedef NoResources<PainterBrushAdjust> ResourceType;
        typedef NoLocalCopy<PainterBrushAdjust> CopyType;
      };

      template<>
      class DataType<PainterItemShaderData>
      {
      public:
        typedef NoBindImages<PainterItemShaderData> BindImageType;
        typedef HasResources<PainterItemShaderData> ResourceType;
        typedef NoLocalCopy<PainterItemShaderData> CopyType;
      };

      template<>
      class DataType<PainterBlendShaderData>
      {
      public:
        typedef NoBindImages<PainterBlendShaderData> BindImageType;
        typedef HasResources<PainterBlendShaderData> ResourceType;
        typedef NoLocalCopy<PainterBlendShaderData> CopyType;
      };

      template<>
      class DataType<PainterBrushShaderData>
      {
      public:
        typedef HasBindImages<PainterBrushShaderData> BindImageType;
        typedef HasResources<PainterBrushShaderData> ResourceType;
        typedef NoLocalCopy<PainterBrushShaderData> CopyType;
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
        vecN<const void*, PainterSurface::number_buffer_types> m_painter;
        std::vector<generic_data> m_data;
        std::vector<reference_counted_ptr<const resource_base> > m_resources;
        std::vector<reference_counted_ptr<const Image> > m_bind_images;
        vecN<unsigned int, PainterSurface::number_buffer_types> m_draw_command_id, m_offset;

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
        void
        initialize(const T &st,
                   PackedValuePoolBase::Slot slot,
                   PackedValuePoolBase *p)
        {
          this->initialize_common(slot, p);
          m_data.resize(st.data_size());
          st.pack_data(make_c_array(m_data));
          ResourceType::fetch_resources(st, &m_resources);
          BindImageType::fetch_bind_images(st, &m_bind_images);
          m_copy.copy_value(st);
        }

        const T&
        unpacked_value(void) const
        {
          return m_copy.m_v;
        }

      private:
        typedef typename PackedValuePoolHelper::DataType<T> GetType;
        typedef typename GetType::ResourceType ResourceType;
        typedef typename GetType::BindImageType BindImageType;
        typedef typename GetType::CopyType CopyType;

        CopyType m_copy;
      };

      class ElementHandle
      {
      public:
        ElementHandle(Element *d = nullptr):
          m_d(d)
        {
          if (m_d)
            {
              m_d->acquire();
            }
        }

        ElementHandle(const ElementHandle &obj):
          m_d(obj.m_d)
        {
          if (m_d)
            {
              m_d->acquire();
            }
        }

        ~ElementHandle()
        {
          if (m_d)
            {
              ElementBase::release(m_d);
            }
        }

        void
        swap(ElementHandle &obj)
        {
          std::swap(m_d, obj.m_d);
        }

        ElementHandle&
        operator=(const ElementHandle &obj)
        {
          if (m_d != obj.m_d)
            {
              ElementHandle v(obj);
              swap(v);
            }
          return *this;
        }

        void
        reset(void)
        {
          if (m_d)
            {
              ElementBase::release(m_d);
            }
          m_d = nullptr;
        }

        const T&
        unpacked_value(void) const
        {
          FASTUIDRAWassert(m_d);
          return m_d->unpacked_value();
        }

        operator
        ElementBase*() const { return m_d; }

      private:
        Element *m_d;
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
