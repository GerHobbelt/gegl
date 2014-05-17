/* This file is an image processing operation for GEGL.
 *
 * This GEGL operation is a port of the main part of the Fractal
 * Explorer plug-in from GIMP. Fractal Explorer (Version 2) was
 * originally written by Daniel Cotting (cotting@multimania.com).
 *
 * GEGL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * GEGL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2006 Kevin Cozens <kcozens@cvs.gnome.org>
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#define MAXNCOLORS 8192

#ifdef GEGL_PROPERTIES

gegl_enum_start (gegl_fractal_explorer_type)
  gegl_enum_value (GEGL_FRACTAL_EXPLORER_TYPE_MANDELBROT, "Mandelbrot")
  gegl_enum_value (GEGL_FRACTAL_EXPLORER_TYPE_JULIA,      "Julia")
  gegl_enum_value (GEGL_FRACTAL_EXPLORER_TYPE_BARNSLEY_1, "Barnsley 1")
  gegl_enum_value (GEGL_FRACTAL_EXPLORER_TYPE_BARNSLEY_2, "Barnsley 2")
  gegl_enum_value (GEGL_FRACTAL_EXPLORER_TYPE_BARNSLEY_3, "Barnsley 3")
  gegl_enum_value (GEGL_FRACTAL_EXPLORER_TYPE_SPIDER,     "Spider")
  gegl_enum_value (GEGL_FRACTAL_EXPLORER_TYPE_MAN_O_WAR,  "Man O War")
  gegl_enum_value (GEGL_FRACTAL_EXPLORER_TYPE_LAMBDA,     "Lambda")
  gegl_enum_value (GEGL_FRACTAL_EXPLORER_TYPE_SIERPINSKI, "Sierpinski")
gegl_enum_end (GeglFractalExplorerType)

gegl_property_enum (fractaltype, _("Fractal type"),
    GeglFractalExplorerType, gegl_fractal_explorer_type,
    "description", _("Type of a fractal"),
    "default", GEGL_FRACTAL_EXPLORER_TYPE_MANDELBROT,
    NULL)
gegl_property_int (iter, _("Iterations"),
    "description", _("Iterations"),
    "default", 50, "min", 1, "max", 1000,
    NULL)
gegl_property_double (zoom, _("Zoom"),
    "description", _("Zoom in the fractal space"),
    "default", 300.0 , "min"   , 0.0000001, "max"     , 10000000.0,
    "ui-min",  0.0001, "ui-max", 10000.0  , "ui-gamma", 1.5,
   NULL)
gegl_property_double (shiftx, _("Shift X"),
    "description", _("X shift in the fractal space"),
    "ui-min", -1000.0, "ui-max",  1000.0, "ui-gamma", 1.5,
    NULL)
gegl_property_double (shifty, _("Shift Y"),
    "description", _("Y shift in the fractal space"),
    "ui-min", -1000.0, "ui-max",  1000.0, "ui-gamma", 1.5,
    NULL)
gegl_property_double (cx, _("CX"),
    "description", _("CX (No effect in Mandelbrot and Sierpinski)"),
    "default", -0.75, "min", -2.5, "max", 2.5,
    NULL)
gegl_property_double (cy, _("CY"),
    "description", _("CY (No effect in Mandelbrot and Sierpinski)"),
    "default", 0.2, "min", -2.5, "max", 2.5,
    NULL)
gegl_property_double (redstretch, _("Red stretch"),
    "description", _("Red stretching factor"), 
    "default", 1.0, "min", 0.0, "max", 1.0,
    NULL)
gegl_property_double (greenstretch, _("Green stretch"),
    "description", _("Green stretching factor"),
    "default", 1.0, "min", 0.0, "max", 1.0,
    NULL)
gegl_property_double (bluestretch, _("Blue stretch"),
    "description", _("Green stretching factor"), 
    "default", 1.0, "min", 0.0, "max", 1.0,
    NULL)

gegl_enum_start (gegl_fractal_explorer_mode)
  gegl_enum_value (GEGL_FRACTAL_EXPLORER_MODE_SIN , "Sine")
  gegl_enum_value (GEGL_FRACTAL_EXPLORER_MODE_COS , "Cosinus")
  gegl_enum_value (GEGL_FRACTAL_EXPLORER_MODE_NONE, "None")
gegl_enum_end (GeglFractalExplorerMode)

gegl_property_enum (redmode, _("Red mode"),
    GeglFractalExplorerMode, gegl_fractal_explorer_mode,
    "description",   _("Red application mode"),
    "default", GEGL_FRACTAL_EXPLORER_MODE_COS,
    NULL)

gegl_property_enum (greenmode, _("Green mode"),
    GeglFractalExplorerMode, gegl_fractal_explorer_mode,
    "default", GEGL_FRACTAL_EXPLORER_MODE_COS,
    "description",   _("Green application mode"), 
    NULL)

gegl_property_enum (bluemode, _("Blue mode"),
    GeglFractalExplorerMode, gegl_fractal_explorer_mode,
    "description", _("Blue application mode"),
    "default", GEGL_FRACTAL_EXPLORER_MODE_SIN,
    NULL)

gegl_property_boolean (redinvert  , _("Red inversion")  , NULL)
gegl_property_boolean (greeninvert, _("Green inversion"), NULL)
gegl_property_boolean (blueinvert , _("Blue inversion") , NULL)

gegl_property_int (ncolors, _("Colors"),
    "description", _("Number of colors"),
    "default", 256, "min", 2, "max", MAXNCOLORS,
    NULL)

gegl_property_boolean (useloglog, _("Loglog smoothing"), NULL)

#else

#define GEGL_OP_POINT_RENDER
#define GEGL_OP_C_FILE       "fractal-explorer.c"

#include "gegl-op.h"
#include <math.h>
#include <stdio.h>

typedef struct
{
  gfloat r, g, b;
} gfloatRGB;

typedef gfloatRGB  clrmap[MAXNCOLORS];

static void
make_color_map (GeglProperties *o, clrmap colormap)
{
  gint     i;
  gfloat   r;
  gfloat   gr;
  gfloat   bl;

  for (i = 0; i < o->ncolors; i++)
    {
      double x = (i*2.0) / o->ncolors;
      r = gr = bl = 0;

      switch (o->redmode)
        {
        case GEGL_FRACTAL_EXPLORER_MODE_SIN:
          r = 0.5 * o->redstretch *(1.0 + sin((x - 1) * G_PI));
          break;
        case GEGL_FRACTAL_EXPLORER_MODE_COS:
          r = 0.5 * o->redstretch *(1.0 + cos((x - 1) * G_PI));
          break;
        case GEGL_FRACTAL_EXPLORER_MODE_NONE:
          r = 0.5 * o->redstretch * x;
          break;
        default:
          break;
        }

      switch (o->greenmode)
        {
        case GEGL_FRACTAL_EXPLORER_MODE_SIN:
          gr = 0.5 * o->greenstretch *(1.0 + sin((x - 1) * G_PI));
          break;
        case GEGL_FRACTAL_EXPLORER_MODE_COS:
          gr = 0.5 * o->greenstretch *(1.0 + cos((x - 1) * G_PI));
          break;
        case GEGL_FRACTAL_EXPLORER_MODE_NONE:
          gr = 0.5 * o->greenstretch * x;
          break;
        default:
          break;
        }

      switch (o->bluemode)
        {
        case GEGL_FRACTAL_EXPLORER_MODE_SIN:
          bl = 0.5 * o->bluestretch * (1.0 + sin ((x - 1) * G_PI));
          break;
        case GEGL_FRACTAL_EXPLORER_MODE_COS:
          bl = 0.5 * o->bluestretch * (1.0 + cos ((x - 1) * G_PI));
          break;
        case GEGL_FRACTAL_EXPLORER_MODE_NONE:
          bl = 0.5 * o->bluestretch * x;
          break;
        default:
          break;
        }

      if (o->redinvert)
        r = 1.0 - r;

      if (o->greeninvert)
        gr = 1.0 - gr;

      if (o->blueinvert)
        bl = 1.0 - bl;

      colormap[i].r = r;
      colormap[i].g = gr;
      colormap[i].b = bl;
    }
}

static void
prepare (GeglOperation *operation)
{
  gegl_operation_set_format (operation, "output", babl_format ("RGBA float"));
}

static GeglRectangle
get_bounding_box (GeglOperation *operation)
{
  return gegl_rectangle_infinite_plane ();
}

static gboolean
process (GeglOperation       *operation,
         void                *out_buf,
         glong                n_pixels,
         const GeglRectangle *roi,
         gint                 level)
{
  GeglProperties *o = GEGL_PROPERTIES (operation);
  gfloat     *out_pixel = out_buf;
  gint        pixelx = roi->x; /* initial x                   */
  gint        pixely = roi->y; /*           and y coordinates */
  gdouble     x,y;             /* coordinate in fractal space */
  gdouble     a,b;             /* main fractal variable in iteration loop */
  gdouble     nexta;
  gdouble     tmpx, tmpy;
  gdouble     foldxinitx;
  gdouble     foldxinity;
  gdouble     foldyinitx;
  gdouble     foldyinity;
  gdouble     tempsqrx;
  gdouble     tempsqry;
  gdouble     olda,oldb;
  gdouble     adjust = 0.0;
  gint        counter;         /* iteration counter */
  gdouble     log2 = log (2.0);
  gint        color;

  clrmap  colormap;

  make_color_map (o, colormap);

  while (n_pixels--)
    {
      x = (pixelx + o->shiftx) / o->zoom;
      y = (pixely + o->shifty) / o->zoom;

      switch (o->fractaltype)
        {
        case GEGL_FRACTAL_EXPLORER_TYPE_MANDELBROT:
          a = b = 0;
          tmpx = tmpy = 0;
          break;
        default:
          tmpx = a = x;
          tmpy = b = y;
        }

      for (counter = 0; counter < o->iter; counter++)
        {
          olda = a;
          oldb = b;

          switch (o->fractaltype)
            {
            case GEGL_FRACTAL_EXPLORER_TYPE_MANDELBROT:
              nexta = a * a - b * b + x;
              b = 2.0 * a * b + y;
              break;

            case GEGL_FRACTAL_EXPLORER_TYPE_JULIA:
              nexta = a * a - b * b + o->cx;
              b = 2.0 * a * b + o->cy;
              break;

            case GEGL_FRACTAL_EXPLORER_TYPE_BARNSLEY_1:
              foldxinitx = olda * o->cx;
              foldyinity = oldb * o->cy;
              foldxinity = olda * o->cy;
              foldyinitx = oldb * o->cx;
              /* orbit calculation */
              if (olda >= 0)
                {
                  nexta = (foldxinitx - o->cx - foldyinity);
                  b  = (foldyinitx - o->cy + foldxinity);
                }
              else
                {
                  nexta = (foldxinitx + o->cx - foldyinity);
                  b  = (foldyinitx + o->cy + foldxinity);
                }
              break;

            case GEGL_FRACTAL_EXPLORER_TYPE_BARNSLEY_2:
              foldxinitx = olda * o->cx;
              foldyinity = oldb * o->cy;
              foldxinity = olda * o->cy;
              foldyinitx = oldb * o->cx;
              /* orbit calculation */
              if (foldxinity + foldyinitx >= 0)
                {
                  nexta = foldxinitx - o->cx - foldyinity;
                  b  = foldyinitx - o->cy + foldxinity;
                }
              else
                {
                  nexta = foldxinitx + o->cx - foldyinity;
                  b  = foldyinitx + o->cy + foldxinity;
                }
              break;

            case GEGL_FRACTAL_EXPLORER_TYPE_BARNSLEY_3:
              foldxinitx  = olda * olda;
              foldyinity  = oldb * oldb;
              foldxinity  = olda * oldb;
              /* orbit calculation */
              if (olda > 0)
                {
                  nexta = foldxinitx - foldyinity - 1.0;
                  b  = foldxinity * 2;
                }
              else
                {
                  nexta = foldxinitx - foldyinity -1.0 + o->cx * olda;
                  b  = foldxinity * 2;
                  b += o->cy * olda;
                }
              break;

            case GEGL_FRACTAL_EXPLORER_TYPE_SPIDER:
              /* { c=z=pixel: z=z*z+c; c=c/2+z, |z|<=4 } */
              nexta = a*a - b*b + tmpx + o->cx;
              b = 2 * olda * oldb + tmpy + o->cy;
              tmpx = tmpx/2 + nexta;
              tmpy = tmpy/2 + b;
              break;

            case GEGL_FRACTAL_EXPLORER_TYPE_MAN_O_WAR:
              nexta = a*a - b*b + tmpx + o->cx;
              b = 2.0 * a * b + tmpy + o->cy;
              tmpx = olda;
              tmpy = oldb;
              break;

            case GEGL_FRACTAL_EXPLORER_TYPE_LAMBDA:
              tempsqrx = a * a;
              tempsqry = b * b;
              tempsqrx = olda - tempsqrx + tempsqry;
              tempsqry = -(oldb * olda);
              tempsqry += tempsqry + oldb;
              nexta = o->cx * tempsqrx - o->cy * tempsqry;
              b = o->cx * tempsqry + o->cy * tempsqrx;
              break;

            case GEGL_FRACTAL_EXPLORER_TYPE_SIERPINSKI:
              nexta = olda + olda;
              b = oldb + oldb;
              if (oldb > .5)
                b = b - 1;
              else if (olda > .5)
                nexta = nexta - 1;
              break;

            default:
              g_warning (_("Unsupported fractal type: %d"), o->fractaltype);
              return FALSE;
            }

          a = nexta;

          if (((a * a) + (b * b)) >= 4.0)
            break;
        }

      if (o->useloglog)
        {
          gdouble modulus_square = (a * a) + (b * b);

          if (modulus_square > (G_E * G_E))
              adjust = log (log (modulus_square) / 2.0) / log2;
          else
              adjust = 0.0;
        }

      color = (gint) (((counter - adjust) * (o->ncolors - 1)) / o->iter);

      out_pixel[0] = colormap[color].r;
      out_pixel[1] = colormap[color].g;
      out_pixel[2] = colormap[color].b;
      out_pixel[3] = 1.0;

      out_pixel += 4;

      /* update x and y coordinates */
      pixelx++;
      if (pixelx>=roi->x + roi->width)
        {
          pixelx=roi->x;
          pixely++;
        }
    }

  return TRUE;
}


static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass            *operation_class;
  GeglOperationPointRenderClass *point_render_class;

  operation_class    = GEGL_OPERATION_CLASS (klass);
  point_render_class = GEGL_OPERATION_POINT_RENDER_CLASS (klass);

  point_render_class->process = process;
  operation_class->get_bounding_box = get_bounding_box;
  operation_class->prepare = prepare;

  gegl_operation_class_set_keys (operation_class,
    "name"       , "gegl:fractal-explorer",
    "categories" , "render",
    "description", _("Fractal Explorer"),
    NULL);
}

#endif
