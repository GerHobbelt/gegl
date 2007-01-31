/* This file is the public GEGL API
 *
 * GEGL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * GEGL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * 2000-2007 © Calvin Williamson, Øyvind Kolås.
 */

#ifndef __GEGL_H__
#define __GEGL_H__

#include <glib-object.h>


/***
 * The GEGL API:
 *
 * This document is both a tutorial and the reference for the C API of GEGL.
 * The concepts covered in the tutotrial should be applicable using other
 * languages as well.
 */

/***
 * Initialization:
 *
 * Before GEGL can be used the engine should be initialized by either calling
 * #gegl_init or through the use of #gegl_get_option_group. To shut down the
 * GEGL engine call #gegl_exit.
 *
 * ---Code sample:
 * #include <gegl.h>
 *
 * int main(int argc, char **argv)
 * {
 *   gegl_init (&argc, &argv);
 *       # other GEGL code
 *   gegl_exit ();
 * }
 */

/**
 * gegl_init:
 * @argc: a pointer to the number of command line arguments.
 * @argv: a pointer to the array of command line arguments.
 *
 * Call this function before using any other GEGL functions. It will
 * initialize everything needed to operate GEGL and parses some
 * standard command line options.  @argc and @argv are adjusted
 * accordingly so your own code will never see those standard
 * arguments.
 *
 * Note that there is an alternative way to initialize GEGL: if you
 * are calling g_option_context_parse() with the option group returned
 * by #gegl_get_option_group(), you don't have to call #gegl_init().
 **/
void           gegl_init                 (gint          *argc,
                                          gchar       ***argv);
/**
 * gegl_get_option_group:
 *
 * Returns a GOptionGroup for the commandline arguments recognized
 * by GEGL. You should add this group to your GOptionContext
 * with g_option_context_add_group() if you are using
 * g_option_context_parse() to parse your commandline arguments.
 */
GOptionGroup * gegl_get_option_group     (void);

/**
 * gegl_exit:
 *
 * Call this function when you're done using GEGL. It will clean up
 * caches and write/dump debug information if the correct debug flags
 * are set.
 */
void           gegl_exit                 (void);


/***
 * GeglNode:
 *
 * The Node is the image processing primitive connected to create compositions
 * in GEGL. The toplevel #GeglNode which contains a graph of #GeglNodes is
 * created with #gegl_node_new. Using this toplevel node we can create children
 * of this node which are individual processing elements using #gegl_node_new_child
 *
 * A node either has an associated operation or is a parent for other nodes,
 * that are connected to their parent through proxies created with
 * #gegl_node_get_input_proxy and #gegl_node_get_output_proxy.
 *
 * The properties available on a node depends on which <a
 * href='operations.html'>operation</a> is specified.
 *
 * ---
 * GeglNode *gegl, *load, *bcontrast;
 *
 * gegl = gegl_node_new ();
 * load = gegl_node_new_child (gegl,
 *                             "operation", "load",
 *                             "path",      "input.png",
 *                             NULL);
 * save = gegl_node_new_child (gegl,
 *                             "operation", "brightness-contrast",
 *                             "brightness", 0.2,
 *                             "contrast",   1.5,
 *                             NULL);
 */
#ifndef GEGL_INTERNAL /* These declarations duplicate internal ones in GEGL */
typedef struct _GeglNode      GeglNode;
GType gegl_node_get_type  (void) G_GNUC_CONST;
#define GEGL_TYPE_NODE  (gegl_node_get_type())
#define GEGL_NODE(obj)  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GEGL_TYPE_NODE, GeglNode))

typedef struct _GeglRectangle GeglRectangle;
GType gegl_rectangle_get_type  (void) G_GNUC_CONST;
#define GEGL_TYPE_RECTANGLE  (gegl_rectangle_get_type())
#endif

/**
 * gegl_node_new:
 *
 * Create a new graph that can contain further processing nodes.
 *
 * Returns a new top level #GeglNode (which can be used as a graph). When you
 * are done using this graph instance it should be unreferenced with g_object_unref.
 * This will also free any sub nodes created from this node.
 */
