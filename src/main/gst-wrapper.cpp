#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstaudiofilter.h>
#include <string.h>

#include <lsp-plug.in/common/finally.h>
#include <lsp-plug.in/common/types.h>

#include <private/version.h>

GST_DEBUG_CATEGORY_STATIC(damage_detector_debug);
#define GST_CAT_DEFAULT damage_detector_debug

#define GST_TYPE_DAMAGE_DETECTOR (gst_damage_detector_get_type())
G_DECLARE_FINAL_TYPE(
    GstDamageDetector,
    gst_damage_detector,
    GST,
    DAMAGE_DETECTOR,
    GstAudioFilter);

struct _GstDamageDetector
{
    GstAudioFilter audiofilter;

    // here you can add additional per-instance data such as properties
};


enum
{
    /* FILL ME */
    LAST_SIGNAL
};

enum
{
    ARG_0
    /* FILL ME */
};

G_DEFINE_TYPE(
    GstDamageDetector,
    gst_damage_detector,
    GST_TYPE_AUDIO_FILTER);

GST_ELEMENT_REGISTER_DEFINE(
    damage_detector,
    "damage_detector",
    GST_RANK_NONE,
    GST_TYPE_DAMAGE_DETECTOR);

static void gst_damage_detector_set_property(
    GObject *object,
    guint prop_id,
    const GValue *value,
    GParamSpec *pspec);

static void gst_damage_detector_get_property(
    GObject *object,
    guint prop_id,
    GValue *value,
    GParamSpec *pspec);

static gboolean gst_damage_detector_setup(
    GstAudioFilter *object,
    const GstAudioInfo *info);

static GstFlowReturn gst_damage_detector_filter(
    GstBaseTransform *object,
    GstBuffer *outbuf,
    GstBuffer *inbuf);

static GstFlowReturn gst_damage_detector_filter_inplace(
    GstBaseTransform *object,
    GstBuffer * buf);

// We only support 32-bit IEEE 754 floating point
#define SUPPORTED_CAPS_STRING \
    GST_AUDIO_CAPS_MAKE(GST_AUDIO_NE(F32))

// GObject vmethod implementations
static void gst_damage_detector_class_init(GstDamageDetectorClass * klass)
{
    GObjectClass *gobject_class = reinterpret_cast<GObjectClass *>(klass);
    GstElementClass *element_class = reinterpret_cast<GstElementClass *>(klass);
    GstBaseTransformClass *btrans_class = reinterpret_cast<GstBaseTransformClass *>(klass);
    GstAudioFilterClass *audio_filter_class = reinterpret_cast<GstAudioFilterClass *>(klass);

    GstCaps *caps;

    gobject_class->set_property = gst_damage_detector_set_property;
    gobject_class->get_property = gst_damage_detector_get_property;

    // this function will be called when the format is set before the
    // first buffer comes in, and whenever the format changes
    audio_filter_class->setup = gst_damage_detector_setup;

    // here you set up functions to process data (either in place, or from
    // one input buffer to another output buffer); only one is required
    btrans_class->transform = gst_damage_detector_filter;
    btrans_class->transform_ip = gst_damage_detector_filter_inplace;

    // Set some basic metadata about your new element
    gst_element_class_set_details_simple(
      element_class,
      "Audio Damage Detector",
      "Filter/Effect/Audio",
      "Detects damage of audio stream",
      "Vladimir Sadovnikov <sadko4u@gmail.com>");

    caps = gst_caps_from_string(SUPPORTED_CAPS_STRING);
    gst_audio_filter_class_add_pad_templates(audio_filter_class, caps);
    gst_caps_unref (caps);
}

static void gst_damage_detector_init(GstDamageDetector *filter)
{
    // This function is called when a new filter object is created. You
    // would typically do things like initialise properties to their
    // default values here if needed.
}

static void gst_damage_detector_set_property(
    GObject *object,
    guint prop_id,
    const GValue *value,
    GParamSpec *pspec)
{
    GstDamageDetector *filter = GST_DAMAGE_DETECTOR(object);

    GST_OBJECT_LOCK(filter);
    switch (prop_id)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
    GST_OBJECT_UNLOCK(filter);
}

