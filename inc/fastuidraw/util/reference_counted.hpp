/*!
 * \file reference_counted.hpp
 * \brief file reference_counted.hpp
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
#include <fastuidraw/util/fastuidraw_memory.hpp>

#include <fastuidraw/util/reference_count_atomic.hpp>
#include <fastuidraw/util/reference_count_non_concurrent.hpp>

/*!
 * All functionality of FastUIDraw is in the namespace fastuidraw.
 */
namespace fastuidraw
{
/*!\addtogroup Utility
 * @{
 */

  /*!
   * \brief
   * A wrapper over a pointer to implement reference counting.
   *
   * The class T must implement the static methods
   *  - T::add_reference(const T*)
   *  - T::remove_reference(const T*)
   *
   * where T::add_reference() increment the reference count and
   * T::remove_reference() decrements the reference count and will
   * delete the object.
   *
   * See also reference_counted_base and reference_counted.
   */
  template<typename T>
  class reference_counted_ptr
  {
  private:
    typedef void (reference_counted_ptr::*unspecified_bool_type)(void) const;

    void
    fake_function(void) const
    {}

  public:
    /*!
     * Ctor, inits the reference_counted_ptr as
     * equivalent to nullptr.
     */
    reference_counted_ptr(void):
      m_p(nullptr)
    {}

    /*!
     * Ctor, initialize from a T*. If passed non-nullptr,
     * then the reference counter is incremented
     * (via T::add_reference()).
     * \param p pointer value from which to initialize
     */
    reference_counted_ptr(T *p):
      m_p(p)
    {
      if (m_p)
        {
          T::add_reference(m_p);
        }
    }

    /*!
     * Copy ctor.
     * \param obj value from which to initialize
     */
    reference_counted_ptr(const reference_counted_ptr &obj):
      m_p(obj.get())
    {
      if (m_p)
        {
          T::add_reference(m_p);
        }
    }

    /*!
     * Ctor from a reference_counted_ptr<U> where U* is
     * implicitely convertible to a T*.
     * \tparam U type where U* is implicitely convertible to a T*.
     * \param obj value from which to initialize
     */
    template<typename U>
    reference_counted_ptr(const reference_counted_ptr<U> &obj):
      m_p(obj.get())
    {
      if (m_p)
        {
          T::add_reference(m_p);
        }
    }

    /*!
     * Move ctor.
     * \param obj object from which to take.
     */
    reference_counted_ptr(reference_counted_ptr &&obj):
      m_p(obj.m_p)
    {
      obj.m_p = nullptr;
    }

    /*!
     * Dtor, if pointer is non-nullptr, then reference is decremented
     * (via T::remove_reference()).
     */
    ~reference_counted_ptr()
    {
      if (m_p)
        {
          T::remove_reference(m_p);
        }
    }

    /*!
     * Assignment operator
     * \param rhs value from which to assign
     */
    reference_counted_ptr&
    operator=(const reference_counted_ptr &rhs)
    {
      reference_counted_ptr temp(rhs);
      temp.swap(*this);
      return *this;
    }

    /*!
     * Assignment operator from a T*.
     * \param rhs value from which to assign
     */
    reference_counted_ptr&
    operator=(T *rhs)
    {
      reference_counted_ptr temp(rhs);
      temp.swap(*this);
      return *this;
    }

    /*!
     * Move assignment operator
     * \param rhs value from which to assign
     */
    reference_counted_ptr&
    operator=(reference_counted_ptr &&rhs)
    {
      if (m_p)
        {
          T::remove_reference(m_p);
        }
      m_p = rhs.m_p;
      rhs.m_p = nullptr;
      return *this;
    }

    /*!
     * Assignment operator from a reference_counted_ptr<U>
     * where U* is implicitely convertible to a T*.
     * \tparam U type where U* is implicitely convertible to a T*.
     * \param rhs value from which to assign
     */
    template<typename U>
    reference_counted_ptr&
    operator=(const reference_counted_ptr<U> &rhs)
    {
      reference_counted_ptr temp(rhs);
      temp.swap(*this);
      return *this;
    }

    /*!
     * Returns the underlying pointer
     */
    T*
    get(void) const
    {
      return m_p;
    }

    /*!
     * Overload of dererefence operator. Under debug build,
     * assers if pointer is nullptr.
     */
    T&
    operator*(void) const
    {
      FASTUIDRAWassert(m_p);
      return *m_p;
    }

    /*!
     * Overload of operator. Under debug build,
     * assers if pointer is nullptr.
     */
    T*
    operator->(void) const
    {
      FASTUIDRAWassert(m_p);
      return m_p;
    }

    /*!
     * Performs swap without needing to increment or
     * decrement the reference counter.
     * \param rhs object with which to swap
     */
    void
    swap(reference_counted_ptr &rhs)
    {
      T *temp;
      temp = rhs.m_p;
      rhs.m_p = m_p;
      m_p = temp;
    }

    /*!
     * Allows one to legally write:
     * \code
     * reference_counted_ptr<T> p;
     *
     * if (p)
     *  {
     *  }
     *
     * if (!p)
     *  {
     *  }
     * \endcode
     */
    operator unspecified_bool_type() const
    {
      return m_p ? &reference_counted_ptr::fake_function : 0;
    }

    /*!
     * Equality comparison operator to a pointer value.
     * \param rhs value with which to compare against
     */
    template<typename U>
    bool
    operator==(U *rhs) const
    {
      return m_p == rhs;
    }

