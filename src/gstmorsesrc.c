/*
  This file is part of [morsesrc].
 
  [morsesrc] is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
 
  [morsesrc] is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Lesser General Public License for more details.
  
  You should have received a copy of the GNU Lesser General Public License
  along with [morsesrc]. If not, see <https://www.gnu.org/licenses/>.

  DESCRIPTION:	
  A GStreamer audio source plugin to convert TEXT to MORSE CODE using a internal 
  conversion table which converts the text string to a morsecode representation.
  The representation is then converted to audio in the format set by caps and sent
  out through the audio sink.

  USAGE:
  gst-launch-1.0 morsesrc text="CQ CQ DE VK3DG" ! autoaudiosink
  gst-launch-1.0 morsesrc text="CQ CQ DE VK3DG" one-shot=true ! autoaudiosink
 
The morse table lookup table method is a compact and efficient way to store the Morse code sequences for all the ASCII characters by
mapping each ASCII character to their corresponding Morse code sequences. 

Here's how it works:
Table Structure: 
	The table is an array of 128 unsigned short integers, where each index corresponds to an ASCII character code.
ASCII Character Codes:
	The table covers the first 128 ASCII character codes, which include all the printable characters (letters, digits, punctuation, etc.) and some non-printable characters (control characters).
Morse Code Representation: 
	Each entry in the table represents a Morse code sequence as a binary number. The binary number is encoded in a specific format:
	The high 3 bits (bits 6-8) represent the number of symbols (dots or dashes) in the Morse code sequence. If this value is 0, it means the sequence has 8 symbols.
	The low 7 bits (bits 0-6) represent the actual Morse code sequence, where:
	0 represents a dot (.)
	1 represents a dash (-)
Example: 
	Let's take the character 'A' (ASCII code 65) as an example. The corresponding entry in the morse table is 0412. Breaking it down:
	High 3 bits: 04 ( binary 0100) means the sequence has 4 symbols.
	Low 7 bits: 12 (binary 00010010) represents the Morse code sequence: .- (dot-dash).
Lookup:
	When the code needs to convert a character to Morse code, it looks up the corresponding entry in the morse table using the ASCII character code as the index.
	It then extracts the Morse code sequence from the entry and uses it to generate the Morse code.
	Here's a step-by-step example of how the code uses the morse table to convert a character to Morse code:

	1. Get the ASCII character code of the character to convert (e.g., 'A' = 65).
	2. Look up the corresponding entry in the morse table using the ASCII character code as the index (e.g., morse_table[65] = 0412).
	3. Extract the number of symbols from the high 3 bits (e.g., 04 = 4 symbols).
	4. Extract the Morse code sequence from the low 7 bits (e.g., 12 = .-).
	5. Generate the Morse code sequence using the extracted information (e.g., .- for the character 'A').


  VERSION CONTROL
  v1.0.0 Initial 
  V1.1.0 Added "about-to-finish" message to gstremaer bus similar to uridecodebin to notify end of buffer to avoid EOS and killing pipeline.
         Added "value validation and clamping for all parameters."
  V1.2.0 Added one-shot mode, improved thread safety, better error handling, and audio envelope shaping.
*/

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/audio/audio.h>
#include <gst/base/gstpushsrc.h>
#include <math.h>
#include <ctype.h>
#include "config.h"

// Define plugin package name etc
#define PACKAGE_VERSION "1.2.0" 
#define GST_LICENSE "LGPL"
#define GST_PACKAGE_NAME "GStreamer Morse Source"
#define GST_PACKAGE_ORIGIN "https://github.com/TVforME/morsesrc"

// Define the default audio format
#define DEFAULT_FORMAT_STR GST_AUDIO_NE ("S16")

// Define the default audio rate
#define DEFAULT_RATE 44100

// Define the default frequency
#define DEFAULT_FREQUENCY 880.0

// Define the default volume
#define DEFAULT_VOLUME 0.5

// Define the default words per minute
#define DEFAULT_WPM 20

// Set resonable boundaries on values.
#define MIN_FREQUENCY 400.0    // 400 Hz minimum
#define MAX_FREQUENCY 2000.0   // 2 kHz maximum
#define MIN_VOLUME 0.0         // Silence
#define MAX_VOLUME 1.0         // Full volume
#define MIN_WPM 5             // 5 WPM minimum (very slow)
#define MAX_WPM 30            // 30 WPM maximum (very fast)

