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
 * Copyright 2024 Øyvind Kolås <pippin@gimp.org>
 */

#include "config.h"
#include <glib/gi18n-lib.h>


#if 1
#include <stdio.h>
#define DEV_MODE 1
#else
#define DEV_MODE 0
#endif

#ifdef GEGL_PROPERTIES

property_int  (iterations, _("iterations"), 4)
               description("How many times to run optimization")
               value_range (0, 64)
#if DEV_MODE==0
               ui_meta("visible", "0")
#endif

property_int  (chance, _("chance"), 100)
               description("Chance of doing optimization")
               value_range (1, 100)
#if DEV_MODE==0
               ui_meta("visible", "0")
#endif

property_int  (phase, _("phase"), 0)
               value_range (0, 16)

property_double (noise_gamma, "noise gamma", 1.4)
               value_range (0.0, 4.0)

property_boolean (grow_shrink, "permit grow/shrink", TRUE)
               description("not active for levels==2")
property_boolean (post_process, "fill in big deviations", FALSE)

property_int  (levels, _("levels"), 3)
               description(_("Only used if no aux image is provided"))
               value_range (2, 255)

property_int  (center_bias, _("center bias"), 32)
               value_range (0, 255)
#if DEV_MODE==0
               ui_meta("visible", "0")
#endif

property_seed (seed, _("Random seed"), rand)
#if DEV_MODE==0
               ui_meta("visible", "0")
#endif

#else

#define GEGL_OP_COMPOSER
#define GEGL_OP_NAME     shuffle_search
#define GEGL_OP_C_SOURCE shuffle-search.c

#include "gegl-op.h"


#include <stdio.h>

static void
prepare (GeglOperation *operation)
{
  GeglRectangle *aux_bounds = gegl_operation_source_get_bounding_box(operation,"aux");

  if (aux_bounds && aux_bounds->width)
  {
    GeglNode *aux_node = gegl_operation_get_source_node (operation, "aux");
    GeglOperation *aux_op = gegl_node_get_gegl_operation(aux_node);
    gegl_operation_set_format (operation, "output",
      gegl_operation_get_format (aux_op, "output"));
  }
  else
  {
    const Babl *space = gegl_operation_get_source_space (operation, "input");
    gegl_operation_set_format (operation, "output",
                               babl_format_with_space ("Y' u16", space));
  }
}

static uint16_t compute_val(int center_bias, const uint16_t *bits, int stride, int x, int y)
{
  int count = 0;
  long int sum = 0;
  for (int v = y-1; v <= y+1; v++)
  for (int u = x-1; u <= x+1; u++)
  {
    int val = bits[v*stride+u];
    int contrib = 32 + (u == x && v == y) * center_bias;
    count += contrib;
    sum += val * contrib;
  }
  return (sum)/count;
}

static void compute_rgb(int center_bias, const uint8_t *rgb, int stride, int x, int y, uint8_t *rgb_out)
{
  int count = 0;
  int sum[3] = {0,0,0};

  for (int v = y-1; v <= y+1; v++)
  for (int u = x-1; u <= x+1; u++)
  {
      int o = 3*(v*stride+u);
      int contrib = 32 + (u == x && v == y) * center_bias;
      count += contrib;
      for (int c = 0; c < 3; c++)
        sum[c] += rgb[o+0] * contrib;
  }

  for (int c = 0; c < 3; c++)
    rgb_out[c] = sum[c] / count;
}

static int rgb_diff(const uint8_t *a, const uint8_t* b)
{
  float sum_sq_diff = 0.0f;
  for (int c = 0; c < 3; c++)
    sum_sq_diff += (a[c]-b[c])*(a[c]-b[c]);
  return sqrtf(sum_sq_diff);
}

enum {
  MUTATE_NONE  = 0,
  MUTATE_HSWAP,
  MUTATE_VSWAP,
  MUTATE_DSWAP,
  MUTATE_DSWAP2,
  MUTATE_HGROW,
  MUTATE_HSHRINK,
  MUTATE_VGROW,
  MUTATE_VSHRINK,
  MUTATE_COUNT
};