static void gst_damage_detector_get_property(
    GObject * object,
    guint prop_id,
    GValue * value,
    GParamSpec * pspec)
{
    GstDamageDetector *filter = GST_DAMAGE_DETECTOR(object);

    GST_OBJECT_LOCK(filter);
    switch (prop_id)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
    GST_OBJECT_UNLOCK(filter);
}

static gboolean gst_damage_detector_setup(
    GstAudioFilter * object,
    const GstAudioInfo * info)
{
    GstDamageDetector *filter = GST_DAMAGE_DETECTOR(object);

    gint sample_rate = GST_AUDIO_INFO_RATE(info);
    gint channels = GST_AUDIO_INFO_CHANNELS(info);
    GstAudioFormat fmt = GST_AUDIO_INFO_FORMAT(info);

    GST_INFO_OBJECT(
        filter,
        "format %d (%s), rate %d, %d channels",
        fmt, GST_AUDIO_INFO_NAME(info), sample_rate, channels);

    // If any setup needs to be done (like memory allocated), do it here
    // The audio filter base class also saves the audio info in
    // GST_AUDIO_FILTER_INFO(filter) so it's automatically available
    // later from there as well

    return TRUE;
}

// You may choose to implement either a copying filter or an
// in-place filter (or both). Implementing only one will give
// full functionality, however, implementing both will cause
// audiofilter to use the optimal function in every situation,
// with a minimum of memory copies.
static GstFlowReturn gst_damage_detector_filter(
    GstBaseTransform *object,
    GstBuffer *inbuf,
    GstBuffer *outbuf)
{
    GstDamageDetector *filter = GST_DAMAGE_DETECTOR(object);
    GST_LOG_OBJECT (filter, "transform buffer");

    // Do something interesting here.  We simply copy the input data
    // to the output buffer for now.
    GstMapInfo map_in;
    if (!gst_buffer_map (inbuf, &map_in, GST_MAP_READ))
        return GST_FLOW_OK;
    lsp_finally { gst_buffer_unmap (inbuf, &map_in); };

    GstMapInfo map_out;
    if (!gst_buffer_map (outbuf, &map_out, GST_MAP_WRITE))
        return GST_FLOW_OK;
    lsp_finally { gst_buffer_unmap (outbuf, &map_out); };

    g_assert (map_out.size == map_in.size);
    memcpy (map_out.data, map_in.data, map_out.size);

    return GST_FLOW_OK;
}

static GstFlowReturn gst_damage_detector_filter_inplace(
    GstBaseTransform *object,
    GstBuffer *buf)
{
    GstDamageDetector *filter = GST_DAMAGE_DETECTOR(object);

    GST_LOG_OBJECT (filter, "transform buffer in place");

    // Do something interesting here.  Doing nothing means the input
    // buffer is simply pushed out as is without any modification
    GstMapInfo map;
    if (!gst_buffer_map (buf, &map, GST_MAP_READWRITE))
        return GST_FLOW_OK;
    lsp_finally { gst_buffer_unmap (buf, &map); };


    return GST_FLOW_OK;
}

static gboolean plugin_init(GstPlugin *plugin)
{
  // Register debug category for filtering log messages
  GST_DEBUG_CATEGORY_INIT(
      damage_detector_debug,
      "damage_detector",
      0,
      "Audio damage detector plugin");

  /* This is the name used in gst-launch-1.0 and gst_element_factory_make() */
  return GST_ELEMENT_REGISTER(damage_detector, plugin);
}

#ifndef PACKAGE
    #define PACKAGE     DAMAGE_DETECTOR_PACKAGE
#endif /* PACKAGE */

// gstreamer looks for this structure to register plugins
GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    damage_detector,
    "Audio damage detector plugin",
    plugin_init,
    LSP_DEF_VERSION_STR(
        DAMAGE_DETECTOR_MAJOR,
        DAMAGE_DETECTOR_MINOR,
        DAMAGE_DETECTOR_MICRO),
    DAMAGE_DETECTOR_LICENSE,
    DAMAGE_DETECTOR_PACKAGE,
    DAMAGE_DETECTOR_ORIGIN);