// Define the supported audio formats
#define FORMAT_STR  " { S16LE, S16BE, U16LE, U16BE, "	\
  "S24_32LE, S24_32BE, U24_32LE, U24_32BE, "		\
  "S32LE, S32BE, U32LE, U32BE, "			\
  "S24LE, S24BE, U24LE, U24BE, "			\
  "S20LE, S20BE, U20LE, U20BE, "			\
  "S18LE, S18BE, U18LE, U18BE, "			\
  "F32LE, F32BE, F64LE, F64BE, "			\
  "S8, U8 }"


static unsigned short morse_table[128] = {
  /*00 */ 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
  /*08 */ 0000, 0000, 0412, 0000, 0000, 0412, 0000, 0000,
  /*10 */ 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
  /*18 */ 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
  /*20 */ 0000, 0665, 0622, 0000, 0000, 0000, 0502, 0636,
  /*28 */ 0515, 0000, 0000, 0512, 0663, 0000, 0652, 0511,
  /*30 */ 0537, 0536, 0534, 0530, 0520, 0500, 0501, 0503,
  /*38 */ 0507, 0517, 0607, 0625, 0000, 0521, 0000, 0614,
  /*40 */ 0000, 0202, 0401, 0405, 0301, 0100, 0404, 0303,
  /*48 */ 0400, 0200, 0416, 0305, 0402, 0203, 0201, 0307,
  /*50 */ 0406, 0413, 0302, 0300, 0101, 0304, 0410, 0306,
  /*58 */ 0411, 0415, 0403, 0000, 0000, 0000, 0000, 0000,
  /*60 */ 0000, 0202, 0401, 0405, 0301, 0100, 0404, 0303,
  /*68 */ 0400, 0200, 0416, 0305, 0402, 0203, 0201, 0307,
  /*70 */ 0406, 0413, 0302, 0300, 0101, 0304, 0410, 0306,
  /*78 */ 0411, 0415, 0403, 0000, 0000, 0000, 0000, 0000
};

// Forward type declarations
typedef struct _GstMorseSrc GstMorseSrc;
typedef struct _GstMorseSrcClass GstMorseSrcClass;

// Define the function pointer type for generating morse code
typedef void (*CW_GENERATE_FUNC) (GstMorseSrc*, guint8 *, gint);

// Define the class structure for the morse source
struct _GstMorseSrcClass
{
  GstPushSrcClass parent_class;
};

// Define the structure for the morse source
struct _GstMorseSrc
{
  GstPushSrc parent;
  gdouble frequency;
  gdouble volume;
  gint wpm;
  gboolean one_shot;
  CW_GENERATE_FUNC cwfunc;
  GstAudioFormatPack packfunc;
  guint packsize;
  gchar *text;
  GString *generated_morse;
  guint position;
  guint samples_per_dot;
  guint samples_per_dash;
  guint samples_per_space;
  GstClockTime timestamp;
  gdouble phase;
  gdouble phase_increment;
  GstSegment segment;
  GstAudioInfo info;
  
  // Thread safety and state management
  GMutex lock;
  gboolean new_text_pending;
  gchar *pending_text;
  gboolean about_to_finish_posted;
  gboolean playback_complete;
  GstState state;
  GstState pending_state;
};

// Define the type macros
#define GST_TYPE_MORSE_SRC (gst_morse_src_get_type())
#define GST_MORSE_SRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_MORSE_SRC, GstMorseSrc))

// Define the properties
enum {
  PROP_0,
  PROP_FREQUENCY,
  PROP_VOLUME,
  PROP_WPM,
  PROP_TEXT,
  PROP_ONE_SHOT,
  LAST_PROP
};

// Function declarations
static void morse_send_char (GString *text, int ch);
static void morse_send_string (GString *text, const char *str);
static GstCaps *gst_morse_src_fixate (GstBaseSrc * bsrc, GstCaps * caps);
static gboolean gst_morse_src_setcaps (GstBaseSrc * basesrc, GstCaps * caps);
static void gst_morse_src_class_init (GstMorseSrcClass * klass);
static void gst_morse_src_init (GstMorseSrc * src);
static GType gst_morse_src_get_type (void);

// Type registration
G_DEFINE_TYPE (GstMorseSrc, gst_morse_src, GST_TYPE_PUSH_SRC)

