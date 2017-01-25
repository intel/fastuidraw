/*!
 * \file signal.hpp
 * \brief file signal.hpp
 *
 * Copyright 2017 by Yusuke Suzuki.
 *
 * Contact: utatane.tea@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Yusuke Suzuki <utatane.tea@gmail.com>
 *
 */


#include <list>
#include <memory>

#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/util/reference_count_non_concurrent.hpp>
#include <fastuidraw/util/util.hpp>

#pragma once

namespace fastuidraw
{

/*!\class Signal
  Simple Signal implementation.
*/
template<typename Function>
class Signal:public noncopyable
{
public:
  typedef Function SlotType;

  class Slots:
    public fastuidraw::reference_counted<Slots>::non_concurrent,
    public std::list<SlotType>
  {
  };

  Signal():
    m_slots(FASTUIDRAWnew Slots())
  { }

  class Connection
  {
  public:
    Connection(fastuidraw::reference_counted_ptr<Slots>& slots, typename Slots::iterator iterator):
      m_slots(slots),
      m_iterator(iterator)
    { }

    /*!
      Disconnect the slot.
     */
    void
    disconnect()
    {
      if(!m_slots)
        {
          return;
        }
      m_slots->erase(m_iterator);
      m_slots = nullptr;
    }

  private:
    fastuidraw::reference_counted_ptr<Slots> m_slots;
    typename Slots::iterator m_iterator;
  };

  /*!
    Call all the associated slots.
   */
  template<typename... Args>
  void
  operator()(Args... args)
  {
    for(auto& slot : *m_slots)
      {
        slot(args...);
      }
  }

  /*!
    Connect the slot and return the connection.
   */
  Connection
  connect(SlotType&& slot)
  {
    typename Slots::iterator iterator;
    iterator = m_slots->insert(m_slots->end(), slot);
    return Connection { m_slots, iterator };
  }

private:
  fastuidraw::reference_counted_ptr<Slots> m_slots;
};

}