GeglNode     * gegl_node_new             (void);

/**
 * gegl_node_new_child:
 * @parent: a #GeglNode
 * @first_property_name: the first property name, should usually be "operation"
 * @...: first property value, optionally followed by more key/value pairs, ended
 * terminated with NULL.
 *
 * Creates a new processing node that performs the specified operation with
 * a NULL terminated list of key/value pairs for initial parameter values
 * configuring the operation.
 *
 * Returns a newly created node, the node will be destroyed by the parent.
 * Calling g_object_unref on a node will cause the node to be dropped by the
 * parent. (You may also add additional references using
 * g_object_ref/g_objecr_unref, but in general relying on the parents reference
 * counting is easiest.)
 */
GeglNode    * gegl_node_new_child        (GeglNode      *parent,
                                          const gchar   *first_property_name,
                                          ...) G_GNUC_NULL_TERMINATED;

/***
 * Making connections:
 *
 * Nodes in GEGL are connected to each other, the resulting graph of nodes
 * represents the image processing pipeline to be processed.
 *
 * ---
 * gegl_node_link_many (background, over, png_save, NULL);
 * gegl_node_connect_to (translate, "output", over, "aux");
 * gegl_node_link_many (text, blur, translate);
 */

/**
 * gegl_node_connect_from:
 * @sink: the node we're connecting an input to
 * @input_pad_name: the name of the input pad we are connecting to
 * @source: the node producing data we want to connect.
 * @output_pad_name: the output pad we want to use on the source.
 *
 * Makes a connection between the pads of two nodes.
 *
 * Returns TRUE if the connection was succesfully made.
 */

gboolean      gegl_node_connect_from     (GeglNode      *sink,
                                          const gchar   *input_pad_name,
                                          GeglNode      *source,
                                          const gchar   *output_pad_name);

/**
 * gegl_node_connect_to:
 * @source: the node producing data we want to connect.
 * @output_pad_name: the output pad we want to use on the source.
 * @sink: the node we're connecting an input to
 * @input_pad_name: the name of the input pad we are connecting to
 *
 * Makes a connection between the pads of two nodes.
 *
 * Returns TRUE if the connection was succesfully made.
 */
gboolean      gegl_node_connect_to       (GeglNode      *source,
                                          const gchar   *output_pad_name,
                                          GeglNode      *sink,
                                          const gchar   *input_pad_name);


/**
 * gegl_node_link:
 * @source: the producer of data.
 * @sink: the consumer of data.
 *
 * Synthetic sugar for linking the "output" pad of @source to the "input"
 * pad of @sink.
 */
void          gegl_node_link             (GeglNode      *source,
                                          GeglNode      *sink);

/**
 * gegl_node_link_many:
 * @source: the producer of data.
 * @first_sink: the first consumer of data.
 * @...: NULL, or optionally more consumers followed by NULL.
 *
 * Synthetic sugar for linking a chain of nodes with "input"->"output", the
 * list is NULL terminated.
 */
void          gegl_node_link_many        (GeglNode      *source,
                                          GeglNode      *first_sink,
                                          ...) G_GNUC_NULL_TERMINATED;

/**
 * gegl_node_disconnect:
 * @node: a #GeglNode
 * @input_pad: the input pad to disconnect.
 *
 * Disconnects node connected to @input_pad of @node (if any).
 *
 * Returns TRUE if a connection was broken.
 */
gboolean      gegl_node_disconnect       (GeglNode      *node,
                                          const gchar   *input_pad);

/***
 * Setting properties:
 *
 * Properties can be set either when creating the node with
 * #gegl_node_new_child as well as later when changing the initial
 * value with #gegl_node_set.
 *
 * To see what operations are available for a given operation look in the <a
 * href='operations.html'>Operations reference</a> or use
 * #gegl_node_get_properties.
 */