// Define the pad template for the morse source
static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE ("src",
                        GST_PAD_SRC,
                        GST_PAD_ALWAYS,
                        GST_STATIC_CAPS ("audio/x-raw, "
                                       "format = (string) " FORMAT_STR ", "
                                       "rate = " GST_AUDIO_RATE_RANGE ", "
                                       "layout = interleaved,"
                                       "channels = " GST_AUDIO_CHANNELS_RANGE)
                        );

static void
gst_morse_src_post_about_to_finish (GstMorseSrc *src)
{
  GstMessage *message;
  
  message = gst_message_new_application (GST_OBJECT (src),
      gst_structure_new ("about-to-finish",
          "source", G_TYPE_STRING, "morsesrc",
          NULL));
  
  gst_element_post_message (GST_ELEMENT (src), message);
}

static void
gst_morse_src_post_playback_complete (GstMorseSrc *src)
{
  GstMessage *message;
  
  message = gst_message_new_application (GST_OBJECT (src),
      gst_structure_new ("morse-playback-complete",
          "source", G_TYPE_STRING, "morsesrc",
          NULL));
  
  gst_element_post_message (GST_ELEMENT (src), message);
}

// Idle callback for state change
static gboolean
gst_morse_src_idle_state_change (gpointer user_data)
{
  GstMorseSrc *src = GST_MORSE_SRC (user_data);
  
  gst_element_set_state (GST_ELEMENT (src), GST_STATE_READY);
  g_object_unref (src);
  
  return G_SOURCE_REMOVE;
}

static void
gst_morse_src_update_text (GstMorseSrc *src)
{
  GstEvent *segment_event;
  gboolean was_playing = FALSE;

  g_mutex_lock(&src->lock);
  if (!src->new_text_pending || !src->pending_text) {
    g_mutex_unlock(&src->lock);
    return;
  }

  was_playing = (src->state == GST_STATE_PLAYING);

  // Update the text
  if (src->text)
    g_free(src->text);
  src->text = src->pending_text;
  src->pending_text = NULL;
  src->new_text_pending = FALSE;

  if (src->generated_morse)
    g_string_free(src->generated_morse, TRUE);

  src->generated_morse = g_string_new(NULL);
  morse_send_string(src->generated_morse, src->text);
  src->position = 0;
  src->about_to_finish_posted = FALSE;
  src->playback_complete = FALSE;
  src->timestamp = 0;

  // Reset segment
  gst_segment_init(&src->segment, GST_FORMAT_TIME);

  if (was_playing) {
    // Send new segment
    segment_event = gst_event_new_segment(&src->segment);
    gst_pad_push_event(GST_BASE_SRC_PAD(src), segment_event);
  }

  g_mutex_unlock(&src->lock);

  // Notify the pipeline of format changes
  gst_element_post_message(GST_ELEMENT(src),
      gst_message_new_duration_changed(GST_OBJECT(src)));
}

static GstStateChangeReturn
gst_morse_src_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstMorseSrc *src = GST_MORSE_SRC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      g_mutex_lock(&src->lock);
      src->state = GST_STATE_READY;
      src->playback_complete = FALSE;
      g_mutex_unlock(&src->lock);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      g_mutex_lock(&src->lock);
      src->state = GST_STATE_PAUSED;
      g_mutex_unlock(&src->lock);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      g_mutex_lock(&src->lock);
      src->state = GST_STATE_PLAYING;
      g_mutex_unlock(&src->lock);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (gst_morse_src_parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      g_mutex_lock(&src->lock);
      src->state = GST_STATE_PAUSED;
      g_mutex_unlock(&src->lock);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      g_mutex_lock(&src->lock);
      src->state = GST_STATE_READY;
      g_mutex_unlock(&src->lock);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      g_mutex_lock(&src->lock);
      src->state = GST_STATE_NULL;
      g_mutex_unlock(&src->lock);
      break;
    default:
      break;
  }

  return ret;
}

static void
morse_send_char (GString *text, int ch)
{
  int nsyms, bitreg;

  if (ch == ' ')
    {
      g_string_append (text, "  ");
      return;
    }

  bitreg = morse_table[ch & 0x7f];

  if ((nsyms = (bitreg >> 6) & 07) == 0)
    nsyms = 8;

  bitreg &= 077;

  while (nsyms-- > 0)
    {
      g_string_append_c (text, ' ');
      if (bitreg & 01)
        {
          g_string_append_c (text, '-');
        }
      else
        {
          g_string_append_c (text, '.');
        }
      bitreg >>= 1;
    }

  g_string_append_c (text, ' ');
}

