
/*

  Tessellation only GLU library with FASTUIDRAW modifications
   - all but tessellation removed
   - arbitary winding rule
   - prefix public functions with fastuidraw_
   - prefix public macros with FASTUIDRAW_
   - change source and header files to cpp and hpp extension
   - so that API header allows for C++ niceties of overloading
   - begin call backs pass a winding number
   - remove triangle strips and fans rendering
   - remove edge flag call back
   - points provided are 2D only (skip computation of plane and projection)

  Original liscense is below

 */

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

#pragma once

#include <climits>

/* the ID FASTUIDRAW_GLU_nullptr_CLIENT_ID is reserved for use
   by FASTUIDRAW_GLU to represent a "nullptr" vertex, do not
   use this ID (which is set to max unsigned int).
 */
#define FASTUIDRAW_GLU_nullptr_CLIENT_ID (UINT_MAX)

/*************************************************************/

/* Boolean */
#define FASTUIDRAW_GLU_FALSE                          0
#define FASTUIDRAW_GLU_TRUE                           1
typedef unsigned char   FASTUIDRAW_GLUboolean;
typedef unsigned int    FASTUIDRAW_GLUenum;


#define FASTUIDRAW_GLU_INVALID_ENUM                   100900
#define FASTUIDRAW_GLU_INVALID_VALUE                  100901
#define FASTUIDRAW_GLU_OUT_OF_MEMORY                  100902
#define FASTUIDRAW_GLU_INCOMPATIBLE_GL_VERSION        100903
#define FASTUIDRAW_GLU_INVALID_OPERATION              100904


/* Primitive type, avoid dependence on GL headers */
#define FASTUIDRAW_GLU_LINE_LOOP 0x0002
#define FASTUIDRAW_GLU_TRIANGLES 0x0004


/* TessCallback */
#define FASTUIDRAW_GLU_TESS_BEGIN                     100100
#define FASTUIDRAW_GLU_BEGIN                          100100
typedef void (*fastuidraw_glu_tess_function_begin)( FASTUIDRAW_GLUenum type, int winding_number);


#define FASTUIDRAW_GLU_TESS_VERTEX                    100101
#define FASTUIDRAW_GLU_VERTEX                         100101
typedef void (*fastuidraw_glu_tess_function_vertex)(unsigned int vertex_id);

#define FASTUIDRAW_GLU_TESS_END                       100102
#define FASTUIDRAW_GLU_END                            100102
typedef void (*fastuidraw_glu_tess_function_end)(void);

#define FASTUIDRAW_GLU_TESS_ERROR                     100103
typedef void (*fastuidraw_glu_tess_function_error)(FASTUIDRAW_GLUenum errnum);


/* Combine is guaranteed to combine 4 vertices coming
   from two edges:
     data[0] : vertex A for edge E
     data[1] : vertex B for edge E
     data[2] : vertex A for edge F
     data[3] : vertex B for edge F
 */
#define FASTUIDRAW_GLU_TESS_COMBINE                   100105
typedef void (*fastuidraw_glu_tess_function_combine)(double x, double y, unsigned int data[4],
                                                     double weight[4], unsigned int *outData);

#define FASTUIDRAW_GLU_TESS_BEGIN_DATA                100106
typedef void (*fastuidraw_glu_tess_function_begin_data)(FASTUIDRAW_GLUenum type, int winding_number, void *polygon_data);

#define FASTUIDRAW_GLU_TESS_VERTEX_DATA               100107
typedef void (*fastuidraw_glu_tess_function_vertex_data)(unsigned int vertex_id, void *polygon_data);

#define FASTUIDRAW_GLU_TESS_END_DATA                  100108
typedef void (*fastuidraw_glu_tess_function_end_data)(void *polygon_data);

#define FASTUIDRAW_GLU_TESS_ERROR_DATA                100109
typedef void (*fastuidraw_glu_tess_function_error_data)(FASTUIDRAW_GLUenum errnum, void *polygon_data);

#define FASTUIDRAW_GLU_TESS_COMBINE_DATA              100111
typedef void (*fastuidraw_glu_tess_function_combine_data)(double x, double y, unsigned int data[4],
                                                          double weight[4], unsigned int *outData,
                                                          void *polygon_data);

/*
  additions from FASTUIDRAW, use a call back for the winding rule
  function signature is:

  FASTUIDRAW_GLU_TESS_WINDING_CALLBACK--> FASTUIDRAW_GLUboolean fill_region(int winding_number)
  FASTUIDRAW_GLU_TESS_WINDING_CALLBACK_DATA --> FASTUIDRAW_GLUboolean fill_region(int winding_number, void *polygon_data)

  return GL_TRUE if the winding_number dictates to fill the region
  return GL_FALSE if the winding_number dictates to not fill the region
*/
#define FASTUIDRAW_GLU_TESS_WINDING_CALLBACK 200100
typedef FASTUIDRAW_GLUboolean (*fastuidraw_glu_tess_function_winding)(int winding_number);

#define FASTUIDRAW_GLU_TESS_WINDING_CALLBACK_DATA 200101
typedef FASTUIDRAW_GLUboolean (*fastuidraw_glu_tess_function_winding_data)(int winding_number, void *polygon_data);