/**
 * gegl_node_set:
 * @node: a #GeglNode
 * @first_property_name: name of the first property to set
 * @...: value for the first property, followed optionally by more name/value
 * pairs, followed by NULL.
 *
 * Set properties on a node, possible properties to be set are the properties
 * of the currently set operations as well as <em>"name"</em> and
 * <em>"operation"</em>. <em>"operation"</em> changes the current operations
 * set for the node, <em>"name"</em> doesn't have any role internally in
 * GEGL.
 * ---
 * gegl_node_set (node, "brightness", -0.2,
 *                      "contrast",   2.0,
 *                      NULL);
 */
void          gegl_node_set              (GeglNode      *node,
                                          const gchar   *first_property_name,
                                          ...) G_GNUC_NULL_TERMINATED;


/***
 * Processing:
 *
 * There are two different ways to do processing with GEGL, either you
 * query any node providing output for a rectangular region to be rendered
 * using #gegl_node_blit, or you use #gegl_node_process on a sink node (A
 * display node, an image file writer or similar). To do iterative processing
 * you need to use a #GeglProcessor see #gegl_processor_work for a code sample.
 */

#ifndef GEGL_INTERNAL
typedef enum
{
  GEGL_BLIT_DEFAULT  = 0,
  GEGL_BLIT_CACHE    = 1 << 0,
  GEGL_BLIT_DIRTY    = 1 << 1,
} GeglBlitFlags;
#endif

/**
 * gegl_node_blit:
 * @node: a #GeglNode
 * @roi: the upper left coordinates to render, and the width/height of the
 * destination buffer.
 * @scale: the scale to render at 1.0 is default, other values changes the
 * width/height of the sampled region.
 * @format: the #BablFormat desired.
 * @rowstride: rowstride in bytes (currently ignored)
 * @destination_buf: a memory buffer large enough to contain the data.
 * @flags: an or'ed combination of GEGL_BLIT_DEFAULT, GEGL_BLIT_CACHE and
 * GEGL_BLIT_DIRTY. if cache is enabled, a cache will be set up for subsequent
 * requests of image data from this node. By passing in GEGL_BLIT_DIRTY the
 * function will return with the latest rendered results in the cache without
 * regard to wheter the regions has been rendered or not.
 *
 * Render a rectangular region from a node.
 */
void          gegl_node_blit             (GeglNode      *node,
                                          GeglRectangle *roi,
                                          gdouble        scale,
                                          void          *format,
                                          gint           rowstride,
                                          gpointer      *destination_buf,
                                          GeglBlitFlags  flags);

/**
 * gegl_node_process:
 * @sink_node: a #GeglNode without outputs.
 *
 * Render a composition. This can be used for instance on a node with a "png-save"
 * operation to render all neccesary data, and make it be written to file, the
 * function is blocking for a non blocking way of doing the same see #GeglProcessor.
 * ---
 * GeglNode      *gegl;
 * GeglRectangle  roi;
 * GeglNode      *png_save;
 * unsigned char *buffer;
 *
 * gegl = gegl_parse_xml (xml_data);
 * roi      = gegl_node_get_bounding_box (gegl);
 * png_save = gegl_node_new_child (gegl,
 *                                 "operation", "png-save",
 *                                 "path",      "output.png",
 *                                 NULL);
 *
 * gegl_node_link (gegl, png_save);
 * gegl_node_process (png_save);
 *
 * buffer = malloc (roi.w*roi.h*4);
 * gegl_node_blit (gegl,
 *                 &roi,
 *                 1.0,
 *                 babl_format("R'G'B'A u8",
 *                 roi.w*4,
 *                 buffer,
 *                 GEGL_BLIT_DEFAULT);
 */
void          gegl_node_process          (GeglNode      *sink_node);


/**
 * GeglProcessor:
 *
 * A #GeglProcessor, is a worker that can be used for background rendering
 * of regions in a node's cache. Or for processing a sink node. For most
 * non GUI tasks using #gegl_node_blit and #gegl_node_process directly
 * should be sufficient. See #gegl_processor_work for a code sample.
 *
 */