static void
pp_rect (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *aux,
         GeglBuffer          *output,
         const GeglRectangle *result)
{
  GeglBufferIterator *gi;
  const Babl *fmt_y_u16 = babl_format("Y u16");
#if 1
  // post-process 
  {
    int bpp = babl_format_get_bytes_per_pixel (gegl_buffer_get_format(output));

    if (bpp == 2)
    {
#if 1
    gi = gegl_buffer_iterator_new (output, result, 0, fmt_y_u16,
                                   GEGL_ACCESS_READ|GEGL_ACCESS_WRITE,
                                   GEGL_ABYSS_NONE, 3);
    gegl_buffer_iterator_add (gi, aux, result, 0, fmt_y_u16,
                              GEGL_ACCESS_READ, GEGL_ABYSS_NONE);
    gegl_buffer_iterator_add (gi, input, result, 0, fmt_y_u16,
                              GEGL_ACCESS_READ, GEGL_ABYSS_NONE);
    while (gegl_buffer_iterator_next (gi))
    {
      guint16 *data  = (guint16*) gi->items[0].data;
      guint16 *aux   = (guint16*) gi->items[1].data;
      guint16 *in    = (guint16*) gi->items[2].data;

      GeglRectangle *roi = &gi->items[0].roi;
      int i = 0;
      for (int y = 0; y < roi->height; y++)
      for (int x = 0; x < roi->width; x++, i++)
      {
        int new_delta  = abs(data[i]-in[i]);
        int orig_delta = abs(aux[i]-in[i]);

        int min_neigh_delta = 65535;
        int self = aux[i];
        for (int u = -1; u<=1;u++)
        for (int v = -1; v<=1;v++)
        {
          if (u + x >=0 && u + x <= roi->width &&
              v + y >=0 && v + y <= roi->height)
          {
            int neigh_delta = abs(aux[(y+v) * roi->width + (x+u)] - (int)self);
            if (neigh_delta && neigh_delta < min_neigh_delta)
              min_neigh_delta = neigh_delta;
          }
        }

        int extra_slack = 3 * 256;
        if (min_neigh_delta != 65535 && orig_delta < new_delta - min_neigh_delta - extra_slack)
          data[i] = aux[i];
      }
    }
#endif
    }
    else
    {
      const Babl *fmt_rgb8 = babl_format("R'G'B' u8");
      gi = gegl_buffer_iterator_new (input, result, 0, fmt_rgb8,
                                     GEGL_ACCESS_READ,
                                     GEGL_ABYSS_NONE, 5);

      gegl_buffer_iterator_add (gi, aux, result, 0, NULL,
                                GEGL_ACCESS_READ, GEGL_ABYSS_NONE);
      gegl_buffer_iterator_add (gi, output, result, 0, NULL,
                                GEGL_ACCESS_READ|GEGL_ACCESS_WRITE, GEGL_ABYSS_NONE);

      gegl_buffer_iterator_add (gi, aux, result, 0, fmt_rgb8,
                                GEGL_ACCESS_READ, GEGL_ABYSS_NONE);

      gegl_buffer_iterator_add (gi, output, result, 0, fmt_rgb8,
                                GEGL_ACCESS_READ, GEGL_ABYSS_NONE);

    while (gegl_buffer_iterator_next (gi))
    {
      guint8 *in_rgb   = (guint8*) gi->items[0].data;
      guint8 *aux_raw  = (guint8*) gi->items[1].data;
      guint8 *data_raw = (guint8*) gi->items[2].data;
      guint8 *aux_rgb  = (guint8*) gi->items[3].data;
      guint8 *data_rgb = (guint8*) gi->items[4].data;

      GeglRectangle *roi = &gi->items[0].roi;
      int i = 0;
      for (int y = 0; y < roi->height; y++)
      for (int x = 0; x < roi->width; x++, i++)
      {
        int new_delta  = rgb_diff(&data_rgb[i*3], &in_rgb[i*3]);
        int orig_delta  = rgb_diff(&aux_rgb[i*3], &in_rgb[i*3]);

        int min_neigh_delta = 255;
        for (int v = -1; v<=1;v++)
        for (int u = -1; u<=1;u++)
        {
          if (u + x >=0 && u + x <= roi->width &&
              v + y >=0 && v + y <= roi->height)
          {
            int neigh_delta = rgb_diff(&aux_rgb[((y+v) * roi->width + (x+u))*3], &aux_rgb[i*3]);
            if (neigh_delta && neigh_delta < min_neigh_delta)
              min_neigh_delta = neigh_delta;
          }
        }

        int extra_slack = 3;
 
        if (min_neigh_delta!=255 && orig_delta < new_delta - min_neigh_delta - extra_slack)
          memcpy(&data_raw[i*bpp], &aux_raw[i*bpp], bpp);
      }
    }
    }
  }
#endif
}

