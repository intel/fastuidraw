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
#include "gluos.hpp"
#include <iostream>
#include <stddef.h>
#include <fastuidraw/util/util.hpp>
#include <vector>
#include <set>
#include <setjmp.h>
#include "memalloc.hpp"
#include "tess.hpp"
#include "mesh.hpp"
#include "sweep.hpp"
#include "tessmono.hpp"
#include "render.hpp"

#define FASTUIDRAW_GLU_TESS_DEFAULT_TOLERANCE 0.0
#define FASTUIDRAW_GLU_TESS_MESH             100112  /* void (*)(GLUmesh *mesh)          */

#define IGNORE(X) do { (void)(X); } while (0)


#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif


/*ARGSUSED*/ static void REGALFASTUIDRAW_GLU_CALL noBegin( FASTUIDRAW_GLUenum type, int winding_number )
{
  IGNORE(type);
  IGNORE(winding_number);
}


/*ARGSUSED*/ static void REGALFASTUIDRAW_GLU_CALL noVertex( unsigned int data )
{
  IGNORE(data);
}


/*ARGSUSED*/ static void REGALFASTUIDRAW_GLU_CALL noEnd( void ) {}

/*ARGSUSED*/ static void REGALFASTUIDRAW_GLU_CALL noError( FASTUIDRAW_GLUenum errnum )
{
  IGNORE(errnum);
}

/*ARGSUSED*/ static void REGALFASTUIDRAW_GLU_CALL noCombine( double x, double y, unsigned int data[4],
                                                            double weight[4], unsigned int *dataOut )
{
  IGNORE(x);
  IGNORE(y);
  IGNORE(data);
  IGNORE(weight);
  *dataOut = 0;
}

/*ARGSUSED*/ static void REGALFASTUIDRAW_GLU_CALL noMesh( GLUmesh *mesh )
{
  IGNORE(mesh);
}


/*ARGSUSED*/ static FASTUIDRAW_GLUboolean REGALFASTUIDRAW_GLU_CALL noWinding(int winding_rule)
{
  return winding_rule != 0;
}

/*ARGSUSED*/ void REGALFASTUIDRAW_GLU_CALL glu_fastuidraw_gl_noBeginData( FASTUIDRAW_GLUenum type, int winding_number,
                                                               void *polygonData )
{
  IGNORE(polygonData);
  IGNORE(type);
  IGNORE(winding_number);
}

/*ARGSUSED*/ void REGALFASTUIDRAW_GLU_CALL glu_fastuidraw_gl_noVertexData( unsigned int data,
                                              void *polygonData )
{
  IGNORE(polygonData);
  IGNORE(data);
}

/*ARGSUSED*/ void REGALFASTUIDRAW_GLU_CALL glu_fastuidraw_gl_noEndData( void *polygonData )
{
  IGNORE(polygonData);
}

/*ARGSUSED*/ void REGALFASTUIDRAW_GLU_CALL glu_fastuidraw_gl_noErrorData( FASTUIDRAW_GLUenum errnum,
                                             void *polygonData )
{
  IGNORE(polygonData);
  IGNORE(errnum);
}

/*ARGSUSED*/ void REGALFASTUIDRAW_GLU_CALL glu_fastuidraw_gl_noCombineData( double x, double y,
                                                                          unsigned int data[4],
                                                                          double weight[4],
                                                                          unsigned int *outData,
                                                                          void *polygonData )
{
  IGNORE(polygonData);
  IGNORE(x);
  IGNORE(y);
  IGNORE(data);
  IGNORE(weight);
  *outData = 0;
}

/*ARGSUSED*/ static FASTUIDRAW_GLUboolean REGALFASTUIDRAW_GLU_CALL noWindingData(int winding_rule,
                                                                                 void *polygonData)
{
  IGNORE(polygonData);
  return winding_rule&1;
}

/* Half-edges are allocated in pairs (see mesh.c) */
typedef struct { GLUhalfEdge e, eSym; } EdgePair;

#undef  MAX
#define MAX(a,b)        ((a) > (b) ? (a) : (b))
#define MAX_FAST_ALLOC  (MAX(sizeof(EdgePair), \
                         MAX(sizeof(GLUvertex),sizeof(GLUface))))