/*
  additions from FASTUIDRAW, use a call back for the winding rule
  function signature is:

  FASTUIDRAW_GLU_TESS_EMIT_MONOTONE --> void emit_monotone(int winding, const unsigned int vertex_ids[], const int winding_nbs[], unsigned int count)
  FASTUIDRAW_GLU_TESS_EMIT_MONOTONE_DATA --> void emit_monotone_data(int winding, const unsigned int vertex_ids[], const int winding_nbs[], unsigned int count, void *polygon_data)

  Is called to emit the monotone polygons BEFORE they are triangulated.
   - winding    : winding number of monotone polygone
   - vertex_ids : vertices of monotone polygon
   - winding_nbs: i'th winding number of the region that shares the edge vertex_ids[i] to vertex_ids[i + 1]
   - count      : number of points of the monotone region
*/
#define FASTUIDRAW_GLU_TESS_EMIT_MONOTONE 200102
typedef void (*fastuidraw_glu_tess_function_emit_monotone)(int winding,
                                                           const unsigned int vertex_ids[],
                                                           const int winding_ids[],
                                                           unsigned int count);
#define FASTUIDRAW_GLU_TESS_EMIT_MONOTONE_DATA 200103
typedef void (*fastuidraw_glu_tess_function_emit_monotone_data)(int winding,
                                                                const unsigned int vertex_ids[],
                                                                const int winding_ids[],
                                                                unsigned int count,
                                                                void *polygon_data);

/* if client requests to capture regions with winding == 0 as well,
 * it needs to supply to GLU what ID's to sue for the corners of the
 * boundary that it induces.
 * \param x x-coordinate GLU is to use for the boundary point
 * \param y y-coordinate GLU is to use for the boundary point
 * \param step how many times to move from boundary in units that
               guarantee that the vertex is distinct.
 * \param is_max_x : true if asking for x-max point, false if asking for x-min point
 * \param is_max_y : true if asking for y-max point, false if asking for y-min point
 * \param outData : if non-null, location to which to write the vertex ID; if nullptr,
                    the vertex is only used internally by the tessellator.
 */
#define FASTUIDRAW_GLU_TESS_BOUNDARY_CORNER 200104
typedef void (*fastuidraw_glu_tess_function_boundary_corner_point)(double *x, double *y, int step,
                                                                   FASTUIDRAW_GLUboolean is_max_x,
                                                                   FASTUIDRAW_GLUboolean is_max_y,
                                                                   unsigned int *outData);
#define FASTUIDRAW_GLU_TESS_BOUNDARY_CORNER_DATA 200105
typedef void (*fastuidraw_glu_tess_function_boundary_corner_point_data)(double *x, double *y, int step,
                                                                        FASTUIDRAW_GLUboolean is_max_x,
                                                                        FASTUIDRAW_GLUboolean is_max_y,
                                                                        unsigned int *outData,
                                                                        void *polygon_data);

/* TessContour */
#define FASTUIDRAW_GLU_CW                             100120
#define FASTUIDRAW_GLU_CCW                            100121
#define FASTUIDRAW_GLU_INTERIOR                       100122
#define FASTUIDRAW_GLU_EXTERIOR                       100123
#define FASTUIDRAW_GLU_UNKNOWN                        100124

/* TessProperty */
#define FASTUIDRAW_GLU_TESS_BOUNDARY_ONLY             100141
#define FASTUIDRAW_GLU_TESS_TOLERANCE                 100142

/* TessError */
#define FASTUIDRAW_GLU_TESS_ERROR1                    100151
#define FASTUIDRAW_GLU_TESS_ERROR2                    100152
#define FASTUIDRAW_GLU_TESS_ERROR3                    100153
#define FASTUIDRAW_GLU_TESS_ERROR4                    100154
#define FASTUIDRAW_GLU_TESS_ERROR5                    100155
#define FASTUIDRAW_GLU_TESS_ERROR6                    100156
#define FASTUIDRAW_GLU_TESS_ERROR7                    100157
#define FASTUIDRAW_GLU_TESS_ERROR8                    100158
#define FASTUIDRAW_GLU_TESS_MISSING_BEGIN_POLYGON     100151
#define FASTUIDRAW_GLU_TESS_MISSING_BEGIN_CONTOUR     100152
#define FASTUIDRAW_GLU_TESS_MISSING_END_POLYGON       100153
#define FASTUIDRAW_GLU_TESS_MISSING_END_CONTOUR       100154
#define FASTUIDRAW_GLU_TESS_COORD_TOO_LARGE           100155
#define FASTUIDRAW_GLU_TESS_NEED_COMBINE_CALLBACK     100156


/*************************************************************/



class fastuidraw_GLUtesselator;

typedef fastuidraw_GLUtesselator fastuidraw_GLUtesselatorObj;
typedef fastuidraw_GLUtesselator fastuidraw_GLUtriangulatorObj;

#define FASTUIDRAW_GLU_TESS_MAX_COORD 1.0e150