static void
morse_send_string (GString *text, const char *str)
{
  while (*str)
    {
      morse_send_char (text, toupper (*str));
      str++;
    }

  g_string_append (text, "   ");
}

// Define the macro for generating morse code with envelope shaping (20ms fade)
#define CW_GENERATOR(sample_t, scale)                                  \
static void                                                            \
MORSE_CW_GENERATE_##sample_t (GstMorseSrc *src,                        \
                             guint8 *buf, gint samples)                \
{                                                                      \
  sample_t *data = (sample_t *) buf;                                   \
  /* Calculate 20ms fade in samples based on sample rate */           \
  gint fade_samples = (gint)(0.020 * GST_AUDIO_INFO_RATE(&src->info)); \
  /* Ensure fade doesn't exceed half the element duration */          \
  fade_samples = MIN(fade_samples, samples / 2);                       \
                                                                       \
  for (gint i = 0; i < samples; i++) {                                 \
    gdouble sample;                                                    \
    gdouble envelope = 1.0;                                            \
                                                                       \
    /* Apply 20ms fade in/out to reduce clicks */                     \
    if (i < fade_samples)                                              \
      envelope = (gdouble)i / fade_samples;                            \
    else if (i > samples - fade_samples)                              \
      envelope = (gdouble)(samples - i) / fade_samples;                \
                                                                       \
    for (gint j = 0; j < GST_AUDIO_INFO_CHANNELS (&src->info); j++) {  \
      sample = src->volume * scale * envelope * sin(src->phase);       \
      data[i * GST_AUDIO_INFO_CHANNELS (&src->info) + j] = sample;     \
    }                                                                  \
    src->phase += src->phase_increment;                                \
    if (src->phase >= 2.0 * G_PI)                                      \
      src->phase -= 2.0 * G_PI;                                        \
  }                                                                    \
}


// Generate morse code for different sample types
CW_GENERATOR (gint16, 32767.0)
CW_GENERATOR (gint32, 2147483647.0)
CW_GENERATOR (gfloat, 1.0)
CW_GENERATOR (gdouble, 1.0)

