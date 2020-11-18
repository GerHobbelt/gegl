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
 * License along with GEGL; if not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright 2006 Øyvind Kolås <pippin@gimp.org>
 */

#include "config.h"
#include <glib/gi18n-lib.h>


#ifdef GEGL_PROPERTIES

property_string (name, _("Name"), "sRGB")
   description (_("One of: sRGB, Adobish, Rec2020, ProPhoto, Apple, ACEScg, ACES2065-1"))
property_format (pointer, _("Pointer"), NULL)
   description (_("pointer to a const * Babl space"))
property_file_path (path, _("Path"), "")
  description (_("File system path to ICC matrix profile to load"))

#else

#define GEGL_OP_FILTER
#define GEGL_OP_NAME            convert_space
#define GEGL_OP_C_SOURCE        convert-space.c

#include "gegl-op.h"
#include <stdio.h>

static void
gegl_convert_space_prepare (GeglOperation *operation)
{
  const Babl *aux_format = gegl_operation_get_source_format (operation,
                                                            "aux");
  GeglProperties *o = GEGL_PROPERTIES (operation);
  const Babl *space = babl_space (o->name);
  if (o->pointer)
    space = o->pointer;
  if (o->path)
  {
    gchar *icc_data = NULL;
    gsize icc_length;
    g_file_get_contents (o->path, &icc_data, &icc_length, NULL);
    if (icc_data)
    {
      const char *error = NULL;
      const Babl *s = babl_space_from_icc (icc_data, (gint)icc_length,
                                 BABL_ICC_INTENT_RELATIVE_COLORIMETRIC, &error);
      if (s) space = s;
      g_free (icc_data);
    }
  }
  if (aux_format)
  {
    space = babl_format_get_space (aux_format);
  }

  gegl_operation_set_format (operation, "output",
                             babl_format_with_space ("RGBA float", space));
}

static gboolean
gegl_convert_space_process (GeglOperation        *operation,
                            GeglOperationContext *context,
                            const gchar          *output_prop,
                            const GeglRectangle  *result,
                            gint                  level)
{
  GeglBuffer *input;

  input = GEGL_BUFFER (gegl_operation_context_get_object (context, "input"));
  if (!input)
    {
      return FALSE;
    }

  gegl_operation_context_take_object (context, "output",
                                      g_object_ref ((GObject *) input));
  return TRUE;
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass *operation_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  operation_class->process = gegl_convert_space_process;
  operation_class->prepare = gegl_convert_space_prepare;

  gegl_operation_class_set_keys (operation_class,
              "name",        "gegl:convert-space",
              "title",       _("Convert color space"),
              "categories",  "core:color",
              "description", _("set color space which subsequent babl-formats in the pipeline are created with, and the ICC profile potentially embedded for external color management, setting a pointer to a format overrides the string property and setting an aux pad overrides both. "),
              NULL);
}

#endif