fastuidraw_GLUtesselator * REGALFASTUIDRAW_GLU_CALL
fastuidraw_gluNewTess_debug(const char *file, int line)
{
  fastuidraw_GLUtesselator *R;
  R = fastuidraw_gluNewTess_release();
  R->fastuidraw_alloc_tracker = fastuidraw::memory::malloc_implement(4, file, line);
  return R;
}

fastuidraw_GLUtesselator * REGALFASTUIDRAW_GLU_CALL
fastuidraw_gluNewTess_release( void )
{
  fastuidraw_GLUtesselator *tess;

  /* Only initialize fields which can be changed by the api.  Other fields
   * are initialized where they are used.
   */

  if (memInit( MAX_FAST_ALLOC ) == 0) {
     return 0;                  /* out of memory */
  }
  tess = (fastuidraw_GLUtesselator *)memAlloc( sizeof( fastuidraw_GLUtesselator ));
  if (tess == nullptr) {
     return 0;                  /* out of memory */
  }


  tess->state = T_DORMANT;

  tess->relTolerance = FASTUIDRAW_GLU_TESS_DEFAULT_TOLERANCE;

  tess->callBegin = &noBegin;
  tess->callVertex = &noVertex;
  tess->callEnd = &noEnd;

  tess->callError = &noError;
  tess->callCombine = &noCombine;
  tess->callMesh = &noMesh;

  tess->callWinding= &noWinding;
  tess->emit_monotone = nullptr;
  tess->boundary_corner_point = nullptr;
  tess->emit_boundary = nullptr;

  tess->callBeginData= &glu_fastuidraw_gl_noBeginData;
  tess->callVertexData= &glu_fastuidraw_gl_noVertexData;
  tess->callEndData= &glu_fastuidraw_gl_noEndData;
  tess->callErrorData= &glu_fastuidraw_gl_noErrorData;
  tess->callCombineData= &glu_fastuidraw_gl_noCombineData;

  tess->callWindingData= &noWindingData;
  tess->emit_monotone_data = nullptr;
  tess->boundary_corner_point_data = nullptr;
  tess->emit_boundary_data = nullptr;

  tess->polygonData= nullptr;

  tess->fastuidraw_alloc_tracker = nullptr;

  return tess;
}

static void MakeDormant( fastuidraw_GLUtesselator *tess )
{
  /* Return the tessellator to its original dormant state. */

  if( tess->mesh != nullptr ) {
    glu_fastuidraw_gl_meshDeleteMesh( tess->mesh );
  }
  tess->state = T_DORMANT;
  tess->lastEdge = nullptr;
  tess->mesh = nullptr;
}

#define RequireState( tess, s )   if( tess->state != s ) GotoState(tess,s)

static void GotoState( fastuidraw_GLUtesselator *tess, enum TessState newState )
{
  while( tess->state != newState ) {
    /* We change the current state one level at a time, to get to
     * the desired state.
     */
    if( tess->state < newState ) {
      switch( tess->state ) {
      case T_DORMANT:
        CALL_ERROR_OR_ERROR_DATA( FASTUIDRAW_GLU_TESS_MISSING_BEGIN_POLYGON );
        fastuidraw_gluTessBeginPolygon( tess, nullptr );
        break;
      case T_IN_POLYGON:
        CALL_ERROR_OR_ERROR_DATA( FASTUIDRAW_GLU_TESS_MISSING_BEGIN_CONTOUR );
        fastuidraw_gluTessBeginContour( tess, FASTUIDRAW_GLU_TRUE );
        break;
      default:
         ;
      }
    } else {
      switch( tess->state ) {
      case T_IN_CONTOUR:
        CALL_ERROR_OR_ERROR_DATA( FASTUIDRAW_GLU_TESS_MISSING_END_CONTOUR );
        fastuidraw_gluTessEndContour( tess );
        break;
      case T_IN_POLYGON:
        CALL_ERROR_OR_ERROR_DATA( FASTUIDRAW_GLU_TESS_MISSING_END_POLYGON );
        /* fastuidraw_gluTessEndPolygon( tess ) is too much work! */
        MakeDormant( tess );
        break;
      default:
         ;
      }
    }
  }
}

void REGALFASTUIDRAW_GLU_CALL
fastuidraw_gluDeleteTess_debug(fastuidraw_GLUtesselator* tess, const char *file, int line)
{
  fastuidraw::memory::free_implement(tess->fastuidraw_alloc_tracker, file, line);
  fastuidraw_gluDeleteTess_release(tess);
}