static void
gst_morse_src_set_property (GObject *object, guint prop_id,
                           const GValue *value, GParamSpec *pspec)
{
  GstMorseSrc *src = GST_MORSE_SRC (object);

  switch (prop_id)
    {
    case PROP_FREQUENCY:
      {
        gdouble freq = g_value_get_double (value);
        if (freq < MIN_FREQUENCY || freq > MAX_FREQUENCY) {
          GST_WARNING_OBJECT (src, "Frequency %f Hz out of range, clamping", freq);
          freq = CLAMP(freq, MIN_FREQUENCY, MAX_FREQUENCY);
        }
        src->frequency = freq;
        if (GST_AUDIO_INFO_RATE(&src->info) > 0) {
          src->phase_increment = 2.0 * G_PI * src->frequency / GST_AUDIO_INFO_RATE(&src->info);
        }
      }
      break;
    case PROP_VOLUME:
      {
        gdouble vol = g_value_get_double (value);
        if (vol < MIN_VOLUME || vol > MAX_VOLUME) {
          GST_WARNING_OBJECT (src, "Volume %f out of range, clamping", vol);
          vol = CLAMP(vol, MIN_VOLUME, MAX_VOLUME);
        }
        src->volume = vol;
      }
      break;
    case PROP_WPM:
      {
        gint wpm = g_value_get_int (value);
        if (wpm < MIN_WPM || wpm > MAX_WPM) {
          GST_WARNING_OBJECT (src, "WPM %d out of range, clamping", wpm);
          wpm = CLAMP(wpm, MIN_WPM, MAX_WPM);
        }
        src->wpm = wpm;
        
        // Recalculate timing if audio info is available
        if (GST_AUDIO_INFO_RATE(&src->info) > 0) {
          gdouble dot_duration = 1.2 / src->wpm;
          src->samples_per_dot = (guint)(dot_duration * GST_AUDIO_INFO_RATE(&src->info));
          if (src->samples_per_dot < 100) {
            src->samples_per_dot = 100;
          }
          src->samples_per_dash = src->samples_per_dot * 3;
          src->samples_per_space = src->samples_per_dot;
        }
      }
      break;
    case PROP_TEXT:
      {
        const gchar *new_text = g_value_get_string(value);
        if (!new_text || strlen(new_text) == 0) {
          GST_WARNING_OBJECT (src, "Empty text provided, ignoring");
          return;
        }
        
        g_mutex_lock(&src->lock);
        if (src->pending_text)
          g_free(src->pending_text);
        src->pending_text = g_strdup(new_text);
        src->new_text_pending = TRUE;
        src->playback_complete = FALSE;
        g_mutex_unlock(&src->lock);
      }
      break;
    case PROP_ONE_SHOT:
      src->one_shot = g_value_get_boolean(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gst_morse_src_get_property (GObject *object, guint prop_id,
                           GValue *value, GParamSpec *pspec)
{
  GstMorseSrc *src = GST_MORSE_SRC (object);

  switch (prop_id)
    {
    case PROP_FREQUENCY:
      g_value_set_double (value, src->frequency);
      break;
    case PROP_VOLUME:
      g_value_set_double (value, src->volume);
      break;
    case PROP_WPM:
      g_value_set_int (value, src->wpm);
      break;
    case PROP_TEXT:
      g_mutex_lock(&src->lock);
      g_value_set_string (value, src->text);
      g_mutex_unlock(&src->lock);
      break;
    case PROP_ONE_SHOT:
      g_value_set_boolean (value, src->one_shot);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gst_morse_src_finalize (GObject * object)
{
  GstMorseSrc *src = GST_MORSE_SRC (object);

  g_mutex_lock(&src->lock);
  
  if (src->pending_text) {
    g_free(src->pending_text);
    src->pending_text = NULL;
  }
  if (src->text) {
    g_free(src->text);
    src->text = NULL;
  }
  if (src->generated_morse) {
    g_string_free(src->generated_morse, TRUE);
    src->generated_morse = NULL;
  }
  
  g_mutex_unlock(&src->lock);
  g_mutex_clear(&src->lock);

  G_OBJECT_CLASS (gst_morse_src_parent_class)->finalize (object);
}

static GstFlowReturn
gst_morse_src_create (GstPushSrc *pushsrc, GstBuffer **buffer)
{
  GstMorseSrc *src = GST_MORSE_SRC (pushsrc);

  // Check for new text
  if (src->new_text_pending) {
    gst_morse_src_update_text(src);
    if (!src->generated_morse || src->generated_morse->len == 0) {
      return GST_FLOW_EOS;
    }
    
    // Create a small silent buffer to maintain pipeline flow
    guint num_samples = src->samples_per_dot;
    size_t bpf = GST_AUDIO_INFO_BPF(&src->info);
    GstBuffer *buf = gst_buffer_new_and_alloc(num_samples * bpf);
    gst_buffer_memset(buf, 0, 0, num_samples * bpf);
    
    GST_BUFFER_PTS(buf) = src->timestamp;
    GST_BUFFER_DURATION(buf) = 
      gst_util_uint64_scale(num_samples, GST_SECOND, GST_AUDIO_INFO_RATE(&src->info));
    src->timestamp += GST_BUFFER_DURATION(buf);
    *buffer = buf;
    
    return GST_FLOW_OK;
  }

  if (!src->generated_morse || src->position >= src->generated_morse->len) {
    if (src->one_shot && !src->playback_complete) {
      src->playback_complete = TRUE;
      
      // Post completion message
      gst_morse_src_post_playback_complete(src);
      
      // Schedule state change to READY in idle callback to avoid deadlock
      g_idle_add(gst_morse_src_idle_state_change, g_object_ref(src));
      
      return GST_FLOW_FLUSHING;
    }
    return GST_FLOW_EOS;
  }

  guint samples_per_dot = src->samples_per_dot;
  guint samples_per_dash = src->samples_per_dot * 3;
  guint samples_per_space = src->samples_per_dot;
  guint max_samples = 5292*10;

  size_t bpf = src->packfunc
    ? src->packsize * GST_AUDIO_INFO_CHANNELS (&src->info)
    : (size_t) GST_AUDIO_INFO_BPF (&src->info);

  GstBuffer *buf = gst_buffer_new_and_alloc (max_samples * bpf);
  GstMapInfo map;
  gst_buffer_map (buf, &map, GST_MAP_WRITE);
  gst_buffer_memset (buf, 0, 0, max_samples * bpf);
  
  guint i = 0;

  while (i < max_samples && src->position < src->generated_morse->len)
    {
      char symbol = src->generated_morse->str[src->position];
      guint num_samples = 0;

      switch (symbol) {
      case '.':
        num_samples = samples_per_dot;
        break;
      case '-':
        num_samples = samples_per_dash;
        break;
      case ' ':
        num_samples = samples_per_space;
        break;
      }

      num_samples = MIN (max_samples - i, num_samples);

      if (symbol != ' ')
        {
          src->cwfunc (src, map.data + i * bpf, num_samples);
        }
      
      i += num_samples;

      if (num_samples < samples_per_space
          && samples_per_space < max_samples - i)
        {
          i += samples_per_space - num_samples;
        }
      
      src->position++;
    }

  // Check if we're near the end (90% through the morse code)
  if (src->generated_morse && 
      src->position > (src->generated_morse->len * 0.9) &&
      !src->about_to_finish_posted) {
    gst_morse_src_post_about_to_finish (src);
    src->about_to_finish_posted = TRUE;
  }

  if (src->packfunc)
    {
      GstBuffer *rbuf = gst_buffer_new_and_alloc (
        max_samples * GST_AUDIO_INFO_BPF (&src->info)
      );

      GstMapInfo rmap;
      gst_buffer_map (rbuf, &rmap, GST_MAP_WRITE);
      src->packfunc (src->info.finfo, 0, map.data, rmap.data, i);
      gst_buffer_set_size (rbuf, GST_AUDIO_INFO_BPF (&src->info) * i);
      gst_buffer_unmap (rbuf, &rmap);
      gst_buffer_unref (buf);
      buf = rbuf;
    }
  else
    {
      gst_buffer_unmap (buf, &map);
      gst_buffer_set_size (buf, i * bpf);
    }
  
  GST_BUFFER_PTS (buf) = src->timestamp;
  GST_BUFFER_DURATION (buf) =
    gst_util_uint64_scale (i, GST_SECOND, GST_AUDIO_INFO_RATE (&src->info));
  src->timestamp += GST_BUFFER_DURATION (buf);
  *buffer = buf;

  return GST_FLOW_OK;
}

static GstCaps *
gst_morse_src_fixate (GstBaseSrc *bsrc, GstCaps *caps)
{
  GstMorseSrc *src = GST_MORSE_SRC (bsrc);
  gint channels;

  caps = gst_caps_make_writable (caps);
  GstStructure *structure = gst_caps_get_structure (caps, 0);

  GST_DEBUG_OBJECT (src, "fixating samplerate to %d", GST_AUDIO_DEF_RATE);
  gst_structure_fixate_field_nearest_int (structure, "rate", GST_AUDIO_DEF_RATE);
  gst_structure_fixate_field_string (structure, "format", DEFAULT_FORMAT_STR);
  gst_structure_fixate_field_string (structure, "layout", "interleaved");
  gst_structure_fixate_field_nearest_int (structure, "channels", 1);

  if (gst_structure_get_int (structure, "channels", &channels) && channels > 2) {
    if (!gst_structure_has_field_typed (structure, "channel-mask",
                                      GST_TYPE_BITMASK))
      gst_structure_set (structure, "channel-mask", GST_TYPE_BITMASK, 0ULL,
                        NULL);
  }

  caps = GST_BASE_SRC_CLASS (gst_morse_src_parent_class)->fixate (bsrc, caps);

  return caps;
}

static gboolean
gst_morse_src_setcaps (GstBaseSrc *basesrc, GstCaps *caps)
{
  GstMorseSrc *src = GST_MORSE_SRC (basesrc);
  GstAudioInfo info;

  if (!gst_audio_info_from_caps (&info, caps))
    goto invalid_caps;

  GST_DEBUG_OBJECT (src, "negotiated to caps %" GST_PTR_FORMAT, (void *) caps);
  
  // More precise timing calculation
  gdouble dot_duration = 1.2 / src->wpm;
  src->samples_per_dot = (guint)(dot_duration * GST_AUDIO_INFO_RATE(&info));
  
  // Ensure minimum samples to avoid clicks
  if (src->samples_per_dot < 100) {
    src->samples_per_dot = 100;
    GST_WARNING_OBJECT(src, "Dot duration too short, using minimum");
  }
  
  src->samples_per_dash = src->samples_per_dot * 3;
  src->samples_per_space = src->samples_per_dot;

  src->cwfunc = NULL;
  src->packfunc = NULL;
  src->packsize = 0;

  src->info = info;
  
  src->phase = 0.0;
  src->phase_increment = 2.0 * G_PI * src->frequency / GST_AUDIO_INFO_RATE(&src->info);

  switch (GST_AUDIO_FORMAT_INFO_FORMAT (src->info.finfo))
    {
    case GST_AUDIO_FORMAT_S16:
      src->cwfunc = MORSE_CW_GENERATE_gint16;
      break;
    case GST_AUDIO_FORMAT_S32:
      src->cwfunc = MORSE_CW_GENERATE_gint32;
      break;
    case GST_AUDIO_FORMAT_F32:
      src->cwfunc = MORSE_CW_GENERATE_gfloat;
      break;
    case GST_AUDIO_FORMAT_F64:
      src->cwfunc = MORSE_CW_GENERATE_gdouble;
      break;
    default:
      switch (src->info.finfo->unpack_format)
        {
        case GST_AUDIO_FORMAT_S32:
          src->cwfunc = MORSE_CW_GENERATE_gint32;
          src->packfunc = src->info.finfo->pack_func;
          src->packsize = sizeof (gint32);
          break;
        case GST_AUDIO_FORMAT_F64:
          src->cwfunc = MORSE_CW_GENERATE_gdouble;
          src->packfunc = src->info.finfo->pack_func;
          src->packsize = sizeof (gdouble);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  
  return TRUE;

 invalid_caps:
  {
    GST_ERROR_OBJECT (basesrc, "received invalid caps");
    return FALSE;
  }
}

static gboolean
gst_morse_src_start (GstBaseSrc *basesrc)
{
  GstMorseSrc *src = GST_MORSE_SRC (basesrc);

  // Configure base source properties
  gst_base_src_set_live(basesrc, FALSE);
  gst_base_src_set_format(basesrc, GST_FORMAT_TIME);

  // Reset playback state
  src->playback_complete = FALSE;

  if (src->generated_morse)
    {
      g_string_free(src->generated_morse, TRUE);
    }

  src->generated_morse = g_string_new(NULL);
  
  // Validate text before processing
  if (!src->text || strlen(src->text) == 0) {
    GST_WARNING_OBJECT (src, "No text provided, using default");
    if (src->text) g_free(src->text);
    src->text = g_strdup("OK");
  }
  
  morse_send_string(src->generated_morse, src->text);
  src->position = 0;
  src->timestamp = 0;
  src->about_to_finish_posted = FALSE;

  // Initialize segment
  gst_segment_init(&src->segment, GST_FORMAT_TIME);

  // Send stream-start event first
  GstEvent *stream_start = gst_event_new_stream_start("morsesrc-stream");
  gst_pad_push_event(GST_BASE_SRC_PAD(src), stream_start);

  // Set and send caps
  GstCaps *caps = gst_caps_new_simple("audio/x-raw",
      "format", G_TYPE_STRING, DEFAULT_FORMAT_STR,
      "rate", G_TYPE_INT, DEFAULT_RATE,
      "channels", G_TYPE_INT, 1,
      "layout", G_TYPE_STRING, "interleaved",
      NULL);
  gst_pad_push_event(GST_BASE_SRC_PAD(src), gst_event_new_caps(caps));
  gst_caps_unref(caps);

  // Then send segment event
  GstEvent *segment_event = gst_event_new_segment(&src->segment);
  gst_pad_push_event(GST_BASE_SRC_PAD(src), segment_event);

  return TRUE;
}

static gboolean
gst_morse_src_stop (GstBaseSrc *basesrc)
{
  GstMorseSrc *src = GST_MORSE_SRC (basesrc);

  g_mutex_lock(&src->lock);
  
  if (src->generated_morse)
    {
      g_string_free(src->generated_morse, TRUE);
      src->generated_morse = NULL;
    }
  
  src->playback_complete = FALSE;
  
  g_mutex_unlock(&src->lock);

  return TRUE;
}

static void
gst_morse_src_init (GstMorseSrc *src)
{
  src->frequency = DEFAULT_FREQUENCY;
  src->volume = DEFAULT_VOLUME;
  src->wpm = DEFAULT_WPM;
  src->one_shot = FALSE;
  src->text = g_strdup("OK");  
  src->generated_morse = NULL;
  src->position = 0;
  src->timestamp = 0;
  src->phase = 0.0;
  src->phase_increment = 0.0;
  
  // Initialize new members
  g_mutex_init(&src->lock);
  src->new_text_pending = FALSE;
  src->pending_text = NULL;
  src->about_to_finish_posted = FALSE;
  src->playback_complete = FALSE;
  src->state = GST_STATE_NULL;
  src->pending_state = GST_STATE_NULL;

  // Initialize segment
  gst_segment_init(&src->segment, GST_FORMAT_TIME);
}

static void
gst_morse_src_class_init (GstMorseSrcClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *pushsrc_class = GST_PUSH_SRC_CLASS(klass);

  gobject_class->set_property = gst_morse_src_set_property;
  gobject_class->get_property = gst_morse_src_get_property;
  gobject_class->finalize = gst_morse_src_finalize;

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_morse_src_change_state);

  // Install properties with limits
  g_object_class_install_property (gobject_class, PROP_FREQUENCY,
      g_param_spec_double ("frequency", "Frequency", 
          "Frequency in Hz (400-2000 Hz)",
          MIN_FREQUENCY,
          MAX_FREQUENCY,
          DEFAULT_FREQUENCY,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_VOLUME,
      g_param_spec_double ("volume", "Volume", 
          "Volume level (0.0-1.0)",
          MIN_VOLUME,
          MAX_VOLUME,
          DEFAULT_VOLUME,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_WPM,
      g_param_spec_int ("wpm", "Words per minute", 
          "Speed in words per minute (5-30 WPM)",
          MIN_WPM,
          MAX_WPM,
          DEFAULT_WPM,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_TEXT,
      g_param_spec_string ("text", "Morse text",
          "String to convert to Morse code", NULL,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_ONE_SHOT,
      g_param_spec_boolean ("one-shot", "One Shot Mode",
          "Automatically transition to READY state after playback completes",
          FALSE,
          G_PARAM_READWRITE));

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &src_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Morse Code to Audio",
      "Source/Audio/Text",
      "Convert text to morse code\n"
      "  Features\n"
      "                           • Text to morse code conversion\n"
      "                           • Adjustable frequency (400-2000 Hz)\n"
      "                           • Variable speed (5-30 WPM)\n"
      "                           • Volume control (0.0-1.0)\n"
      "                           • One-shot mode support\n"
      "                           • About-to-finish notification\n"
      "                           • Envelope shaping to reduce clicks\n\n"
      "  Build Date               " BUILD_DATE "\n"
      "  Version                  " PACKAGE_VERSION,
      "Robert Hensel <vk3dgtv@gmail.com>"); 

  // Set base source functions
  basesrc_class->start = GST_DEBUG_FUNCPTR (gst_morse_src_start);
  basesrc_class->stop = GST_DEBUG_FUNCPTR (gst_morse_src_stop);
  basesrc_class->fixate = GST_DEBUG_FUNCPTR (gst_morse_src_fixate);
  basesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_morse_src_setcaps);
  pushsrc_class->create = GST_DEBUG_FUNCPTR (gst_morse_src_create);
}

static gboolean
plugin_init (GstPlugin *plugin)
{
  gst_plugin_add_dependency_simple(
    plugin,
    NULL,
    NULL,
    NULL,
    GST_PLUGIN_DEPENDENCY_FLAG_NONE);

  return gst_element_register (plugin, "morsesrc", GST_RANK_NONE,
      GST_TYPE_MORSE_SRC);
}

// Update the plugin definition with proper PACKAGE definition
#ifndef PACKAGE
#define PACKAGE "morsesrc"
#endif

GST_PLUGIN_DEFINE (
  GST_VERSION_MAJOR,
  GST_VERSION_MINOR,
  morsesrc,
  "Text to Morse Code Audio Converter\n"
  "  Build Date               " BUILD_DATE,
  plugin_init,
  PACKAGE_VERSION,
  GST_LICENSE,
  GST_PACKAGE_NAME,
  GST_PACKAGE_ORIGIN
)

