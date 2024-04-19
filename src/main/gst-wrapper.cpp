#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstaudiofilter.h>
#include <string.h>

#include <lsp-plug.in/common/finally.h>
#include <lsp-plug.in/common/types.h>

#include <private/version.h>
#include <private/DamageDetector.h>

static constexpr size_t IO_BUF_SIZE     = 0x400;

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

    dd::DamageDetector *processor;
    float *left;
    float *right;
};


enum properties_t
{
    PROP_THRESHOLD,
    PROP_REACTIVITY,
    PROP_DETECT_TIME,
    PROP_ESTIMATION_TIME,
    PROP_EVENTS
};

#define gst_damage_detector_parent_class parent_class

G_DEFINE_TYPE( // @suppress("Unused static function")
    GstDamageDetector,
    gst_damage_detector,
    GST_TYPE_AUDIO_FILTER);

GST_ELEMENT_REGISTER_DEFINE(
    damage_detector,
    "damage_detector",
    GST_RANK_NONE,
    GST_TYPE_DAMAGE_DETECTOR);

static void gst_damage_detector_init(GstDamageDetector *filter);

static void gst_damage_detector_finalize(GObject * object);

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
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE (
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(
        "audio/x-raw, "
        "format = (string) " GST_AUDIO_NE(F32) ", "
        "channels = (int) 2, "
        "rate = (int) [ 1, max ]"
    )
);

static GstStaticPadTemplate source_factory = GST_STATIC_PAD_TEMPLATE (
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(
        "audio/x-raw, "
        "format = (string) " GST_AUDIO_NE(F32) ", "
        "channels = (int) 2, "
        "rate = (int) [ 1, max ]"
    )
);


// GObject vmethod implementations
static void gst_damage_detector_class_init(GstDamageDetectorClass * klass)
{
    GObjectClass *gobject_class = reinterpret_cast<GObjectClass *>(klass);
    GstElementClass *element_class = reinterpret_cast<GstElementClass *>(klass);
    GstBaseTransformClass *btrans_class = reinterpret_cast<GstBaseTransformClass *>(klass);
    GstAudioFilterClass *audio_filter_class = reinterpret_cast<GstAudioFilterClass *>(klass);

    gobject_class->set_property = gst_damage_detector_set_property;
    gobject_class->get_property = gst_damage_detector_get_property;
    gobject_class->finalize = gst_damage_detector_finalize;

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

    // Pads
    gst_element_class_add_pad_template(
        element_class,
        gst_static_pad_template_get(&sink_factory));
    gst_element_class_add_pad_template(
        element_class,
        gst_static_pad_template_get(&source_factory));

    // Properties
    g_object_class_install_property(
        gobject_class, PROP_THRESHOLD,
        g_param_spec_float(
            "threshold", "Threshold", "Signal trigger threshold [dB]",
            dd::DamageDetector::MIN_THRESHOLD, dd::DamageDetector::MAX_THRESHOLD, dd::DamageDetector::DFL_THRESHOLD,
            G_PARAM_READWRITE));

    g_object_class_install_property(
        gobject_class, PROP_REACTIVITY,
        g_param_spec_float(
            "reactivity", "Reactivity", "Reactivity of the RMS value calculation [ms]",
            dd::DamageDetector::MIN_REACTIVITY, dd::DamageDetector::MAX_REACTIVITY, dd::DamageDetector::DFL_REACTIVITY,
            G_PARAM_READWRITE));

    g_object_class_install_property(
        gobject_class, PROP_DETECT_TIME,
        g_param_spec_float(
            "d_time", "Detection time", "Audio click detection time [s]",
            dd::DamageDetector::MIN_DETECT_TIME, dd::DamageDetector::MAX_DETECT_TIME, dd::DamageDetector::DFL_DETECT_TIME,
            G_PARAM_READWRITE));

    g_object_class_install_property(
        gobject_class, PROP_ESTIMATION_TIME,
        g_param_spec_float(
            "e_time", "Estimation time", "Estimation time window for calculating number of events [s]",
            dd::DamageDetector::MIN_ESTIMATE_TIME, dd::DamageDetector::MAX_ESTIMATE_TIME, dd::DamageDetector::DFL_ESTIMATE_TIME,
            G_PARAM_READWRITE));

    g_object_class_install_property(
        gobject_class, PROP_EVENTS,
        g_param_spec_uint(
            "events", "Events", "The number of detected stream corruption events",
            0, dd::DamageDetector::MAX_EVENTS * 2, 0,
            G_PARAM_READABLE));
}