#ifndef GEGL_INTERNAL
GType gegl_processor_get_type  (void) G_GNUC_CONST;
typedef struct _GeglProcessor      GeglProcessor;
#define GEGL_TYPE_PROCESSOR  (gegl_processor_get_type())
#define GEGL_PROCESSOR(obj)  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GEGL_TYPE_PROCESSOR, GeglProcessor))
#endif

/**
 * gegl_node_new_processor:
 * @node: a #GeglNode
 * @rectangle: the #GeglRectangle to work on or NULL to work on all available
 * data.
 *
 * Returns a new #GeglProcessor.
 */
GeglProcessor *gegl_node_new_processor      (GeglNode      *node,
                                             GeglRectangle *rectangle);

/**
 * gegl_processor_set_rectangle:
 * @processor: a #GeglProcessor
 * @rectangle: the new #GeglRectangle the processor shold work on or NULL
 * to make it work on all data in the buffer.
 *
 * Change the rectangle a #GeglProcessor is working on.
 */
void           gegl_processor_set_rectangle (GeglProcessor *processor,
                                             GeglRectangle *rectangle);


/**
 * gegl_processor_work:
 * @processor: a #GeglProcessor
 * @progress: a location to store the (estimated) percentage complete.
 *
 * Do an iteration of work for the processor.
 *
 * Returns TRUE if there is more work to be done.
 *
 * ---
 * GeglProcessor *processor = gegl_node_new_processor (node, &roi);
 * double         progress;
 *
 * while (gegl_processor_work (processor, &progress))
 *   g_warning ("%f%% complete", progress);
 * gegl_processor_destroy (processor);
 */
gboolean       gegl_processor_work          (GeglProcessor *processor,
                                             gdouble       *progress);


/**
 * gegl_processor_destroy:
 * @processor: a #GeglProcessor
 *
 * Frees up resources used by a processing handle.
 */
void           gegl_processor_destroy       (GeglProcessor *processor);


/***
 * State queries:
 *
 * This section lists functions that retrieve information, mostly needed
 * for interacting with a graph in a GUI, not creating one from scratch.
 *
 * You can figure out what the bounding box of a node is with #gegl_node_get_bounding_box,
 * retrieve the values of named properties using #gegl_node_get.
 */

/**
 * gegl_list_operations:
 *
 * Returns a list of available operations names. The list should not be freed.
 */
GSList      * gegl_list_operations (void);

/**
 * gegl_node_detect:
 * @node: a #GeglNode
 * @x: x coordinate
 * @y: y coordinate
 *
 * Performs hit detection by returning the node providing data at a given
 * coordinate pair. Currently operates only on bounding boxes and not
 * pixel data.
 *
 * Returns the GeglNode providing the data ending up at @x,@y in the output
 * of @node.
 */
GeglNode    * gegl_node_detect           (GeglNode      *node,
                                          gint           x,
                                          gint           y);


/**
 * gegl_node_find_property:
 * @node: the node to lookup a paramspec on
 * @property_name: the name of the property to get a paramspec for.
 *
 * Returns the GParamSpec of property or NULL if no such property exists.
 */
GParamSpec  * gegl_node_find_property    (GeglNode      *node,
                                          const gchar   *property_name);


/**
 * gegl_node_get:
 * @node: a #GeglNode
 * @first_property_name: name of the first property to get.
 * @...: return location for the first property, followed optionally by more
 * name/value pairs, followed by NULL.
 *
 * Gets properties of a #GeglNode.
 * ---
 * double level;
 * char  *path;
 *
 * gegl_node_get (png_save, "path", &path, NULL);
 * gegl_node_get (threshold, "level", &level, NULL);
 */
void          gegl_node_get              (GeglNode      *node,
                                          const gchar   *first_property_name,
                                          ...) G_GNUC_NULL_TERMINATED;

/**
 * gegl_node_get_bounding_box:
 * @node: a #GeglNode
 *
 * Returns the position and dimensions of a rectangle spanning the area
 * defined by a node.
 */
GeglRectangle gegl_node_get_bounding_box (GeglNode      *node);

/**
 * gegl_node_get_children:
 * @node: the node to retrieve the children of.
 *
 * Returns a list of nodes with children/internal nodes. The list must be
 * freed by the caller.
 */
