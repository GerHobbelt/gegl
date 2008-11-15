/* This file is an image processing operation for GEGL
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
 * Copyright 2006 Øyvind Kolås <pippin@gimp.org>
 */

#include "config.h"
#include <glib/gi18n-lib.h>


#ifdef GEGL_CHANT_PROPERTIES

gegl_chant_double (value, _("Opacity"), -10.0, 10.0, 1.0,
         _("Global opacity value that is always used on top of the optional auxiliary input buffer."))

#else

#define GEGL_CHANT_TYPE_POINT_COMPOSER
#define GEGL_CHANT_C_FILE       "opacity.c"

#include "gegl-chant.h"


static void prepare (GeglOperation *self)
{
  gegl_operation_set_format (self, "input", babl_format ("RaGaBaA float"));
  gegl_operation_set_format (self, "output", babl_format ("RaGaBaA float"));
  gegl_operation_set_format (self, "aux", babl_format ("Y float"));
}

static gboolean
process (GeglOperation       *op,
         void                *in_buf,
         void                *aux_buf,
         void                *out_buf,
         const glong          n_pixels,
         const GeglRectangle *roi)
{
  gfloat *in = in_buf;
  gfloat *out = out_buf;
  gfloat *aux = aux_buf;
  gfloat value = GEGL_CHANT_PROPERTIES (op)->value;

  if (aux == NULL)
    {
      if (value == 1.0)
        {
          gint i;
          for (i=0; i<n_pixels; i++)
            {
              gint j;
              for (j=0; j<4; j++)
                out[j] = in[j];
              in  += 4;
              out += 4;
            }
        }
      else
        {
          gint i;
          for (i=0; i<n_pixels; i++)
            {
              gint j;
              for (j=0; j<4; j++)
                out[j] = in[j] * value;
              in  += 4;
              out += 4;
            }
        }
    }
  else if (value == 1.0)
    {
      gint i;
      for (i=0; i<n_pixels; i++)
        {
          gint j;
          for (j=0; j<4; j++)
            out[j] = in[j] * (*aux);
          in  += 4;
          out += 4;
          aux += 1;
        }
    }
  else
    {
      gint i;
      for (i=0; i<n_pixels; i++)
        {
          gfloat v = (*aux) * value;
          gint j;
          for (j=0; j<4; j++)
            out[j] = in[j] * v;
          in  += 4;
          out += 4;
          aux += 1;
        }
    }
  return TRUE;
}


static void
gegl_chant_class_init (GeglChantClass *klass)
{
  GeglOperationClass              *operation_class;
  GeglOperationPointComposerClass *point_composer_class;

  operation_class      = GEGL_OPERATION_CLASS (klass);
  point_composer_class = GEGL_OPERATION_POINT_COMPOSER_CLASS (klass);

  point_composer_class->process = process;
  operation_class->prepare = prepare;

  /* overriding the caching behavior that point-composers do
   * not have caches, this means that an opacity op can be
   * inserted where you want a cache to be in the graph
   */
  operation_class->no_cache = FALSE;

  operation_class->name        = "gegl:opacity";
  operation_class->categories  = "transparency";
  operation_class->description =
        _("Weights the opacity of the input both the value of the aux"
          " input and the global value property.");
}

#endif