static void
improve_rect_1bpp (GeglOperation *operation, GeglBuffer          *input,
              GeglBuffer          *output,
              const GeglRectangle *roi,
              int                  iterations,
              int                  chance)
{
  GeglProperties *o = GEGL_PROPERTIES (operation);
  const Babl *fmt_y_u16 = babl_format("Y u16");
  GeglRectangle ref_rect  = {roi->x-1, roi->y-1, roi->width+3, roi->height+3};
  GeglRectangle bit_rect = {roi->x-2, roi->y-2, roi->width+5, roi->height+5};
  uint16_t *bits = malloc (bit_rect.width*bit_rect.height*2);
  uint16_t *ref  = malloc (ref_rect.width*ref_rect.height*2);
  gegl_buffer_get(output,  &bit_rect, 1.0f, fmt_y_u16, bits, bit_rect.width*2, GEGL_ABYSS_CLAMP);
  gegl_buffer_get(input, &ref_rect, 1.0f, fmt_y_u16, ref, ref_rect.width*2, GEGL_ABYSS_CLAMP);

  int prev_swaps = 1;

  for (int i = 0; i < iterations && prev_swaps; i++)
  {
    int hswaps = 0;
    int vswaps = 0;
    int dswaps = 0;
    int dswap2s = 0;
    int grows = 0;
    int shrinks = 0;

#if 0
  for (int y = 1+(iterations&1); y < roi->height-2; y+=2)
  for (int x = 1+(iterations&1); x < roi->width-2; x+=2)
#else
  for (int y = 0; y < roi->height; y+=1)
  for (int x = 0; x < roi->width; x+=1)
#endif
  if ((gegl_random_int_range(o->rand, x, y, 0, i, 0, 100)) < chance)
  {
    long int score[MUTATE_COUNT] = {0,};
    
    uint8_t backup[16];

#define DO_SWAP(rx,ry,brx,bry)\
    {int tmp = bits[bit_rect.width* (y+2) + x + 2+ bit_rect.width * ry+ rx];\
    bits[bit_rect.width* (y+2) + x + 2+bit_rect.width * ry + rx] = bits[bit_rect.width* (y+2) + x + 2+bit_rect.width * bry + brx];\
    bits[bit_rect.width* (y+2) + x + 2+bit_rect.width * bry + brx] = tmp;}


#define DO_SET(rx,ry,brx,bry) \
    {memcpy(backup, &bits[(bit_rect.width* (y+2) + x + 2+bit_rect.width*(ry)+1*(rx))],2);\
     memcpy(&bits[(bit_rect.width* (y+2) + x + 2+bit_rect.width*(ry)+(rx))], \
            &bits[(bit_rect.width* (y+2) + x + 2+bit_rect.width*bry+brx)], 2);}

#define DO_UNSET(rx,ry)  \
    {memcpy(&bits[(bit_rect.width* (y+2) + x + 2+bit_rect.width*(ry)+1*(rx))], backup, 2);}

#define DO_HSWAP DO_SWAP(0,0,1,0)
#define DO_VSWAP DO_SWAP(0,0,0,1)
#define DO_DSWAP DO_SWAP(0,0,1,1)
#define DO_DSWAP2 DO_SWAP(1,0,0,1)

#define MEASURE(score_name) \
    for (int v = -1; v <= 2; v++) \
    for (int u = -1; u <= 2; u++) \
    { \
      int ref_val = ref[ref_rect.width * ((y+v)+1) + (x + u) + 1];\
      int val = compute_val(o->center_bias, bits+2+(bit_rect.width*2), bit_rect.width, x+u, y+v);\
      score[score_name] += (val-ref_val)*(val-ref_val);\
    }
 
    MEASURE(MUTATE_NONE);

    if (x < roi->width - 2)
    {
      DO_HSWAP;
      MEASURE(MUTATE_HSWAP);
      DO_HSWAP;
    }

    if (y < roi->height -2)
    {
      DO_VSWAP;
      MEASURE(MUTATE_VSWAP);
      DO_VSWAP;
    }

    if (x < roi->width -2 && y < roi->height - 2)
    {
      DO_DSWAP;
      MEASURE(MUTATE_DSWAP);
      DO_DSWAP;

      DO_DSWAP2;
      MEASURE(MUTATE_DSWAP2);
      DO_DSWAP2;
    }
    if (o->grow_shrink && i > 0)// && (gegl_random_int_range(o->rand, x, y, i*10, 0, 100)) < 25)
    {
    DO_SET(1,0,0,0)
    MEASURE(MUTATE_HGROW);
    DO_UNSET(1,0)

    DO_SET(0,1,0,0)
    MEASURE(MUTATE_VGROW);
    DO_UNSET(0,1)

    DO_SET(0,0,1,0)
    MEASURE(MUTATE_HSHRINK);
    DO_UNSET(0,0)

    DO_SET(0,0,0,1)
    MEASURE(MUTATE_VSHRINK);
    DO_UNSET(0,0)

      score[MUTATE_HGROW] *= 2.3;
      score[MUTATE_VGROW] *= 2.3;
      score[MUTATE_HSHRINK] *= 2.3;
      score[MUTATE_VSHRINK] *= 2.3;
    }

    int best = 0;
    for (int j = 0; j < MUTATE_COUNT;j++)
      if (score[j] && score[j] < score[best]) best = j;

    switch(best)
    {
       case MUTATE_NONE: break;
       case MUTATE_HSWAP:
         hswaps++;
         DO_HSWAP;
         break;
       case MUTATE_VSWAP:
         vswaps++;
         DO_VSWAP;
         break;
       case MUTATE_DSWAP:
         dswaps++;
         DO_DSWAP;
         break;
       case MUTATE_DSWAP2:
         dswap2s++;
         DO_DSWAP2;
         break;
       case MUTATE_HGROW:
         grows++;
         DO_SET(1,0,0,0)
         break;
       case MUTATE_VGROW:
         grows++;
         DO_SET(0,1,0,0)
         break;
       case MUTATE_HSHRINK:
         shrinks++;
         DO_SET(0,0,1,0)
         break;
       case MUTATE_VSHRINK:
         shrinks++;
         DO_SET(0,0,0,1)
         break;
    }
  }
#if DEV_MODE
    printf("%i hswap:%i vswap:%i dswap:%i dswap2:%i grow:%i shrink:%i\n", i, hswaps, vswaps, dswaps, dswap2s, grows, shrinks);
#endif

    prev_swaps = hswaps + vswaps + dswaps + dswap2s + grows + shrinks;
  }

  gegl_buffer_set(output, &bit_rect, 0, fmt_y_u16, bits, bit_rect.width * 2);
  free (bits);
  free (ref);

#undef DO_HSWAP
#undef DO_VSWAP
#undef DO_DSWAP
#undef DO_DSWAP2
#undef DO_SWAP
#undef DO_SET
#undef DO_UNSET
#undef MEASURE
}