void REGALFASTUIDRAW_GLU_CALL
fastuidraw_gluDeleteTess_release( fastuidraw_GLUtesselator *tess )
{
  RequireState( tess, T_DORMANT );
  memFree( tess );
}

void REGALFASTUIDRAW_GLU_CALL
fastuidraw_gluTessPropertyTolerance(fastuidraw_GLUtesselator *tess, double value)
{
  if( value < 0.0 || value > 1.0 ) return;

  tess->relTolerance = value;
}

double REGALFASTUIDRAW_GLU_CALL
fastuidraw_gluGetTessPropertyTolerance(fastuidraw_GLUtesselator *tess)
{
  return tess->relTolerance;
}

FASTUIDRAW_GLUboolean
call_tess_winding_or_winding_data_implement(fastuidraw_GLUtesselator *tess, int a)
{
  if (tess->callWindingData != &noWindingData)
    return (*tess->callWindingData)(a, tess->polygonData);
   else
     return (*tess->callWinding)(a);
}

#define FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(which, type, label) \
  void fastuidraw_gluTessCallback##label(fastuidraw_GLUtesselator *tess, type f) { fastuidraw_gluTessCallback(tess, which, (FASTUIDRAW_GLUfuncptr)(f)); }

FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_BEGIN, fastuidraw_glu_tess_function_begin, Begin)
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_VERTEX, fastuidraw_glu_tess_function_vertex, Vertex)
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_END, fastuidraw_glu_tess_function_end, End)
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_ERROR, fastuidraw_glu_tess_function_error, Error)
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_COMBINE, fastuidraw_glu_tess_function_combine, Combine)
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_WINDING_CALLBACK, fastuidraw_glu_tess_function_winding, FillRule)
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_EMIT_MONOTONE, fastuidraw_glu_tess_function_emit_monotone, EmitMonotone)
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_BOUNDARY_CORNER, fastuidraw_glu_tess_function_boundary_corner_point, BoundaryCornerPoint)
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_EMIT_BOUNDARY, fastuidraw_glu_tess_function_emit_boundary, EmitBoundary);

FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_BEGIN_DATA, fastuidraw_glu_tess_function_begin_data, Begin)
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_VERTEX_DATA, fastuidraw_glu_tess_function_vertex_data, Vertex)
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_END_DATA, fastuidraw_glu_tess_function_end_data, End)
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_ERROR_DATA, fastuidraw_glu_tess_function_error_data, Error)
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_COMBINE_DATA, fastuidraw_glu_tess_function_combine_data, Combine)
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_WINDING_CALLBACK_DATA, fastuidraw_glu_tess_function_winding_data, FillRule)
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_EMIT_MONOTONE_DATA, fastuidraw_glu_tess_function_emit_monotone_data, EmitMonotone)
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_BOUNDARY_CORNER_DATA, fastuidraw_glu_tess_function_boundary_corner_point_data, BoundaryCornerPoint)
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK_IMPLEMENT(FASTUIDRAW_GLU_TESS_EMIT_BOUNDARY_DATA, fastuidraw_glu_tess_function_emit_boundary_data, EmitBoundary);