GSList      * gegl_node_get_children     (GeglNode      *node);

/**
 * gegl_node_get_consumers:
 * @node: the node we are querying.
 * @output_pad: the output pad we want to know who uses.
 * @nodes: optional return location for array of nodes.
 * @pads: optional return location for array of pad names.
 *
 * Retrieve which pads on which nodes are connected to a named output_pad,
 * and the number of connections. Both the location for the generated
 * nodes array and pads array can be left as NULL. If they are non NULL
 * both should be freed with g_free. The arrays are NULL terminated.
 *
 * Returns the number of consumers connected to this output_pad.
 */
gint          gegl_node_get_consumers    (GeglNode      *node,
                                          const gchar   *output_pad,
                                          GeglNode    ***nodes,
                                          const gchar ***pads);

/**
 * gegl_node_get_input_proxy:
 * @node: a #GeglNode
 * @pad_name: the name of the pad.
 *
 * Proxies are used to route between nodes of a subgraph contained within
 * a node.
 *
 * Returns an input proxy for the named pad. If no input proxy exists with
 * this name a new one will be created.
 */
GeglNode    * gegl_node_get_input_proxy  (GeglNode      *node,
                                          const gchar   *pad_name);

/**
 * gegl_node_get_operation:
 * @node: a #GeglNode
 *
 * Returns the type of processing operation associated with this node, or
 * an empty string if there is no op associated. The special name "GraphNode"
 * is returned if the node is the container of a subgraph.
 */
const gchar * gegl_node_get_operation    (GeglNode      *node);

/**
 * gegl_node_get_output_proxy:
 * @node: a #GeglNode
 * @pad_name: the name of the pad.
 *
 * Proxies are used to route between nodes of a subgraph contained within
 * a node.
 *
 * Returns a output proxy for the named pad. If no output proxy exists with
 * this name a new one will be created.
 */
GeglNode    * gegl_node_get_output_proxy (GeglNode      *node,
                                          const gchar   *pad_name);

/**
 * gegl_node_get_producer:
 * @node: the node we are querying
 * @input_pad_name: the input pad we want to get the producer for
 * @output_pad_name: optional pointer to a location where we can store a
 *                   freshly allocated string with the name of the output pad.
 *
 * Returns the node providing data or NULL if no node is connected to the
 * input_pad.
 */
GeglNode    * gegl_node_get_producer     (GeglNode      *node,
                                          gchar         *input_pad_name,
                                          gchar        **output_pad_name);

/**
 * gegl_node_get_properties:
 * @node: a #GeglNode
 * @n_properties: return location for number of properties.
 *
 * Returns an allocated array of #GParamSpecs describing the properties
 * of the operation currently set for a node.
 */
GParamSpec ** gegl_node_get_properties   (GeglNode      *node,
                                          guint         *n_properties);





/***
 * XML:
 * The XML format used by GEGL is not stable and should not be relied on
 * for anything but testing purposes yet.
 */

/**
 * gegl_parse_xml:
 * @xmldata: a \0 terminated string containing XML data to be parsed.
 * @path_root: a file system path that relative paths in the XML will be
 * resolved in relation to.
 *
 * The #GeglNode returned contains the graph described by the tree of stacks
 * in the XML document, the tree is connected to the "output" pad of the
 * returned node and thus can be used directly for processing.
 *
 * Returns a GeglNode containing the parsed XML as a subgraph.
 */
GeglNode    * gegl_parse_xml             (const gchar   *xmldata,
                                          const gchar   *path_root);

/**
 * gegl_to_xml:
 * @node: a #GeglNode
 * @path_root: filesystem path to construct relative paths from.
 *
 * Returns a freshly allocated \0 terminated string containing a XML
 * serialization of the composition produced by a node (and thus also
 * the nodes contributing data to the specified node). To export a
 * gegl graph, connect the internal output node to an output proxy(see
 * #gegl_node_get_output_proxy.
 */
gchar       * gegl_to_xml                (GeglNode      *node,
                                          const gchar   *path_root);

#ifndef GEGL_INTERNAL

