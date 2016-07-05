#include "PainterWidget.hpp"

PainterWidget::
PainterWidget(PainterWidget *parent):
  m_dimensions(SkSize::Make(100, 100)), //whatever.
  m_clipped(true),
  m_skip_drawing(false),
  m_parent(parent)
{
  m_parent_matrix_this.reset();
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
paint(SkCanvas *painter)
{
  pre_paint();
  if(m_skip_drawing)
    {
      return;
    }

  painter->save();
  painter->concat(m_parent_matrix_this);

  if(m_clipped)
    {
      painter->clipRect(SkRect::MakeSize(m_dimensions), SkRegion::kIntersect_Op);
    }

  painter->save();
  paint_pre_children(painter);
  painter->restore();

  for(std::list<PainterWidget*>::iterator iter = m_children.begin(),
        end = m_children.end(); iter!=end; ++iter)
    {
      PainterWidget *c(*iter);
      assert(c->m_parent == this);
      c->paint(painter);
    }

  painter->save();
  paint_post_children(painter);
  painter->restore();

  painter->restore();
}

void
PainterWidget::
paint_pre_children(SkCanvas *painter)
{}

void
PainterWidget::
paint_post_children(SkCanvas *painter)
{}