static void
improve_rect (GeglOperation *operation, GeglBuffer          *input,
              GeglBuffer          *output,
              const GeglRectangle *roi,
              int                  iterations,
              int                  chance)
{
  GeglProperties *o = GEGL_PROPERTIES (operation);
  int bpp = babl_format_get_bytes_per_pixel (gegl_buffer_get_format(output));
  const Babl *fmt_raw = gegl_buffer_get_format(output);

  if (bpp == 2 && babl_get_name(fmt_raw)[0]=='Y')
  {
    improve_rect_1bpp (operation, input, output, roi, iterations, chance);
    return;
  }

  const Babl *fmt_rgb_u8 = babl_format("R'G'B' u8");

  GeglRectangle ref_rect  = {roi->x-1, roi->y-1, roi->width+3, roi->height+3};
  GeglRectangle bit_rect = {roi->x-2, roi->y-2, roi->width+5, roi->height+5};

  uint8_t *bits = malloc (bit_rect.width*bit_rect.height*bpp);
  uint8_t *bits_rgb = malloc (bit_rect.width*bit_rect.height*3);
  uint8_t *ref  = malloc (ref_rect.width*ref_rect.height*3);

  gegl_buffer_get(output,  &bit_rect, 1.0f, fmt_raw, bits, bit_rect.width*bpp, GEGL_ABYSS_CLAMP);
  gegl_buffer_get(output,  &bit_rect, 1.0f, fmt_rgb_u8, bits_rgb, bit_rect.width*3, GEGL_ABYSS_CLAMP);
  gegl_buffer_get(input,   &ref_rect, 1.0f, fmt_rgb_u8, ref, ref_rect.width*3, GEGL_ABYSS_CLAMP);

  int prev_swaps = 1;

  for (int i = 0; i < iterations && prev_swaps; i++)
  {
    int hswaps = 0;
    int vswaps = 0;
    int dswaps = 0;
    int dswap2s = 0;
    int grows = 0;
    int shrinks = 0;

#if 1
  for (int y = 1+(iterations&1); y < roi->height-1; y+=2)
  for (int x = 1+(iterations&1); x < roi->width-1; x+=2)
#else
  for (int y = 1; y < roi->height; y+=1)
  for (int x = 1; x < roi->width; x+=1)
#endif
  if ((gegl_random_int_range(o->rand, x, y, 0, i, 0, 100)) < chance){
    
    int score[MUTATE_COUNT] = {0,};

#define MEASURE(score_name) \
    for (int v = -1; v <= 2; v++)\
    for (int u = -1; u <= 2; u++)\
    {\
      uint8_t computed_rgb[4];\
      int sq_diff = 0;\
      compute_rgb(o->center_bias, bits_rgb + 3*(2+(bit_rect.width)*2), bit_rect.width, x+u, y+v, &computed_rgb[0]);\
      for (int c = 0; c<3; c++)\
      {\
        int val = ref[3*(ref_rect.width * ((y+v)+1) + (x + u) + 1)+c] - computed_rgb[c];\
        sq_diff += val*val;\
      }\
      score[score_name] += sq_diff;\
    }

    uint8_t backup[16];
    uint8_t backup_rgb[16];


#define DO_SWAP(rx,ry,brx,bry) \
    {char tmp[16]; memcpy(tmp, &bits[bpp*(bit_rect.width* (y+2) + x + 2+bit_rect.width*(ry)+1*(rx))], bpp);\
    memcpy(&bits[bpp*(bit_rect.width* (y+2) + x + 2+bit_rect.width*(ry)+(rx))], \
           &bits[bpp*(bit_rect.width* (y+2) + x + 2+bit_rect.width*bry+brx)], bpp);\
    memcpy(&bits[bpp*(bit_rect.width* (y+2) + x + 2+bit_rect.width*bry+brx)], \
           tmp, bpp);\
    memcpy(tmp, &bits_rgb[3*(bit_rect.width* (y+2) + x + 2+bit_rect.width*ry+rx)], 3);\
    memcpy(&bits_rgb[3*(bit_rect.width* (y+2) + x + 2+bit_rect.width*ry+rx)], \
           &bits_rgb[3*(bit_rect.width* (y+2) + x + 2+bit_rect.width*bry+brx)], 3);\
    memcpy(&bits_rgb[3*(bit_rect.width* (y+2) + x + 2+bit_rect.width*bry+brx)],  tmp, 3);}

#define DO_SET(rx,ry,brx,bry) \
    {memcpy(backup, &bits[bpp*(bit_rect.width* (y+2) + x + 2+bit_rect.width*(ry)+1*(rx))], bpp);\
     memcpy(backup_rgb, &bits_rgb[3*(bit_rect.width* (y+2) + x + 2+bit_rect.width*(ry)+1*(rx))], 3);\
     memcpy(&bits[bpp*(bit_rect.width* (y+2) + x + 2+bit_rect.width*(ry)+(rx))], \
            &bits[bpp*(bit_rect.width* (y+2) + x + 2+bit_rect.width*bry+brx)], bpp);\
     memcpy(&bits_rgb[3*(bit_rect.width* (y+2) + x + 2+bit_rect.width*ry+rx)], \
            &bits_rgb[3*(bit_rect.width* (y+2) + x + 2+bit_rect.width*bry+brx)], 3);}

#define DO_UNSET(rx,ry)  \
    {memcpy(&bits[bpp*(bit_rect.width* (y+2) + x + 2+bit_rect.width*(ry)+1*(rx))], backup, bpp);\
     memcpy(&bits_rgb[3*(bit_rect.width* (y+2) + x + 2+bit_rect.width*(ry)+1*(rx))], backup_rgb, 3);}


#define DO_HSWAP DO_SWAP(0,0,1,0)
#define DO_VSWAP DO_SWAP(0,0,0,1)
#define DO_DSWAP DO_SWAP(0,0,1,1)
#define DO_DSWAP2 DO_SWAP(0,1,1,0)

    MEASURE(MUTATE_NONE);

    DO_HSWAP;
    MEASURE(MUTATE_HSWAP);
    DO_HSWAP;

    DO_VSWAP;
    MEASURE(MUTATE_VSWAP);
    DO_VSWAP;
#if 1
    DO_DSWAP;
    MEASURE(MUTATE_DSWAP);
    DO_DSWAP;

    DO_DSWAP2;
    MEASURE(MUTATE_DSWAP2);
    DO_DSWAP2;
#endif

#if 1

    if (i > 1 && (gegl_random_int_range(o->rand, x, y, 3, i, 0, 100)) < 25)
    {
    DO_SET(1,0,0,0)
    MEASURE(MUTATE_HGROW);
    DO_UNSET(1,0)

    DO_SET(0,1,0,0)
    MEASURE(MUTATE_VGROW);
    DO_UNSET(0,1)

    DO_SET(0,0,1,0)
    MEASURE(MUTATE_HSHRINK);
    DO_UNSET(0,0)

    DO_SET(0,0,0,1)
    MEASURE(MUTATE_VSHRINK);
    DO_UNSET(0,0)
    }

#if 0
    // we give a penalty to shrinks and grows, which makes swaps take precedence
    score[MUTATE_HSHRINK] *= 1.4;
    score[MUTATE_VSHRINK] *= 1.4;
    score[MUTATE_HGROW] *= 1.4;
    score[MUTATE_VGROW] *= 1.4;
#endif

#endif

    int best = 0;
    for (int j = 0; j < MUTATE_COUNT;j++)
      if (score[j] && score[j] < score[best]) best = j;

    switch(best)
    {
       case MUTATE_NONE: break;
       case MUTATE_HSWAP:
         hswaps++;
         DO_HSWAP;
         break;
       case MUTATE_VSWAP:
         vswaps++;
         DO_VSWAP;
         break;
       case MUTATE_DSWAP:
         dswaps++;
         DO_DSWAP;
         break;
       case MUTATE_DSWAP2:
         dswap2s++;
         DO_DSWAP2;
         break;
       case MUTATE_HGROW:
         grows++;
         DO_SET(1,0,0,0)
         break;
       case MUTATE_VGROW:
         grows++;
         DO_SET(0,1,0,0)
         break;
       case MUTATE_HSHRINK:
         shrinks++;
         DO_SET(0,0,1,0)
         break;
       case MUTATE_VSHRINK:
         shrinks++;
         DO_SET(0,0,0,1)
         break;
    }
  }
#if DEV_MODE
    printf("%i hswap:%i vswap:%i dswap:%i dswap2:%i grow:%i shrink:%i\n", i, hswaps, vswaps, dswaps, dswap2s, grows, shrinks);
#endif

    prev_swaps = hswaps + vswaps + dswaps + dswap2s + grows + shrinks;
  }

  gegl_buffer_set(output, &bit_rect, 0, fmt_raw, bits, bit_rect.width*bpp);
  free (bits);
  free (bits_rgb);
  free (ref);

#undef DO_HSWAP
#undef DO_VSWAP
#undef DO_DSWAP
#undef DO_DSWAP2
#undef DO_SWAP
#undef DO_SET
#undef DO_UNSET
#undef MEASURE
}


