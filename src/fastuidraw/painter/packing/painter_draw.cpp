/*!
 * \file painter_draw.cpp
 * \brief file painter_draw.cpp
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


#include <vector>
#include <fastuidraw/painter/packing/painter_draw.hpp>

namespace
{

  enum map_status_t
    {
      status_mapped,
      status_waiting_for_actions_to_complete,
      status_unmapped,
    };

  class PainterDrawCommandPrivate
  {
  public:
    explicit
    PainterDrawCommandPrivate(fastuidraw::PainterDrawCommand *p):
      m_map_status(status_mapped),
      m_action_count(0),
      m_attribs_written(0),
      m_indices_written(0),
      m_data_store_written(0),
      m_p(p)
    {}

    enum map_status_t m_map_status;
    unsigned int m_action_count;
    std::vector<fastuidraw::PainterDrawCommand::DelayedAction::handle> m_actions;
    unsigned int m_attribs_written, m_indices_written, m_data_store_written;
    fastuidraw::PainterDrawCommand *m_p;
  };

  class DelayedActionPrivate
  {
  public:
    DelayedActionPrivate(void):
      m_cmd(NULL),
      m_slot(0)
    {}

    PainterDrawCommandPrivate *m_cmd;
    unsigned int m_slot;
  };
}

/////////////////////////////////////////
// fastuidraw::PainterDrawCommand::DelayedAction methods
fastuidraw::PainterDrawCommand::DelayedAction::
DelayedAction(void)
{
  m_d = FASTUIDRAWnew DelayedActionPrivate();
}

fastuidraw::PainterDrawCommand::DelayedAction::
~DelayedAction(void)
{
  DelayedActionPrivate *d;
  d = reinterpret_cast<DelayedActionPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

void
fastuidraw::PainterDrawCommand::DelayedAction::
perform_action(void)
{
  DelayedActionPrivate *d;
  d = reinterpret_cast<DelayedActionPrivate*>(m_d);

  assert(d->m_cmd != NULL);
  assert(d->m_slot < d->m_cmd->m_actions.size());
  assert(d->m_cmd->m_actions[d->m_slot] == this);

  action(d->m_cmd->m_p);

  d->m_cmd->m_actions[d->m_slot] = handle();
  d->m_cmd->m_action_count--;
  if(d->m_cmd->m_action_count == 0 && d->m_cmd->m_map_status == status_waiting_for_actions_to_complete)
    {
      d->m_cmd->m_p->complete_unmapping();
    }

  d->m_cmd = NULL;
  d->m_slot = 0;
}


/////////////////////////////////////////
// fastuidraw::PainterDrawCommand methods
fastuidraw::PainterDrawCommand::
PainterDrawCommand(void)
{
  m_d = FASTUIDRAWnew PainterDrawCommandPrivate(this);
}

fastuidraw::PainterDrawCommand::
~PainterDrawCommand(void)
{
  PainterDrawCommandPrivate *d;
  d = reinterpret_cast<PainterDrawCommandPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

void
fastuidraw::PainterDrawCommand::
add_action(const DelayedAction::handle &h) const
{
  PainterDrawCommandPrivate *d;
  DelayedActionPrivate *hd;

  assert(h);

  d = reinterpret_cast<PainterDrawCommandPrivate*>(m_d);
  hd = reinterpret_cast<DelayedActionPrivate*>(h->m_d);

  assert(hd->m_cmd == NULL);
  assert(d->m_map_status != status_unmapped);

  hd->m_cmd = d;
  hd->m_slot = d->m_actions.size();
  ++d->m_action_count;
  d->m_actions.push_back(h);
}

void
fastuidraw::PainterDrawCommand::
unmap(unsigned int attributes_written,
      unsigned int indices_written,
      unsigned int data_store_written) const
{
  PainterDrawCommandPrivate *d;
  d = reinterpret_cast<PainterDrawCommandPrivate*>(m_d);

  assert(d->m_map_status == status_mapped);

  d->m_attribs_written = attributes_written;
  d->m_indices_written = indices_written;
  d->m_data_store_written = data_store_written;
  d->m_map_status = status_waiting_for_actions_to_complete;
  if(d->m_action_count == 0)
    {
      complete_unmapping();
    }
}

void
fastuidraw::PainterDrawCommand::
complete_unmapping(void) const
{
  PainterDrawCommandPrivate *d;
  d = reinterpret_cast<PainterDrawCommandPrivate*>(m_d);

  assert(d->m_map_status == status_waiting_for_actions_to_complete);
  assert(d->m_action_count == 0);
  unmap_implement(d->m_attribs_written, d->m_indices_written, d->m_data_store_written);
  d->m_map_status = status_unmapped;
}

bool
fastuidraw::PainterDrawCommand::
unmapped(void) const
{
  PainterDrawCommandPrivate *d;
  d = reinterpret_cast<PainterDrawCommandPrivate*>(m_d);
  return d->m_map_status == status_unmapped;
}
