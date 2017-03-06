/*
 * SGI FREE SOFTWARE LICENSE B (Version 2.0, Sept. 18, 2008)
 * Copyright (C) 1991-2000 Silicon Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice including the dates of first publication and
 * either this permission notice or a reference to
 * http://oss.sgi.com/projects/FreeB/
 * shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * SILICON GRAPHICS, INC. BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of Silicon Graphics, Inc.
 * shall not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization from
 * Silicon Graphics, Inc.
 */
/*
** Author: Eric Veach, July 1994.
**
*/
#include <iostream>

#include "gluos.hpp"
#include <assert.h>
#include <stddef.h>
#include "mesh.hpp"
#include "tess.hpp"
#include "render.hpp"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif


static void RenderTriangles( fastuidraw_GLUtesselator *tess, GLUface *head );



/************************ Strips and Fans decomposition ******************/

/* glu_fastuidraw_gl_renderMesh( tess, mesh ) takes a mesh and breaks it into triangle
 * fans, strips, and separate triangles.  A substantial effort is made
 * to use as few rendering primitives as possible (ie. to make the fans
 * and strips as large as possible).
 *
 * The rendering output is provided as callbacks (see the api).
 */
void glu_fastuidraw_gl_renderMesh( fastuidraw_GLUtesselator *tess, GLUmesh *mesh )
{
  /* Make a list of separate triangles so we can render them all at once */
  tess->lonelyTriList = NULL;
  RenderTriangles(tess, mesh->fHead.next);
}

static void RenderTriangles( fastuidraw_GLUtesselator *tess, GLUface *f )
{
  /* Now we render all the separate triangles which could not be
   * grouped into a triangle fan or strip.
   */
  GLUhalfEdge *e;
  int prevWindingNumber;
  int justStarted = 1, prevWindingReady = 0;
  GLUface *startFace;

  startFace = f;

  for( ; f != startFace || justStarted; f = f->next ) {
    if(f->winding_number != 0 && CALL_TESS_WINDING_OR_WINDING_DATA(f->winding_number))
      {
        assert(f->inside);
        e = f->anEdge;

        /* Loop once for each edge (there will always be 3 edges) */
        if( !prevWindingReady || prevWindingNumber != f->winding_number ) {
          prevWindingNumber = f->winding_number;
          prevWindingReady = 1;
          CALL_BEGIN_OR_BEGIN_DATA( FASTUIDRAW_GLU_TRIANGLES, f->winding_number );
        }

        do {
          CALL_VERTEX_OR_VERTEX_DATA( e->Org->client_id );
          e = e->Lnext;
        } while(e != f->anEdge );
      }
    justStarted = 0;
  }
  CALL_END_OR_END_DATA();
}




/************************ Boundary contour decomposition ******************/

/* glu_fastuidraw_gl_renderBoundary( tess, mesh ) takes a mesh, and outputs one
 * contour for each face marked "inside".  The rendering output is
 * provided as callbacks (see the api).
 */
void glu_fastuidraw_gl_renderBoundary( fastuidraw_GLUtesselator *tess, GLUmesh *mesh )
{
  GLUface *f;
  GLUhalfEdge *e;

  for( f = mesh->fHead.next; f != &mesh->fHead; f = f->next ) {
    if( f->inside ) {
      CALL_BEGIN_OR_BEGIN_DATA( FASTUIDRAW_GLU_LINE_LOOP, f->winding_number );
      e = f->anEdge;
      do {
        CALL_VERTEX_OR_VERTEX_DATA( e->Org->client_id );
        e = e->Lnext;
      } while( e != f->anEdge );
      CALL_END_OR_END_DATA();
    }
  }
}


/************************ Quick-and-dirty decomposition ******************/

#define SIGN_INCONSISTENT 2