static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *arg_aux,
         GeglBuffer          *output,
         const GeglRectangle *result,
         gint                 level)
{
  GeglBuffer          *aux = arg_aux;
  GeglProperties *o = GEGL_PROPERTIES (operation);
  const Babl *fmt_y_u16 = babl_format("Y' u16");
  GeglBufferIterator *gi;

  if(!aux)
  {
    GeglRectangle bounds = *result;
    bounds.x -= 2;
    bounds.y -= 2;
    bounds.width += 4;
    bounds.height += 4;
    aux = gegl_buffer_new (&bounds, fmt_y_u16);
 
    gi = gegl_buffer_iterator_new (aux, &bounds, 0, fmt_y_u16,
                                   GEGL_ACCESS_READ|GEGL_ACCESS_WRITE,
                                   GEGL_ABYSS_NONE, 2);
    gegl_buffer_iterator_add (gi, input, &bounds, 0, fmt_y_u16,
                              GEGL_ACCESS_READ, GEGL_ABYSS_NONE);

#if 1
//#define dither_mask(u,v,c) (((((u)+ c* 67) ^ (v) * 149) * 1234) & 255)
#define dither_mask(u,v,c)   (((((u)+ c* 67) + (v) * 236) * 119) & 255)
#else
#include "../common/blue-noise-data.inc"
#define dither_mask(u,v,c) blue_noise_data_u8[c][((v) % 256) * 256 + ((u) % 256)]
#endif

  while (gegl_buffer_iterator_next (gi))
  {
      guint16 *data = (guint16*) gi->items[0].data;
      guint16 *in   = (guint16*) gi->items[1].data;
      GeglRectangle *roi = &gi->items[0].roi;
      int i = 0;
      int levels = o->levels - 1;
      int rlevels = 65536/levels;
      for (int y = 0; y < roi->height; y++)
      for (int x = 0; x < roi->width; x++, i++)
       {
          int input = in[i];
#if 0
          mask = ((mask * mask) -32767)/levels;
#else
          int mask = ((powf(dither_mask(roi->x+x, roi->y+y, o->phase)/255.0, o->noise_gamma))*65535-32767)/(levels);
#endif
          int value = input + mask;
          value = ((value + rlevels/2) /rlevels)*(rlevels);
          if (value < 0) data[i] = 0;
          else if (value > 65535) data[i] = 65535;
          else
          data[i] = value;
       }
   }
#if 1
    GeglRectangle set_bound = *result;
    set_bound.width+=3;
    set_bound.height+=3;
    gegl_buffer_copy(aux, &set_bound, GEGL_ABYSS_NONE, output, NULL);
#else
    gegl_buffer_copy(aux, result, GEGL_ABYSS_NONE, output, NULL);
#endif
  }
  else
  {
    gegl_buffer_copy(aux, result, GEGL_ABYSS_NONE, output, NULL);
  }

  improve_rect(operation, input, output, result, o->iterations, o->chance);

  if (o->post_process)
    pp_rect(operation, input, aux, output, result);


  if (aux != arg_aux)
    g_clear_object (&aux);

  return TRUE;
}