void REGALFASTUIDRAW_GLU_CALL
fastuidraw_gluTessCallback( fastuidraw_GLUtesselator *tess, FASTUIDRAW_GLUenum which, FASTUIDRAW_GLUfuncptr fn)
{
  switch( which ) {
  case FASTUIDRAW_GLU_TESS_BEGIN:
    tess->callBegin = (fn == nullptr) ? &noBegin : (void (REGALFASTUIDRAW_GLU_CALL *)(FASTUIDRAW_GLUenum, int)) fn;
    return;
  case FASTUIDRAW_GLU_TESS_BEGIN_DATA:
    tess->callBeginData = (fn == nullptr) ?
        &glu_fastuidraw_gl_noBeginData : (void (REGALFASTUIDRAW_GLU_CALL *)(FASTUIDRAW_GLUenum, int, void *)) fn;
    return;
  case FASTUIDRAW_GLU_TESS_VERTEX:
    tess->callVertex = (fn == nullptr) ? &noVertex :
                                      (void (REGALFASTUIDRAW_GLU_CALL *)(unsigned int)) fn;
    return;
  case FASTUIDRAW_GLU_TESS_VERTEX_DATA:
    tess->callVertexData = (fn == nullptr) ?
        &glu_fastuidraw_gl_noVertexData : (void (REGALFASTUIDRAW_GLU_CALL *)(unsigned int, void *)) fn;
    return;
  case FASTUIDRAW_GLU_TESS_END:
    tess->callEnd = (fn == nullptr) ? &noEnd : (void (REGALFASTUIDRAW_GLU_CALL *)(void)) fn;
    return;
  case FASTUIDRAW_GLU_TESS_END_DATA:
    tess->callEndData = (fn == nullptr) ? &glu_fastuidraw_gl_noEndData :
                                       (void (REGALFASTUIDRAW_GLU_CALL *)(void *)) fn;
    return;
  case FASTUIDRAW_GLU_TESS_ERROR:
    tess->callError = (fn == nullptr) ? &noError : (void (REGALFASTUIDRAW_GLU_CALL *)(FASTUIDRAW_GLUenum)) fn;
    return;
  case FASTUIDRAW_GLU_TESS_ERROR_DATA:
    tess->callErrorData = (fn == nullptr) ?
        &glu_fastuidraw_gl_noErrorData : (void (REGALFASTUIDRAW_GLU_CALL *)(FASTUIDRAW_GLUenum, void *)) fn;
    return;
  case FASTUIDRAW_GLU_TESS_COMBINE:
    tess->callCombine = (fn == nullptr) ? &noCombine :
    (void (REGALFASTUIDRAW_GLU_CALL *)(double , double, unsigned int[4], double [4], unsigned int* )) fn;
    return;
  case FASTUIDRAW_GLU_TESS_COMBINE_DATA:
    tess->callCombineData = (fn == nullptr) ? &glu_fastuidraw_gl_noCombineData :
                                           (void (REGALFASTUIDRAW_GLU_CALL *)(double,
                                                                             double,
                                                                             unsigned int[4],
                                                                             double [4],
                                                                             unsigned int*,
                                                                             void *)) fn;
    return;
  case FASTUIDRAW_GLU_TESS_MESH:
    tess->callMesh = (fn == nullptr) ? &noMesh : (void (REGALFASTUIDRAW_GLU_CALL *)(GLUmesh *)) fn;
    return;

  case FASTUIDRAW_GLU_TESS_WINDING_CALLBACK:
    tess->callWinding=(fn==nullptr)? &noWinding: (FASTUIDRAW_GLUboolean (REGALFASTUIDRAW_GLU_CALL *)(int)) fn;
    return;

  case FASTUIDRAW_GLU_TESS_WINDING_CALLBACK_DATA:
    tess->callWindingData=(fn==nullptr)? &noWindingData: (FASTUIDRAW_GLUboolean (REGALFASTUIDRAW_GLU_CALL *)(int, void*)) fn;
    return;

  case FASTUIDRAW_GLU_TESS_EMIT_MONOTONE:
    tess->emit_monotone = (fastuidraw_glu_tess_function_emit_monotone)fn;
    return;

  case FASTUIDRAW_GLU_TESS_EMIT_MONOTONE_DATA:
    tess->emit_monotone_data = (fastuidraw_glu_tess_function_emit_monotone_data)fn;
    return;

  case FASTUIDRAW_GLU_TESS_BOUNDARY_CORNER:
    tess->boundary_corner_point = (fastuidraw_glu_tess_function_boundary_corner_point)fn;
    return;

  case FASTUIDRAW_GLU_TESS_BOUNDARY_CORNER_DATA:
    tess->boundary_corner_point_data = (fastuidraw_glu_tess_function_boundary_corner_point_data)fn;
    return;

  case FASTUIDRAW_GLU_TESS_EMIT_BOUNDARY:
    tess->emit_boundary = (fastuidraw_glu_tess_function_emit_boundary)fn;
    break;

  case FASTUIDRAW_GLU_TESS_EMIT_BOUNDARY_DATA:
    tess->emit_boundary_data = (fastuidraw_glu_tess_function_emit_boundary_data)fn;
    break;

  default:
    CALL_ERROR_OR_ERROR_DATA( FASTUIDRAW_GLU_INVALID_ENUM );
    return;
  }
}

