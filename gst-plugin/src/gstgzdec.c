/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2022 Diego Nieto <diego.nieto.m@outlook.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-gzdec
 *
 * gzdec decompress gzip streams
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! gzdec ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "zlib.h"

#include <gst/gst.h>

#include "gstgzdec.h"

GST_DEBUG_CATEGORY_STATIC (gst_gzdec_debug);
#define GST_CAT_DEFAULT gst_gzdec_debug

// zlib structure to inflate
static z_stream strm;


/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

#define gst_gzdec_parent_class parent_class
G_DEFINE_TYPE (Gstgzdec, gst_gzdec, GST_TYPE_ELEMENT);

GST_ELEMENT_REGISTER_DEFINE (gzdec, "gzdec", GST_RANK_NONE,
    GST_TYPE_GZDEC);

static void gst_gzdec_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_gzdec_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_gzdec_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static GstFlowReturn gst_gzdec_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);

/* GObject vmethod implementations */

/* initialize the gzdec's class */
static void
gst_gzdec_class_init (GstgzdecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_gzdec_set_property;
  gobject_class->get_property = gst_gzdec_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  gst_element_class_set_details_simple (gstelement_class,
      "gzdec",
      "Plugin to decompress gzip files",
      "Plugin to decompress gzip files", "Diego Nieto <diego.nieto.m@outlook.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_gzdec_init (Gstgzdec * filter)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_gzdec_sink_event));
  gst_pad_set_chain_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_gzdec_chain));
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->silent = FALSE;
  filter->initialized = FALSE;
}

static void
gst_gzdec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstgzdec *filter = GST_GZDEC (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gzdec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstgzdec *filter = GST_GZDEC (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_gzdec_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  Gstgzdec *filter;
  gboolean ret;

  filter = GST_GZDEC (parent);

  GST_LOG_OBJECT (filter, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_START:
    {
      GST_DEBUG("GST_EVENT_STREAM_START\n");
      strm.zalloc = Z_NULL;
      strm.zfree = Z_NULL;
      strm.opaque = Z_NULL;
      strm.avail_in = 0;
      strm.next_in = Z_NULL;
      // 15 zlib fomat, 32 zlib and gzip format, 16 gzip format
      ret = inflateInit2(&strm, 32);
      if (ret == Z_OK) {
        filter->initialized = TRUE;
      } else {
        GST_WARNING("Error when initializing the zlib\n");
      }
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    case GST_EVENT_EOS:
    {
      GST_DEBUG("GST_EVENT_EOS\n");
      /* clean up and return */
      (void)inflateEnd(&strm);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      /* do something with the caps */

      /* and forward */
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}


GstBuffer *decompress(GstBuffer *inputBuffer)
{
  // Default ChunkSize
  const unsigned long long int ChunkSize = 16384;

  GstBuffer *outputBuffer = gst_buffer_new();
  if (!outputBuffer) {
    return NULL;
  }
  GstMemory *memory = NULL;

  // Intermediate buffer
  guint8 *inputData;
  gsize inputDataSize;
  guint8 *outputData;

  // Mapping structures
  GstMapInfo map_in;
  GstMapInfo map_out;

  if (gst_buffer_map (inputBuffer, &map_in, GST_MAP_READ)) {
    memory = gst_allocator_alloc(NULL, ChunkSize, NULL);

    if (!gst_memory_map (memory, &map_out, GST_MAP_WRITE)) {
      GST_ELEMENT_ERROR (GST_ELEMENT (NULL), STREAM, FAILED, (NULL), (NULL));
      return NULL;
    }
  } else {
    GST_ELEMENT_ERROR (GST_ELEMENT (NULL), STREAM, FAILED, (NULL), (NULL));
    return NULL;
  }

  // Initialize mapping
  inputData = map_in.data;
  inputDataSize = map_in.size;
  outputData = map_out.data;

  // Error handler for zlib
  int ret;
  // Available data in the ouput buffer
  unsigned have;

  GST_DEBUG("RAW input data size: %lu\n", inputDataSize);
  strm.avail_in = inputDataSize;
  if (strm.avail_in == 0) {
      return NULL;
  }
  strm.next_in = inputData;

  /* run inflate() on input until output buffer not full */
  do {
      strm.avail_out = ChunkSize;
      strm.next_out = outputData;
      ret = inflate(&strm, Z_NO_FLUSH);
      switch (ret) {
      case Z_NEED_DICT:
          ret = Z_DATA_ERROR;     /* and fall through */
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
          (void)inflateEnd(&strm);
          return NULL;
      }
      have = ChunkSize - strm.avail_out;
      GST_DEBUG("Decompressed size %d\n", have);


      // Last chunk
      if (have <= ChunkSize) {
        // Allocate more data for the next output buffer
        GstMemory *lastChunkMemory = gst_allocator_alloc(NULL, have, NULL);
        lastChunkMemory = gst_memory_copy(memory, 0, have);
        gst_buffer_insert_memory(outputBuffer, -1, lastChunkMemory);
      } else {
        // Release memory by itselft since that memory it is
        // not inserted into the buffer
        gst_memory_unmap(memory, &map_out);
        // Append to the buffer current memory data
        gst_buffer_insert_memory(outputBuffer, -1, memory);
        gst_memory_unref(memory);
        // Allocate more data for the next output buffer
        memory = gst_allocator_alloc(NULL, ChunkSize, NULL);
        if (!gst_memory_map(memory, &map_out, GST_MAP_WRITE)) {
          return NULL;
        }
        outputData = map_out.data;
      }
  } while (strm.avail_out == 0);

  // Clean up the input
  gst_buffer_unmap (inputBuffer, &map_in);

  return outputBuffer;
}


/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_gzdec_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  Gstgzdec *filter;
  GstBuffer *outbuf = NULL;

  filter = GST_GZDEC (parent);
  if (filter->initialized == FALSE) {
    GST_ERROR("Processing is not possible. Decoder it is not initialized");
    return GST_FLOW_ERROR;
  }

  outbuf = decompress(buf);

  gst_buffer_unref(buf);
  if (!outbuf) {
    GST_ERROR("Error when inflating the data in the pipeline");
    /* something went wrong - signal an error */
    GST_ELEMENT_ERROR (GST_ELEMENT (filter), STREAM, FAILED, (NULL), (NULL));
    return GST_FLOW_ERROR;
  }

  return gst_pad_push (filter->srcpad, outbuf);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
gzdec_init (GstPlugin * gzdec)
{
  /* debug category for filtering log messages
   *
   * exchange the string 'Template gzdec' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_gzdec_debug, "gzdec",
      0, "Debug for gzdec");

  return GST_ELEMENT_REGISTER (gzdec, gzdec);
}

/* PACKAGE: this is usually set by meson depending on some _INIT macro
 * in meson.build and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use meson to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstgzdec"
#endif

/* gstreamer looks for this structure to register gzdecs
 *
 * exchange the string 'Template gzdec' with your gzdec description
 */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    gzdec,
    "gzdec",
    gzdec_init,
    PACKAGE_VERSION, "LGPL", "GStreamer template Plug-ins", "https://gstreamer.freedesktop.org")