/***
 * GeglRectangle:
 *
 * GeglRectangles are used in #gegl_node_get_bounding_box and #gegl_node_blit
 * for specifying rectangles.
 *
 * </p><pre>struct GeglRectangle
 * {
 *   gint x;
 *   gint y;
 *   gint w;
 *   gint h;
 * };</pre><p>
 *
 */
struct _GeglRectangle
{
  gint x;
  gint y;
  gint w;
  gint h;
};

/***
 * GeglColor:
 *
 * GeglColor is used for properties that use a gegl color, use #gegl_color_new
 * with a NULL string to create a new blank one, gegl_colors are destroyed
 * with g_object_unref when they no longer are needed.
 *
 * The colors used by gegls are described in a format similar to CSS, the
 * textstring "rgb(1.0,1.0,1.0)" signifies opaque white and
 * "rgba(1.0,0.0,0.0,0.75)" is a 75% opaque red. Hexadecimal forms like #RRGGBB
 * and #RRGGBBAA are also supported.
 */
typedef struct _GeglColor     GeglColor;
GType gegl_color_get_type (void) G_GNUC_CONST;
#define GEGL_TYPE_COLOR (gegl_color_get_type())
#endif

/**
 * gegl_color_new:
 * @string: an CSS style color string.
 *
 * Returns a #GeglColor object suitable for use with #gegl_node_set.
 */
GeglColor   * gegl_color_new             (const gchar   *string);

/**
 * gegl_color_get_rgba:
 * @color: a #GeglColor
 * @r: return location for red component.
 * @g: return location for green component.
 * @b: return location for blue component.
 * @a: return location for alpha component.
 *
 * Retrieve RGB component values from a #GeglColor.
 */
void          gegl_color_get_rgba        (GeglColor     *color,
                                          gfloat        *r,
                                          gfloat        *g,
                                          gfloat        *b,
                                          gfloat        *a);

/**
 * gegl_color_set_rgba:
 * @color: a #GeglColor
 * @r: new value for red component
 * @g: new value for green component
 * @b: new value for blue component
 * @a: new value for alpha component
 *
 * Retrieve RGB component values from a GeglColor.
 */
void          gegl_color_set_rgba        (GeglColor     *color,
                                          gfloat         r,
                                          gfloat         g,
                                          gfloat         b,
                                          gfloat         a);

/***
 * Bindings conveniences:
 *
 * The following functions are mostly included to make it easier
 * to create language bindings. The varargs versions most often
 * lead to more readable C code.
 */

/**
 * gegl_node_create_child:
 * @parent: a #GeglNode
 * @operation: the type of node to create.
 *
 * Creates a new processing node that performs the specified operation.
 *
 * Returns a newly created node, the node will be destroyed by the parent.
 * Calling g_object_unref on a node will cause the node to be dropped by the
 * parent. (You may also add additional references using
 * g_object_ref/g_objecr_unref, but in general relying on the parents reference
 * counting is easiest.)
 */

GeglNode     * gegl_node_create_child    (GeglNode      *parent,
                                          const gchar   *operation);


/**
 * gegl_node_get_property:
 * @node: the node to get a property from
 * @property_name: the name of the property to get
 * @value: pointer to a GValue where the value of the property should be stored
 *
 * This is mainly included for language bindings. Using #gegl_node_get is
 * more convenient when programming in C.
 *
 */
void          gegl_node_get_property     (GeglNode      *node,
                                          const gchar   *property_name,
                                          GValue        *value);

/**
 * gegl_node_set_property:
 * @node: a #GeglNode
 * @property_name: the name of the property to set
 * @value: a GValue containing the value to be set in the property.
 *
 * This is mainly included for language bindings. Using #gegl_node_set is
 * more convenient when programming in C.
 */
void          gegl_node_set_property     (GeglNode      *node,
                                          const gchar   *property_name,
                                          const GValue  *value);

/*** this is just here to trick the parser.
 */
#include "gegl/gegl-paramspecs.h"
#include <babl/babl.h>

#endif  /* __GEGL_H__ */
