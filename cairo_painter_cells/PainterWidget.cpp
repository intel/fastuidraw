#include <assert.h>
#include "PainterWidget.hpp"

PainterWidget::
PainterWidget(PainterWidget *parent):
  m_dimensions(100.0, 100.0), //whatever.
  m_clipped(true),
  m_skip_drawing(false),
  m_parent(parent)
{
  cairo_matrix_init_identity(&m_parent_matrix_this);
  if(m_parent != NULL)
    {
      m_iterator_loc = m_parent->m_children.insert(m_parent->m_children.end(), this);
    }
}

PainterWidget::
~PainterWidget(void)
{
  if(m_parent != NULL)
    {
      m_parent->m_children.erase(m_iterator_loc);
    }

  for(std::list<PainterWidget*>::iterator iter = m_children.begin(),
        end = m_children.end(); iter != end; ++iter)
    {
      PainterWidget *c(*iter);

      assert(c->m_parent == this);
      c->m_parent = NULL;
      delete c;
    }
}


void
PainterWidget::
parent(PainterWidget *p)
{
  if(p == m_parent)
    {
      return;
    }

  if(m_parent != NULL)
    {
      m_parent->m_children.erase(m_iterator_loc);
    }

  assert(!is_ancestor_of(p));
  m_parent = p;
  m_iterator_loc = m_parent->m_children.insert(m_parent->m_children.end(), this);
}

bool
PainterWidget::
is_ancestor_of(PainterWidget *q)
{
  for(PainterWidget *p = q; p != NULL; p = p->parent())
    {
      if(this == p)
        return true;
    }
  return false;
}


void
PainterWidget::
paint(cairo_t *painter)
{
  pre_paint();
  if(m_skip_drawing)
    {
      return;
    }

  cairo_save(painter);
  cairo_transform(painter, &m_parent_matrix_this);

  if(m_clipped)
    {
      cairo_new_path(painter);
      cairo_rectangle(painter, 0.0, 0.0, m_dimensions.m_x, m_dimensions.m_y);
      cairo_clip(painter);
    }

  cairo_save(painter);
  paint_pre_children(painter);
  cairo_restore(painter);

  for(std::list<PainterWidget*>::iterator iter = m_children.begin(),
        end = m_children.end(); iter!=end; ++iter)
    {
      PainterWidget *c(*iter);
      assert(c->m_parent == this);
      c->paint(painter);
    }

  cairo_save(painter);
  paint_post_children(painter);
  cairo_restore(painter);

  cairo_restore(painter);
}

void
PainterWidget::
paint_pre_children(cairo_t *)
{}

void
PainterWidget::
paint_post_children(cairo_t *)
{}
