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
 * 2022 Beaver (GEGL inner glow) *2023 InnerShadow for GEGL Styles
 */

 /* The name innerglow has already been taken by Beaver's most popular text styling plugin GEGL Effects. Which is a incompatible fork of this. (they cannot be swapped)
    Releases of GEGL Effects post May 2023 use (beaver:innerglow) name space but for now thousands of people are using the plugin and they will experience a breakage
    if they don't rountinely update. I assume they don't. */

 /* This filter is a stand alone meant to be fused with Gimp's blending options. But it also is meant to be called with GEGL Styles 
    From a technical perspective this is literally inverted transparency a drop shadow then removal of the color fill that drop shadow applied too*/


#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_PROPERTIES

#define IGGUIDE \
"id=1 dst-atop aux=[ ref=1 color value=#ffffff ] crop xor srgb=true aux=[ ref=1 ] color-overlay value=#000000 "\
 /* This is a GEGL Graph. It replaces color with transparency and transparency with color. */


enum_start (gegl_innerglow_grow_shape)
  enum_value (GEGL_INNERGLOW_GROW_SHAPE_SQUARE,  "squareig",  N_("Square"))
  enum_value (GEGL_INNERGLOW_GROW_SHAPE_CIRCLE,  "circleig",  N_("Circle"))
  enum_value (GEGL_INNERGLOW_GROW_SHAPE_DIAMOND, "diamondig", N_("Diamond"))
enum_end (innerglowshape)


property_enum   (grow_shape, _("Grow shape"),
                 innerglowshape, gegl_innerglow_grow_shape,
                 GEGL_INNERGLOW_GROW_SHAPE_CIRCLE)
  description   (_("The shape to expand or contract the shadow in"))


property_double (x, _("X"), 0.0)
  description   (_("Horizontal shadow offset"))
  ui_range      (-15.0, 15.0)
  value_range   (-15.0, 15.0)
  ui_steps      (1, 2)
  ui_meta       ("unit", "pixel-distance")
  ui_meta       ("axis", "x")

property_double (y, _("Y"), 0.0)
  description   (_("Vertical shadow offset"))
  ui_range      (-15.0, 15.0)
  value_range   (-15.0, 15.0)
  ui_steps      (1, 2)
  ui_meta       ("unit", "pixel-distance")
  ui_meta       ("axis", "y")



property_double (radius, _("Blur radius"), 7.5)
  value_range   (0.0, 40.0)
  ui_range      (0.0, 30.0)
  ui_steps      (1, 5)
  ui_gamma      (1.5)
  ui_meta       ("unit", "pixel-distance")


property_double (grow_radius, _("Grow radius"), 4.0)
  value_range   (2, 30.0)
  ui_range      (2, 30.0)
  ui_digits     (0)
  ui_steps      (1, 5)
  ui_gamma      (1.5)
  ui_meta       ("unit", "pixel-distance")
  description (_("The distance to expand the shadow before blurring; a negative value will contract the shadow instead"))

property_double (opacity, _("Opacity"), 1.2)
  value_range   (0.0, 2.0)
  ui_steps      (0.01, 0.10)


property_color (value, _("Color"), "#fbff00")
    description (_("The color to paint over the input"))
    ui_meta     ("role", "color-primary")


property_double  (cover, _("Median fix for non-effected pixels on edges"), 60)
  value_range (50, 100)
  description (_("Median Blur covers uneffected pixels. Making this slider too high will make it outline-like. So only slide it has high as you need to cover thin shape corners.'"))
 /* This option has to be visible as certain inner shadows will look bad if it is locked at 100. And if it is disabled all the pixels on the text's edges will be unmodified.  */

#else

#define GEGL_OP_META
#define GEGL_OP_NAME     gegl_inner_glow
#define GEGL_OP_C_SOURCE inner-glow.c

#include "gegl-op.h"

static void attach (GeglOperation *operation)
{
  GeglNode *gegl = operation->node;
  GeglNode *input, *it, *shadow, *hiddencolor, *color, *atop, *median, *crop, *in,  *output;
  GeglColor *hidden_color = gegl_color_new ("#00ffffAA");
 /* Inner Glow's GEGL Graph will break without this hidden color */

  input    = gegl_node_get_input_proxy (gegl, "input");
  output   = gegl_node_get_output_proxy (gegl, "output");


  it    = gegl_node_new_child (gegl,
                                  "operation", "gegl:gegl", "string", IGGUIDE,
                                  NULL);
 /* Inner Glow's GEGL Graph will break without this hidden GEGL graph.*/

  in    = gegl_node_new_child (gegl,
                                  "operation", "gegl:src-in",
                                  NULL);

  shadow    = gegl_node_new_child (gegl,
                                  "operation", "gegl:dropshadow",
                                  NULL);

  hiddencolor    = gegl_node_new_child (gegl,
                                  "operation", "gegl:color-overlay",
                                   "value", hidden_color, NULL);
 /* Inner Glow's GEGL Graph will break without this hidden color */

  color    = gegl_node_new_child (gegl,
                                  "operation", "gegl:color-overlay",
                                  NULL);

  atop    = gegl_node_new_child (gegl,
                                  "operation", "gegl:src-atop",
                                  NULL);


  crop    = gegl_node_new_child (gegl,
                                  "operation", "gegl:crop",
                                  NULL);

  median     = gegl_node_new_child (gegl, "operation", "gegl:median-blur",
                                         "radius",       1,
                                         NULL);
 /* This is meant so inner glow to reach pixels in tight corners */

gegl_operation_meta_redirect (operation, "grow_radius", shadow, "grow-radius");
gegl_operation_meta_redirect (operation, "radius", shadow, "radius");
gegl_operation_meta_redirect (operation, "opacity", shadow, "opacity");
gegl_operation_meta_redirect (operation, "grow_shape", shadow, "grow-shape");
gegl_operation_meta_redirect (operation, "value", color, "value");
gegl_operation_meta_redirect (operation, "x", shadow, "x");
gegl_operation_meta_redirect (operation, "y", shadow, "y");
gegl_operation_meta_redirect (operation, "cover", median, "alpha-percentile");


 gegl_node_link_many (input, it,  shadow, hiddencolor, atop, in, median, color, crop, output, NULL);
 gegl_node_connect (in, "aux", input, "output");

}

static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass *operation_class;

  operation_class = GEGL_OPERATION_CLASS (klass);

  operation_class->attach = attach;

  gegl_operation_class_set_keys (operation_class,
    "name",        "gegl:inner-glow",
    "title",       _("Inner Glow"),
    "reference-hash", "1g3do6aaoo1100g0fjf25sb2ac",
    "description", _("GEGL does an inner shadow glow effect; for more interesting use different blend mode than the default, Replace."),
    "gimp:menu-path", "<Image>/Filters/Light and Shadow/",
    "gimp:menu-label", _("Inner Glow..."),
    NULL);
}

#endif
