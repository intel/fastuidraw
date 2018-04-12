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

#ifndef fastuidraw_glu_tess_h_
#define fastuidraw_glu_tess_h_

#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <setjmp.h>
#include "gluos.hpp"
#include "glu-tess.hpp"
#include "mesh.hpp"
#include "dict.hpp"
#include "priorityq.hpp"


/* The begin/end calls must be properly nested.  We keep track of
 * the current state to enforce the ordering.
 */
enum TessState { T_DORMANT, T_IN_POLYGON, T_IN_CONTOUR };

/* We cache vertex data for single-contour polygons so that we can
 * try a quick-and-dirty decomposition first.
 */
#define TESS_MAX_CACHE  100

typedef struct CachedVertex {
  double        s, t;
  unsigned int  client_id;
  int real_edge;
} CachedVertex;

class fastuidraw_GLUtesselator {
public:
  /*** state needed for collecting the input data ***/

  enum TessState state;         /* what begin/end calls have we seen? */

  GLUhalfEdge   *lastEdge;      /* lastEdge->Org is the most recent vertex */
  GLUmesh       *mesh;          /* stores the input contours, and eventually
                                   the tessellation itself */

  void          (REGALFASTUIDRAW_GLU_CALL *callError)( FASTUIDRAW_GLUenum errnum );


  /*** state needed for the line sweep ***/

  double        relTolerance;   /* tolerance for merging features */
  //FASTUIDRAW_GLUenum       windingRule;    /* rule for determining polygon interior */
  FASTUIDRAW_GLUboolean      fatalError;     /* fatal error: needed combine callback */

  Dict          *dict;          /* edge dictionary for sweep line */
  PriorityQ     *pq;            /* priority queue of vertex events */
  GLUvertex     *event;         /* current sweep event being processed */

  void          (REGALFASTUIDRAW_GLU_CALL *callCombine)( double x, double y, unsigned int data[4],
                                                        double weight[4], unsigned int *outData );

  /*** state needed for rendering callbacks (see render.c) ***/

  FASTUIDRAW_GLUboolean      boundaryOnly;   /* Extract contours, not triangles */
  GLUface       *lonelyTriList;
    /* list of triangles which could not be rendered as strips or fans */

  void          (REGALFASTUIDRAW_GLU_CALL *callBegin)( FASTUIDRAW_GLUenum type, int winding_number );
  void          (REGALFASTUIDRAW_GLU_CALL *callVertex)( unsigned int data );
  void          (REGALFASTUIDRAW_GLU_CALL *callEnd)( void );
  void          (REGALFASTUIDRAW_GLU_CALL *callMesh)( GLUmesh *mesh );

  FASTUIDRAW_GLUboolean              (REGALFASTUIDRAW_GLU_CALL *callWinding)(int winding_number);
  void                               (REGALFASTUIDRAW_GLU_CALL *emit_monotone)(int winding,
                                                                               const unsigned int vertex_ids[],
                                                                               const int winding_ids[],
                                                                               unsigned int count);

  /*** state needed to cache single-contour polygons for renderCache() */

  FASTUIDRAW_GLUboolean      emptyCache;             /* empty cache on next vertex() call */
  int           cacheCount;             /* number of cached vertices */
  CachedVertex  cache[TESS_MAX_CACHE];  /* the vertex data */

  /*** rendering callbacks that also pass polygon data  ***/
  void          (REGALFASTUIDRAW_GLU_CALL *callBeginData)( FASTUIDRAW_GLUenum type, int winding_number, void *polygonData);
  void          (REGALFASTUIDRAW_GLU_CALL *callVertexData)( unsigned int data, void *polygonData );
  void          (REGALFASTUIDRAW_GLU_CALL *callEndData)( void *polygonData );
  void          (REGALFASTUIDRAW_GLU_CALL *callErrorData)( FASTUIDRAW_GLUenum errnum, void *polygonData );
  void          (REGALFASTUIDRAW_GLU_CALL *callCombineData)( double x, double y, unsigned int data[4],
                                                            double weight[4], unsigned int *outData,
                                                            void *polygonData );

  FASTUIDRAW_GLUboolean    (REGALFASTUIDRAW_GLU_CALL *callWindingData)(int winding_number,
                                                                       void *polygonData );
  void                     (REGALFASTUIDRAW_GLU_CALL *emit_monotone_data)(int winding,
                                                                          const unsigned int vertex_ids[],
                                                                          const int winding_ids[],
                                                                          unsigned int count,
                                                                          void *polygonData);

  jmp_buf env;                  /* place to jump to when memAllocs fail */

  void *polygonData;            /* client data for current polygon */

  /*for tracking tessellation creation, only active in debug
   */
  void *fastuidraw_alloc_tracker;

  /*value is 1 if current contour getting added affects winding, 0 if it should not
   */
  int edges_real;
};

void REGALFASTUIDRAW_GLU_CALL glu_fastuidraw_gl_noBeginData( FASTUIDRAW_GLUenum type, int winding_number, void *polygonData );
void REGALFASTUIDRAW_GLU_CALL glu_fastuidraw_gl_noVertexData( unsigned int data, void *polygonData );
void REGALFASTUIDRAW_GLU_CALL glu_fastuidraw_gl_noEndData( void *polygonData );
void REGALFASTUIDRAW_GLU_CALL glu_fastuidraw_gl_noErrorData( FASTUIDRAW_GLUenum errnum, void *polygonData );
void REGALFASTUIDRAW_GLU_CALL glu_fastuidraw_gl_noCombineData( double x, double y, unsigned int data[4],
                                                             double weight[4], unsigned int *outData,
                                                             void *polygonData );
FASTUIDRAW_GLUboolean REGALFASTUIDRAW_GLU_CALL glu_fastuidraw_gl_noWindingData(int winding_rule,
                                                                            void *polygonData);

#define CALL_BEGIN_OR_BEGIN_DATA(a,w)                    \
   if (tess->callBeginData != &glu_fastuidraw_gl_noBeginData) \
     (*tess->callBeginData)((a),(w), tess->polygonData); \
   else (*tess->callBegin)((a), (w));

#define CALL_VERTEX_OR_VERTEX_DATA(a) \
   if (tess->callVertexData != &glu_fastuidraw_gl_noVertexData) \
      (*tess->callVertexData)((a),tess->polygonData); \
   else (*tess->callVertex)((a));

#define CALL_END_OR_END_DATA() \
   if (tess->callEndData != &glu_fastuidraw_gl_noEndData) \
      (*tess->callEndData)(tess->polygonData); \
   else (*tess->callEnd)();

#define CALL_COMBINE_OR_COMBINE_DATA(a1,a2,b,c,d)                 \
  if (tess->callCombineData != &glu_fastuidraw_gl_noCombineData)    \
    (*tess->callCombineData)((a1),(a2),(b),(c),(d),tess->polygonData);   \
  else (*tess->callCombine)((a1),(a2),(b),(c),(d));

#define CALL_ERROR_OR_ERROR_DATA(a) \
   if (tess->callErrorData != &glu_fastuidraw_gl_noErrorData) \
      (*tess->callErrorData)((a),tess->polygonData); \
   else (*tess->callError)((a));


FASTUIDRAW_GLUboolean
call_tess_winding_or_winding_data_implement(fastuidraw_GLUtesselator *tess, int a);

#define CALL_TESS_WINDING_OR_WINDING_DATA(a) \
  call_tess_winding_or_winding_data_implement(tess, (a))

#endif
