/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2022 Diego Nieto <<user@hostname.org>>
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
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/controller/gstcontroller.h>

#include "gstgzdec.h"

#include "zlib.h"

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
  PROP_SILENT,
};

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("ANY")
);

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE (
  "src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("ANY")
);

/* debug category for fltering log messages
 *
 * FIXME:exchange the string 'Template gzdec' with your description
 */
#define DEBUG_INIT(bla) \
  GST_DEBUG_CATEGORY_INIT (gst_gzdec_debug, "gzdec", 0, "Template gzdec");

GST_BOILERPLATE_FULL (Gstgzdec, gst_gzdec, GstBaseTransform,
    GST_TYPE_BASE_TRANSFORM, DEBUG_INIT);

static void gst_gzdec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gzdec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_gzdec_transform (GstBaseTransform * bt,
    GstBuffer * outbuf, GstBuffer * inbuf);
static GstFlowReturn gst_gzdec_transform_ip (GstBaseTransform * base,
    GstBuffer * outbuf);

/* GObject vmethod implementations */

static void
gst_gzdec_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_details_simple (element_class,
    "gzdec",
    "Generic/Filter",
    "FIXME:Generic Template Filter",
    "Diego Nieto <<user@hostname.org>>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));
}

/* initialize the gzdec's class */
static void
gst_gzdec_class_init (GstgzdecClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;
  gobject_class->set_property = gst_gzdec_set_property;
  gobject_class->get_property = gst_gzdec_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
    g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE));

  GST_BASE_TRANSFORM_CLASS (klass)->transform_ip =
      GST_DEBUG_FUNCPTR (gst_gzdec_transform_ip);

  GST_BASE_TRANSFORM_CLASS (klass)->transform =
      GST_DEBUG_FUNCPTR (gst_gzdec_transform);
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_gzdec_init (Gstgzdec *filter, GstgzdecClass * klass)
{
  filter->silent = FALSE;

  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;
  // 15 zlib fomat, 32 zlib and gzip format, 16 gzip format
  int ret = inflateInit2(&strm, 32);
  if (ret == Z_OK) {
    filter->initialized = TRUE;
  } else {
    GST_WARNING("Error when initializing the zlib\n");
  }
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

/* GstBaseTransform vmethod implementations */

gboolean decompress(GstBuffer *inputBuffer, GstBuffer *outputBuffer)
{
  // Default ChunkSize
  const unsigned long long int ChunkSize = 16384;

  if (!outputBuffer) {
    return FALSE;
  }
  GstBuffer *memory = gst_buffer_new_and_alloc(ChunkSize);

  // Intermediate buffer
  guint8 *inputData;
  gsize inputDataSize;
  guint8 *outputData;

  // Initialize mapping
  inputData = GST_BUFFER_DATA(inputBuffer);
  inputDataSize = GST_BUFFER_SIZE(inputBuffer);
  outputData = GST_BUFFER_DATA(memory);

  int j;
  for (j=0; j<ChunkSize; j++)
  {
    outputData[j] = 1;
  }

  char buffer[ChunkSize];
  outputData = buffer;

  // Error handler for zlib
  int ret;
  // Available data in the ouput buffer
  unsigned have;

  GST_DEBUG("RAW input data: %s\n", inputData);

  GST_DEBUG("RAW input data size: %lu\n", inputDataSize);
  strm.avail_in = inputDataSize;
  if (strm.avail_in == 0) {
      return FALSE;
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
          GST_WARNING("Error when inflating the data\n");
          return FALSE;
      }
      have = ChunkSize - strm.avail_out;
      GST_DEBUG("Decompressed size %d\n", have);

      // Last chunk
      if (have <= ChunkSize) {
        // Allocate more data for the next output buffer
        GstBuffer *lastChunkMemory = gst_buffer_new_and_alloc(have);
        memcpy(GST_BUFFER_DATA(lastChunkMemory), 
               GST_BUFFER_DATA(memory),
               have);
        outputBuffer = gst_buffer_join(outputBuffer, lastChunkMemory);
        //gst_buffer_insert_memory(outputBuffer, -1, memory);
      } else {
        // Append to the buffer current memory data
        //gst_buffer_insert_memory(outputBuffer, -1, memory);
        outputBuffer = gst_buffer_join(outputBuffer, memory);
        // Allocate more data for the next output buffer
        memory = gst_buffer_new_and_alloc(ChunkSize);
        outputData = GST_BUFFER_DATA(memory);
      }
  } while (strm.avail_out == 0);

  return TRUE;
}


static GstFlowReturn 
gst_gzdec_transform (GstBaseTransform * bt, GstBuffer * outbuf, GstBuffer * inbuf)
{
  Gstgzdec *filter = GST_GZDEC (bt);

  if (filter->initialized == TRUE) {
    if (decompress(inbuf, outbuf) == TRUE) {
      return GST_FLOW_OK;
    } else {
      GST_WARNING("Error processing the input\n");
      return GST_FLOW_ERROR;
    }
  } else {
    GST_WARNING("Filter not initialized\n");
    return GST_FLOW_ERROR;
  }
}

/* this function does the actual processing
 */
static GstFlowReturn
gst_gzdec_transform_ip (GstBaseTransform * base, GstBuffer * outbuf)
{
  return GST_FLOW_OK;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
gzdec_init (GstPlugin * gzdec)
{
  /* initialize gst controller library */
  gst_controller_init(NULL, NULL);

  return gst_element_register (gzdec, "gzdec", GST_RANK_NONE,
      GST_TYPE_GZDEC);
}

/* gstreamer looks for this structure to register gzdecs
 *
 * FIXME:exchange the string 'Template gzdec' with you gzdec description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "gzdec",
    "Template gzdec",
    gzdec_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