static int AddVertex( fastuidraw_GLUtesselator *tess, double x, double y, unsigned int data)
{
  GLUhalfEdge *e;

  e = tess->lastEdge;
  if( e == nullptr ) {
    /* Make a self-loop (one vertex, one edge). */

    e = glu_fastuidraw_gl_meshMakeEdge( tess->mesh );
    if (e == nullptr) return 0;
    if ( !glu_fastuidraw_gl_meshSplice( e, e->Sym ) ) return 0;
  } else {
    /* Create a new vertex and edge which immediately follow e
     * in the ordering around the left face.
     */
    if (glu_fastuidraw_gl_meshSplitEdge( e ) == nullptr) return 0;
    e = e->Lnext;
  }

  /* The new vertex is now e->Org. */
  e->Org->client_id = data;
  e->Org->s = x;
  e->Org->t = y;

  /* The winding of an edge says how the winding number changes as we
   * cross from the edge''s right face to its left face.  We add the
   * vertices in such an order that a CCW contour will add +1 to
   * the winding number of the region inside the contour.
   */
  e->winding = tess->edges_real;
  e->Sym->winding = -tess->edges_real;

  tess->lastEdge = e;

  return 1;
}

static void CacheVertex( fastuidraw_GLUtesselator *tess, double x, double y, unsigned int data )
{
  CachedVertex *v = &tess->cache[tess->cacheCount];

  v->client_id = data;
  v->s = x;
  v->t = y;
  v->real_edge = tess->edges_real;
  ++tess->cacheCount;
}


static int EmptyCache( fastuidraw_GLUtesselator *tess )
{
  CachedVertex *v = tess->cache;
  CachedVertex *vLast;
  int add_return_value, edges_real;

  tess->mesh = glu_fastuidraw_gl_meshNewMesh();
  if (tess->mesh == nullptr) return 0;

  edges_real = tess->edges_real;
  for( vLast = v + tess->cacheCount; v < vLast; ++v ) {

    tess->edges_real = v->real_edge;
    add_return_value = AddVertex( tess, v->s, v->t, v->client_id);
    tess->edges_real = edges_real;

    if ( !add_return_value ) return 0;
  }
  tess->cacheCount = 0;
  tess->emptyCache = FALSE;

  return 1;
}

static
void checkTooLarge(double *p, int *tooLarge)
{
  if( *p < -FASTUIDRAW_GLU_TESS_MAX_COORD ) {
    *p = -FASTUIDRAW_GLU_TESS_MAX_COORD;
    *tooLarge = TRUE;
  }
  if( *p > FASTUIDRAW_GLU_TESS_MAX_COORD ) {
    *p = FASTUIDRAW_GLU_TESS_MAX_COORD;
    *tooLarge = TRUE;
  }
}

void REGALFASTUIDRAW_GLU_CALL
fastuidraw_gluTessVertex( fastuidraw_GLUtesselator *tess, double x, double y, unsigned int data )
{
  int tooLarge = FALSE;

  FASTUIDRAWassert(data != FASTUIDRAW_GLU_nullptr_CLIENT_ID);
  RequireState( tess, T_IN_CONTOUR );

  if( tess->emptyCache ) {
    if ( !EmptyCache( tess ) ) {
       CALL_ERROR_OR_ERROR_DATA( FASTUIDRAW_GLU_OUT_OF_MEMORY );
       return;
    }
    tess->lastEdge = nullptr;
  }

  checkTooLarge(&x, &tooLarge);
  checkTooLarge(&y, &tooLarge);


  if( tooLarge ) {
    CALL_ERROR_OR_ERROR_DATA( FASTUIDRAW_GLU_TESS_COORD_TOO_LARGE );
  }

  if( tess->mesh == nullptr ) {
    if( tess->cacheCount < TESS_MAX_CACHE ) {
      CacheVertex( tess, x, y, data );
      return;
    }
    if ( !EmptyCache( tess ) ) {
       CALL_ERROR_OR_ERROR_DATA( FASTUIDRAW_GLU_OUT_OF_MEMORY );
       return;
    }
  }
  if ( !AddVertex( tess, x, y, data ) ) {
       CALL_ERROR_OR_ERROR_DATA( FASTUIDRAW_GLU_OUT_OF_MEMORY );
  }
}