static void gst_damage_detector_init(GstDamageDetector *filter)
{
    // Initialize filter and buffers
    filter->processor   = new dd::DamageDetector(2);
    filter->left        = new float[IO_BUF_SIZE * 2];
    filter->right       = &filter->left[IO_BUF_SIZE];
}

static void gst_damage_detector_finalize(GObject * object)
{
    GstDamageDetector *filter = GST_DAMAGE_DETECTOR(object);

    // Finalize filter and buffers
    delete filter->processor;
    delete filter->left;

    filter->processor   = NULL;
    filter->left        = NULL;
    filter->right       = NULL;

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_damage_detector_set_property(
    GObject *object,
    guint prop_id,
    const GValue *value,
    GParamSpec *pspec)
{
    GstDamageDetector *filter = GST_DAMAGE_DETECTOR(object);

    GST_OBJECT_LOCK(filter);
    lsp_finally { GST_OBJECT_UNLOCK(filter); };

    dd::DamageDetector *p = filter->processor;

    switch (prop_id)
    {
        case PROP_THRESHOLD:
            p->set_threshold(g_value_get_float(value));
            break;

        case PROP_REACTIVITY:
            p->set_reactivity(g_value_get_float(value));
            break;

        case PROP_DETECT_TIME:
            p->set_detect_time(g_value_get_float(value));
            break;

        case PROP_ESTIMATION_TIME:
            p->set_detect_time(g_value_get_float(value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void gst_damage_detector_get_property(
    GObject * object,
    guint prop_id,
    GValue * value,
    GParamSpec * pspec)
{
    GstDamageDetector *filter = GST_DAMAGE_DETECTOR(object);

    GST_OBJECT_LOCK(filter);
    lsp_finally { GST_OBJECT_UNLOCK(filter); };

    dd::DamageDetector *p = filter->processor;

    switch (prop_id)
    {
        case PROP_THRESHOLD:
            g_value_set_float(value, p->threshold());
            break;

        case PROP_REACTIVITY:
            g_value_set_float(value, p->reactivity());
            break;

        case PROP_DETECT_TIME:
            g_value_set_float(value, p->detect_time());
            break;

        case PROP_ESTIMATION_TIME:
            g_value_set_float(value, p->estimation_time());
            break;

        case PROP_EVENTS:
            g_value_set_float(value, p->events_count());
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
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

    return GST_AUDIO_FILTER_CLASS(parent_class)->setup(object, info);
}

static GstFlowReturn gst_damage_detector_process(
    GstDamageDetector *object,
    void *dst, const void *src, size_t bytes)
{
    const size_t samples = bytes / (sizeof(float) * 2);

    const float *sptr   = reinterpret_cast<const float *>(src);
    float *dptr         = reinterpret_cast<float *>(dst);

    for (size_t offset=0; offset < samples; )
    {
        // Determine the number of samples to process
        const size_t to_do  = lsp::lsp_min(IO_BUF_SIZE, samples - offset);

        // De-interleave data
        for (size_t i=0; i<to_do; ++i, sptr += 2)
        {
            object->left[i] = sptr[0];
            object->right[i] = sptr[1];
        }

        // Do the processing stuff
        object->processor->bind_input(0, object->left);
        object->processor->bind_input(1, object->right);
        object->processor->bind_output(0, object->left);
        object->processor->bind_output(1, object->right);

        object->processor->process(to_do);

        // Interleave data
        for (size_t i=0; i<to_do; ++i, dptr += 2)
        {
            dptr[0] = object->left[i];
            dptr[1] = object->right[i];
        }

        // Update the offset
        offset             += to_do;
    }

    return GST_FLOW_OK;
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

    return gst_damage_detector_process(filter, map_out.data, map_in.data, map_out.size);
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

    return gst_damage_detector_process(filter, map.data, map.data, map.size);
}

static gboolean plugin_init(GstPlugin *plugin)
{
  // Register debug category for filtering log messages
  GST_DEBUG_CATEGORY_INIT(
      damage_detector_debug,
      "damage_detector",
      0,
      "Audio damage detector plugin");

  // This is the name used in gst-launch-1.0 and gst_element_factory_make()
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