    /*!
     * Equality comparison operator to a
     * reference_counted_ptr value.
     * \tparam U type where U* is implicitely comparable to a T*.
     * \param rhs value with which to compare against
     */
    template<typename U>
    bool
    operator==(const reference_counted_ptr<U> &rhs) const
    {
      return m_p == rhs.get();
    }

    /*!
     * Inequality comparison operator to a pointer value.
     * \tparam U type where U* is implicitely comparable to a T*.
     * \param rhs value with which to compare against
     */
    template<typename U>
    bool
    operator!=(U *rhs) const
    {
      return m_p != rhs;
    }

    /*!
     * Inequality comparison operator to
     * a reference_counted_ptr value.
     * \tparam U type where U* is implicitely comparable to a T*.
     * \param rhs value with which to compare against
     */
    template<typename U>
    bool
    operator!=(const reference_counted_ptr<U> &rhs) const
    {
      return m_p != rhs.get();
    }

    /*!
     * Comparison operator for sorting.
     * \param rhs value with which to compare against
     */
    bool
    operator<(const reference_counted_ptr &rhs) const
    {
      return get() < rhs.get();
    }

    /*!
     * Clears the reference_counted_ptr object,
     * equivalent to
     * \code
     * operator=(reference_counted_ptr());
     * \endcode
     */
    void
    clear(void)
    {
      if (m_p)
        {
          T::remove_reference(m_p);
          m_p = nullptr;
        }
    }
    /*!
     * Perform static cast to down cast (uses static_cast internally).
     * \tparam U type to which to cast
     */
    template<typename U>
    reference_counted_ptr<U>
    static_cast_ptr(void) const
    {
      return static_cast<U*>(get());
    }

    /*!
     * Perform const cast to cast (uses const_cast internally).
     * \tparam U type to which to cast
     */
    template<typename U>
    reference_counted_ptr<U>
    const_cast_ptr(void) const
    {
      return const_cast<U*>(get());
    }

    /*!
     * Perform dynamic cast to down cast (uses dynamic_cast internally).
     * \tparam U type to which to cast
     */
    template<typename U>
    reference_counted_ptr<U>
    dynamic_cast_ptr(void) const
    {
      return dynamic_cast<U*>(get());
    }

  private:
    T *m_p;
  };

  /*!
   * Equality comparison operator
   * \param lhs left hand side of operator
   * \param rhs right hand side of operator
   */
  template<typename T, typename S>
  inline
  bool
  operator==(T *lhs,
             const reference_counted_ptr<S> &rhs)
  {
    return lhs == rhs.get();
  }

  /*!
   * Inequality comparison operator
   * \param lhs left hand side of operator
   * \param rhs right hand side of operator
   */
  template<typename T, typename S>
  inline
  bool
  operator!=(T *lhs,
             const reference_counted_ptr<S> &rhs)
  {
    return lhs != rhs.get();
  }

  /*!
   * swap() routine, equivalent to
   * \code
   * lhs.swap(rhs);
   * \endcode
   * \param lhs object to swap
   * \param rhs object to swap
   */
  template<typename T>
  inline
  void
  swap(reference_counted_ptr<T> &lhs,
       reference_counted_ptr<T> &rhs)
  {
    lhs.swap(rhs);
  }

  /*!
   * \brief
   * Base class to use for reference counted objects,
   * for using reference_counted_ptr. See also \ref reference_counted.
   * Object deletion (when the reference count goes to zero) is performed
   * via \ref FASTUIDRAWdelete. As a consequence of using \ref
   * FASTUIDRAWdelete, objects must be created with \ref FASTUIDRAWnew.
   *
   * \tparam T object type that is reference counted
   * \tparam Counter object type to perform reference counting.
   *
   * The type Counter must expose the methods:
   * - void add_reference() to increment the counter
   * - bool remove_reference() to decrement the counter and return true
   *   if the counter is zero after the decrement operation.
   */
  template<typename T, typename Counter>
  class reference_counted_base:noncopyable
  {
  public:
    /*!
     * Empty ctor.
     */
    reference_counted_base(void)
    {}

    virtual
    ~reference_counted_base()
    {}

    /*!
     * Adds a reference count to an object.
     * \param p pointer to object to which to add reference count.
     */
    static
    void
    add_reference(const reference_counted_base<T, Counter> *p)
    {
      FASTUIDRAWassert(p);
      p->m_counter.add_reference();
    }

    /*!
     * Removes a reference count to an object, if the reference
     * count is 0, then deletes the object with \ref FASTUIDRAWdelete.
     * \param p pointer to object from which to remove reference count.
     */
    static
    void
    remove_reference(const reference_counted_base<T, Counter> *p)
    {
      FASTUIDRAWassert(p);
      if (p->m_counter.remove_reference())
        {
          reference_counted_base<T, Counter> *q;
          q = const_cast<reference_counted_base<T, Counter>*>(p);
          FASTUIDRAWdelete(q);
        }
    }
  private:
    mutable Counter m_counter;
  };

  /*!
   * \brief
   * Defines default reference counting base classes.
   *
   * See also reference_counted_base and reference_counted_ptr.
   */
  template<typename T>
  class reference_counted
  {
  public:
    /*!
     * \brief
     * Typedef to reference counting which is NOT thread safe
     */
    typedef reference_counted_base<T, reference_count_non_concurrent> non_concurrent;

    /*!
     * \brief
     * Typedef to reference counting which is thread safe by atomically
     * adding and removing reference
     */
    typedef reference_counted_base<T, reference_count_atomic> concurrent;
  };
/*! @} */

} //namespace fastuidraw