void REGALFASTUIDRAW_GLU_CALL
fastuidraw_gluTessBeginPolygon( fastuidraw_GLUtesselator *tess, void *data )
{
  RequireState( tess, T_DORMANT );

  tess->state = T_IN_POLYGON;
  tess->cacheCount = 0;
  tess->emptyCache = FALSE;
  tess->mesh = nullptr;

  tess->polygonData= data;

  if (CALL_TESS_WINDING_OR_WINDING_DATA(0) == FASTUIDRAW_GLU_TRUE) {
    /* disable the cache if the winding 0 is to be picked up */
    tess->mesh = glu_fastuidraw_gl_meshNewMesh();
  }
}


void REGALFASTUIDRAW_GLU_CALL
fastuidraw_gluTessBeginContour( fastuidraw_GLUtesselator *tess, FASTUIDRAW_GLUboolean contour_real )
{
  RequireState( tess, T_IN_POLYGON );

  tess->state = T_IN_CONTOUR;
  tess->lastEdge = nullptr;
  if( tess->cacheCount > 0 ) {
    /* Just set a flag so we don't get confused by empty contours
     * -- these can be generated accidentally with the obsolete
     * NextContour() interface.
     */
    tess->emptyCache = TRUE;
  }

  if(contour_real) {
    tess->edges_real = 1;
  }
  else {
    tess->edges_real = 0;
  }
}


void REGALFASTUIDRAW_GLU_CALL
fastuidraw_gluTessEndContour( fastuidraw_GLUtesselator *tess )
{
  RequireState( tess, T_IN_CONTOUR );
  tess->state = T_IN_POLYGON;
}

static void
glu_fastuidraw_gl_emitBoundaryOfMesh(int winding, fastuidraw_GLUtesselator *tess, GLUmesh *mesh )
{
  std::vector<unsigned int> vertex_ids;
  GLUface *f, *next;
  GLUhalfEdge *e;

  for( f = mesh->fHead.next; f != &mesh->fHead; f = next ) {
    next = f->next;
    if( f->inside && !glu_fastuidraw_gl_excludeFace(f)) {
      vertex_ids.clear();
      e = f->anEdge;
      do {
        vertex_ids.push_back(e->Org->client_id );
        e = e->Lnext;
      } while( e != f->anEdge );

      if(tess->emit_boundary_data) {
        (*tess->emit_boundary_data)(winding, &vertex_ids[0], vertex_ids.size(), tess->polygonData);
      } else if (tess->emit_boundary) {
        (*tess->emit_boundary)(winding, &vertex_ids[0], vertex_ids.size());
      }
    }
  }
}

static void
glu_fastuidraw_gl_emitBoundaries( fastuidraw_GLUtesselator *tess, GLUmesh *mesh )
{
  /* compute the winding numbers */
  std::set<int> winding_numbers;
  GLUface *f;
  int just_started;

  for (f = mesh->fHead.next, just_started = 1; f != mesh->fHead.next || just_started; f = f->next) {
    winding_numbers.insert(f->winding_number);
    just_started = 0;
  }

  for(int w : winding_numbers) {
    GLUmesh *m;

    m = glu_fastuidraw_gl_copyMesh(mesh);
    glu_fastuidraw_gl_meshKeepOnly(m, w);
    glu_fastuidraw_gl_emitBoundaryOfMesh(w, tess, m);
    glu_fastuidraw_gl_meshDeleteMesh(m);
  }
}