static int CheckNormal( fastuidraw_GLUtesselator *tess)
/*
 * If check==FALSE, we compute the polygon normal and place it in norm[].
 * If check==TRUE, we check that each triangle in the fan from v0 has a
 * consistent orientation with respect to norm[].  If triangles are
 * consistently oriented CCW, return 1; if CW, return -1; if all triangles
 * are degenerate return 0; otherwise (no consistent orientation) return
 * SIGN_INCONSISTENT.
 */
{
  CachedVertex *v0 = tess->cache;
  CachedVertex *vn = v0 + tess->cacheCount;
  CachedVertex *vc;
  double dot, xc, yc, xp, yp;
  int sign = 0;

  /* Find the polygon normal.  It is important to get a reasonable
   * normal even when the polygon is self-intersecting (eg. a bowtie).
   * Otherwise, the computed normal could be very tiny, but perpendicular
   * to the true plane of the polygon due to numerical noise.  Then all
   * the triangles would appear to be degenerate and we would incorrectly
   * decompose the polygon as a fan (or simply not render it at all).
   *
   * We use a sum-of-triangles normal algorithm rather than the more
   * efficient sum-of-trapezoids method (used in CheckOrientation()
   * in normal.c).  This lets us explicitly reverse the signed area
   * of some triangles to get a reasonable normal in the self-intersecting
   * case.
   */

  vc = v0 + 1;
  xc = vc->s - v0->s;
  yc = vc->t - v0->t;
  while( ++vc < vn ) {
    xp = xc; yp = yc;
    xc = vc->s - v0->s;
    yc = vc->t - v0->t;

    /* Compute (vp - v0) cross (vc - v0) */
    dot = xp*yc - yp*xc;
    if( dot != 0 ) {
      /* Check the new orientation for consistency with previous triangles */
      if( dot > 0 ) {
        if( sign < 0 ) return SIGN_INCONSISTENT;
        sign = 1;
      } else {
        if( sign > 0 ) return SIGN_INCONSISTENT;
        sign = -1;
      }
    }
  }
  return sign;
}

/* glu_fastuidraw_gl_renderCache( tess ) takes a single contour and tries to render it
 * as a triangle fan.  This handles convex polygons, as well as some
 * non-convex polygons if we get lucky.
 *
 * Returns TRUE if the polygon was successfully rendered.  The rendering
 * output is provided as callbacks (see the api).
 */
FASTUIDRAW_GLUboolean glu_fastuidraw_gl_renderCache( fastuidraw_GLUtesselator *tess )
{
  CachedVertex *v0 = tess->cache;
  CachedVertex *vn = v0 + tess->cacheCount;
  CachedVertex *vc, *vp;
  int sign;

  if( tess->cacheCount < 3 ) {
    /* Degenerate contour -- no output */
    return TRUE;
  }



  sign = CheckNormal( tess );
  if( sign == SIGN_INCONSISTENT ) {
    /* Fan triangles did not have a consistent orientation */
    return FALSE;
  }
  if( sign == 0 ) {
    /* All triangles were degenerate */
    return TRUE;
  }

  /* Make sure we do the right thing for each winding rule */
  {
    FASTUIDRAW_GLUboolean in_region;

    in_region=CALL_TESS_WINDING_OR_WINDING_DATA(sign);
    if(!in_region)
      return TRUE;
  }

  if(tess->boundaryOnly)
    {
      CALL_BEGIN_OR_BEGIN_DATA(FASTUIDRAW_GLU_LINE_LOOP, sign);
      CALL_VERTEX_OR_VERTEX_DATA( v0->client_id );
      if( sign > 0 ) {
        for( vc = v0+1; vc < vn; ++vc ) {
          CALL_VERTEX_OR_VERTEX_DATA( vc->client_id );
        }
      } else {
        for( vc = vn-1; vc > v0; --vc ) {
          CALL_VERTEX_OR_VERTEX_DATA( vc->client_id );
        }
      }
    }
  else
    {
      CALL_BEGIN_OR_BEGIN_DATA(FASTUIDRAW_GLU_TRIANGLES, sign);
      if( sign > 0 ) {
        for( vc = v0+2, vp = v0+1; vc < vn; ++vc, ++vp) {
          CALL_VERTEX_OR_VERTEX_DATA( v0->client_id );
          CALL_VERTEX_OR_VERTEX_DATA( vp->client_id );
          CALL_VERTEX_OR_VERTEX_DATA( vc->client_id );
        }
      } else {
        for( vc = v0+2, vp = v0+1; vc < vn; ++vc, ++vp ) {
          CALL_VERTEX_OR_VERTEX_DATA( v0->client_id );
          CALL_VERTEX_OR_VERTEX_DATA( vc->client_id );
          CALL_VERTEX_OR_VERTEX_DATA( vp->client_id );
        }
      }
    }
  CALL_END_OR_END_DATA();
  return TRUE;
}
