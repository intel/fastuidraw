#pragma once

#include <list>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/painter/painter.hpp>

class PainterWidget:fastuidraw::noncopyable
{
public:

  explicit
  PainterWidget(PainterWidget *parent = nullptr);

  virtual
  ~PainterWidget(void);

  void
  paint(const fastuidraw::reference_counted_ptr<fastuidraw::Painter> &painter);

  PainterWidget*
  parent(void)
  {
    return m_parent;
  }

  void
  parent(PainterWidget *p);

  /* returns true if an ancestor of q
     is this
   */
  bool
  is_ancestor_of(PainterWidget *q);

  /* clip_in_rect for widget
   */
  fastuidraw::vec2 m_dimensions;

  /* transformation from local coordinate to parent coordinates
   */
  fastuidraw::float3x3 m_parent_matrix_this;

  /* if true, content is clipped to m_dimensions
   */
  bool m_clipped;

  /* if true, draw the PainterWidget as transparent
   */
  bool m_draw_transparent;

  /* only has effect if m_draw_transparent is true;
   * indicates to ignore the image alpha from the
   * transparency layer
   */
  bool m_ignore_alpha_if_transparent;

  /* if true, skip drawing both the widgets
     and alls its descendants
   */
  bool m_skip_drawing;
protected:

  virtual
  void
  pre_paint(void) {}

  virtual
  void
  paint_pre_children(const fastuidraw::reference_counted_ptr<fastuidraw::Painter> &painter);

  virtual
  void
  paint_post_children(const fastuidraw::reference_counted_ptr<fastuidraw::Painter> &painter);

private:
  PainterWidget *m_parent;
  std::list<PainterWidget*>::iterator m_iterator_loc;
  std::list<PainterWidget*> m_children;
};