void REGALFASTUIDRAW_GLU_CALL
fastuidraw_gluTessEndPolygon( fastuidraw_GLUtesselator *tess )
{
  GLUmesh *mesh;

  RequireState( tess, T_IN_POLYGON );
  if (CALL_TESS_WINDING_OR_WINDING_DATA(0) == TRUE && HAVE_BOUNDARY_CORNER_POINT) {
    /* if user-space requests the 0 region is filled, we add two
     * square contours one larger than the next, each larger than
     * the bounding box and each going in opposite directions.
     * The points on the outside contour get special marks so that
     * any region that has them, is not reported back the client.
     */

    FASTUIDRAW_GLUboolean boundary_flags[4][2] = {
      {FALSE, FALSE},
      {FALSE, TRUE},
      {TRUE, TRUE},
      {TRUE, FALSE},
    };

    fastuidraw_gluTessBeginContour(tess, TRUE);
    for (int i = 0; i < 4; ++i) {
      double x(0.0), y(0.0);
      unsigned int client_id(FASTUIDRAW_GLU_nullptr_CLIENT_ID);

      CALL_BOUNDARY_CORNER_POINT(&x, &y, 0, boundary_flags[i][0], boundary_flags[i][1], &client_id);
      AddVertex(tess, x, y, client_id);
    }
    fastuidraw_gluTessEndContour(tess);

    fastuidraw_gluTessBeginContour(tess, TRUE);
    for (int i = 3; i >= 0; --i) {
      double x(0.0), y(0.0);

      CALL_BOUNDARY_CORNER_POINT(&x, &y, 1, boundary_flags[i][0], boundary_flags[i][1], nullptr);
      AddVertex(tess, x, y, FASTUIDRAW_GLU_nullptr_CLIENT_ID);
    }
    fastuidraw_gluTessEndContour(tess);
  }

  if (setjmp(tess->env) != 0) {
     /* come back here if out of memory */
     CALL_ERROR_OR_ERROR_DATA( FASTUIDRAW_GLU_OUT_OF_MEMORY );
     return;
  }

  RequireState( tess, T_IN_POLYGON );
  tess->state = T_DORMANT;

  if( tess->mesh == nullptr ) {
    if( tess->callMesh == &noMesh && CALL_TESS_WINDING_OR_WINDING_DATA(0) == FALSE) {

      /* Try some special code to make the easy cases go quickly
       * (eg. convex polygons).  This code does NOT handle multiple contours,
       * intersections, edge flags, and of course it does not generate
       * an explicit mesh either.
       */
      if( glu_fastuidraw_gl_renderCache( tess )) {
        tess->polygonData= nullptr;
        return;
      }
    }
    if ( !EmptyCache( tess ) ) longjmp(tess->env,1); /* could've used a label*/
  }

  /* glu_fastuidraw_gl_computeInterior( tess ) computes the planar arrangement specified
   * by the given contours, and further subdivides this arrangement
   * into regions.  Each region is marked "inside" if it belongs
   * to the polygon, according to the rule given by tess->windingRule.
   * Each interior region is guaranteed be monotone.
   */
  if ( !glu_fastuidraw_gl_computeInterior( tess ) ) {
     longjmp(tess->env,1);      /* could've used a label */
  }

  mesh = tess->mesh;
  if( ! tess->fatalError ) {
    int rc = 1;

    /* Emit monotones BEFORE tessellating the interior because
     *  that reduces all monotone regions to seperate triangles
     */
    if (tess->emit_monotone || tess->emit_monotone_data) {
      glu_fastuidraw_gl_emitMonotones(tess, mesh);
    }

    if (tess->emit_boundary || tess->emit_boundary_data) {
      glu_fastuidraw_gl_emitBoundaries(tess, mesh);
    }

    rc = glu_fastuidraw_gl_meshTessellateInterior( mesh );
    if (rc == 0) longjmp(tess->env,1);  /* could've used a label */

    glu_fastuidraw_gl_meshCheckMesh( mesh );

    if( tess->callBegin != &noBegin || tess->callEnd != &noEnd
       || tess->callVertex != &noVertex
       || tess->callBeginData != &glu_fastuidraw_gl_noBeginData
       || tess->callEndData != &glu_fastuidraw_gl_noEndData
       || tess->callVertexData != &glu_fastuidraw_gl_noVertexData )
    {
      glu_fastuidraw_gl_renderMesh( tess, mesh );      /* output strips and fans */
    }
    if( tess->callMesh != &noMesh ) {

      /* Throw away the exterior faces, so that all faces are interior.
       * This way the user doesn't have to check the "inside" flag,
       * and we don't need to even reveal its existence.  It also leaves
       * the freedom for an implementation to not generate the exterior
       * faces in the first place.
       */
      glu_fastuidraw_gl_meshDiscardExterior( mesh );
      (*tess->callMesh)( mesh );                /* user wants the mesh itself */
      tess->mesh = nullptr;
      tess->polygonData= nullptr;
      return;
    }
  }

  glu_fastuidraw_gl_meshDeleteMesh( mesh );
  tess->polygonData= nullptr;
  tess->mesh = nullptr;
}


/*XXXblythe unused function*/
#if 0
void REGALFASTUIDRAW_GLU_CALL
gluDeleteMesh( GLUmesh *mesh )
{
  glu_fastuidraw_gl_meshDeleteMesh( mesh );
}
#endif



/*******************************************************/