/* Internal convenience typedefs */
typedef void (*FASTUIDRAW_GLUfuncptr)(void);


fastuidraw_GLUtesselator*
fastuidraw_gluNewTess_debug(const char *file, int line);

void
fastuidraw_gluDeleteTess_debug(fastuidraw_GLUtesselator* tess, const char *file, int line);

void
fastuidraw_gluDeleteTess_release(fastuidraw_GLUtesselator* tess);

fastuidraw_GLUtesselator*
fastuidraw_gluNewTess_release(void);


#ifdef FASTUIDRAW_DEBUG

#define fastuidraw_gluNewTess fastuidraw_gluNewTess_debug(__FILE__, __LINE__);
#define fastuidraw_gluDeleteTess(tess) fastuidraw_gluDeleteTess_debug(tess, __FILE__, __LINE__)

#else

#define fastuidraw_gluNewTess fastuidraw_gluNewTess_release();
#define fastuidraw_gluDeleteTess(tess) fastuidraw_gluDeleteTess_release(tess)

#endif


void fastuidraw_gluTessBeginContour (fastuidraw_GLUtesselator* tess, FASTUIDRAW_GLUboolean contour_real);
void fastuidraw_gluTessBeginPolygon (fastuidraw_GLUtesselator* tess, void* data);
void fastuidraw_gluTessEndContour (fastuidraw_GLUtesselator* tess);
void fastuidraw_gluTessEndPolygon (fastuidraw_GLUtesselator* tess);
void fastuidraw_gluTessVertex (fastuidraw_GLUtesselator* tess, double x, double y, unsigned int vertex_id);

void fastuidraw_gluTessCallback (fastuidraw_GLUtesselator* tess, FASTUIDRAW_GLUenum which, FASTUIDRAW_GLUfuncptr CallBackFunc);


/*
  set and fetch the merging tolerance
 */
double fastuidraw_gluTessPropertyTolerance(fastuidraw_GLUtesselator* tess);
void fastuidraw_gluTessPropertyTolerance(fastuidraw_GLUtesselator *tess, double value);

/*
  set and fetch weather or not to only provide line loops
  decribing the boundary, using FASTUIDRAW_GLU_FALSE and FASTUIDRAW_GLU_TRUE.
  If value is FASTUIDRAW_GLU_TRUE, then the boundary is provided
  only as a sequence of line loops. If value is FASTUIDRAW_GLU_FALSE, then
  tessellates according to the fill rule.
 */
int fastuidraw_gluGetTessPropertyBoundaryOnly(fastuidraw_GLUtesselator *tess);
void fastuidraw_gluTessPropertyBoundaryOnly(fastuidraw_GLUtesselator *tess, int value);


#define FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(which, type, label)                \
  void fastuidraw_gluTessCallback##label(fastuidraw_GLUtesselator *tess, type f)


FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(FASTUIDRAW_GLU_TESS_BEGIN, fastuidraw_glu_tess_function_begin, Begin);
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(FASTUIDRAW_GLU_TESS_VERTEX, fastuidraw_glu_tess_function_vertex, Vertex);
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(FASTUIDRAW_GLU_TESS_END, fastuidraw_glu_tess_function_end, End);
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(FASTUIDRAW_GLU_TESS_ERROR, fastuidraw_glu_tess_function_error, Error);
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(FASTUIDRAW_GLU_TESS_COMBINE, fastuidraw_glu_tess_function_combine, Combine);
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(FASTUIDRAW_GLU_TESS_WINDING_CALLBACK, fastuidraw_glu_tess_function_winding, FillRule);
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(FASTUIDRAW_GLU_TESS_EMIT_MONOTONE, fastuidraw_glu_tess_function_emit_monotone, EmitMonotone);
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(FASTUIDRAW_GLU_TESS_BOUNDARY_CORNER, fastuidraw_glu_tess_function_boundary_corner_point, BoundaryCornerPoint);

FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(FASTUIDRAW_GLU_TESS_BEGIN_DATA, fastuidraw_glu_tess_function_begin_data, Begin);
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(FASTUIDRAW_GLU_TESS_VERTEX_DATA, fastuidraw_glu_tess_function_vertex_data, Vertex);
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(FASTUIDRAW_GLU_TESS_END_DATA, fastuidraw_glu_tess_function_end_data, End);
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(FASTUIDRAW_GLU_TESS_ERROR_DATA, fastuidraw_glu_tess_function_error_data, Error);
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(FASTUIDRAW_GLU_TESS_COMBINE_DATA, fastuidraw_glu_tess_function_combine_data, Combine);
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(FASTUIDRAW_GLU_TESS_WINDING_CALLBACK_DATA, fastuidraw_glu_tess_function_winding_data, FillRule);
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(FASTUIDRAW_GLU_TESS_EMIT_MONOTONE_DATA, fastuidraw_glu_tess_function_emit_monotone_data, EmitMonotone);
FASTUIDRAW_GLU_TESS_TYPE_SAFE_CALL_BACK(FASTUIDRAW_GLU_TESS_BOUNDARY_CORNER_DATA, fastuidraw_glu_tess_function_boundary_corner_point_data, BoundaryCornerPoint);