static GeglRectangle
get_required_for_output (GeglOperation       *self,
                         const gchar         *input_pad,
                         const GeglRectangle *roi)
{
  const GeglRectangle *in_rect =
          gegl_operation_source_get_bounding_box (self, "input");

  if (in_rect && ! gegl_rectangle_is_infinite_plane (in_rect))
  {
    return *in_rect;
  }
  return *roi;
}

static GeglRectangle
get_cached_region (GeglOperation       *operation,
                   const GeglRectangle *roi)
{
  const GeglRectangle *bounds = gegl_operation_source_get_bounding_box (operation, "input");
  // force chunking to 64x64 aligned blocks
  GeglRectangle rect = *roi;
  
  int dx = (roi->x & 0x2f);

  rect.x -= dx;
  rect.width  += dx;

  dx = (8192 - rect.width) & 0x2f;

  rect.width += dx;


  int dy = (roi->y & 0x2f);
  rect.y -= dy;
  rect.height += dy;

  dy = (8192 - rect.height) & 0x2f;

  rect.height += dy;
#if 1
  if (rect.x + rect.width > bounds->x + bounds->width)
    rect.width = bounds->x + bounds->width - rect.x;
  if (rect.y + rect.height > bounds->y + bounds->height)
    rect.height = bounds->y + bounds->height - rect.y;
#endif
  return rect;
}


static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass         *operation_class;
  GeglOperationComposerClass *composer_class;

  operation_class           = GEGL_OPERATION_CLASS (klass);
  composer_class            = GEGL_OPERATION_COMPOSER_CLASS (klass);
  operation_class->cache_policy = GEGL_CACHE_POLICY_ALWAYS;
  operation_class->threaded = FALSE;
  operation_class->prepare  = prepare;
  operation_class->get_required_for_output = get_required_for_output;
  operation_class->get_cached_region = get_cached_region;
  composer_class->process   = process;

  gegl_operation_class_set_keys (operation_class,
    "name",        "gegl:shuffle-search",
    "title",       _("Optimize Dither"),
    "categories",  "dither",
    "reference-hash", "e9de784b7a9c200bb7652b6b58a4c94a",
    "description", _("Shuffles pixels with neighbors to optimize dither, by shuffling neighboring pixels; if an image is provided as aux input, it is used as dithering starting point."),
    "gimp:menu-path", "<Image>/Colors",
    NULL);
}

#endif
