#include "obs.h"
#include "obs-module.h"
#include "obs-frontend-api.h"
#include "plugin-support.h"
#include "media-io/audio-io.h"

struct decklink_output_filter_context {
	obs_output_t *output;
	obs_source_t *source;
	obs_canvas_t *canvas;
	audio_t *silent_audio;

	bool active;
};

static bool silent_audio_callback(void *param, uint64_t start_ts, uint64_t end_ts, uint64_t *new_ts,
				   uint32_t active_mixers, struct audio_output_data *mixes)
{
	UNUSED_PARAMETER(param);
	UNUSED_PARAMETER(end_ts);
	UNUSED_PARAMETER(active_mixers);
	UNUSED_PARAMETER(mixes);
	*new_ts = start_ts;
	return true;
}

static const char *decklink_output_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("DecklinkOutput");
}

static void decklink_output_filter_stop(void *data)
{
	struct decklink_output_filter_context *filter = data;

	obs_log(LOG_INFO, "Filter stop requested (active=%s)", filter->active ? "true" : "false");

	if (!filter->active)
		return;

	filter->active = false;

	obs_output_force_stop(filter->output);
	obs_source_dec_showing(obs_filter_get_parent(filter->source));
	obs_output_release(filter->output);
	obs_canvas_release(filter->canvas);
	filter->output = NULL;
	filter->canvas = NULL;

	if (filter->silent_audio) {
		audio_output_close(filter->silent_audio);
		filter->silent_audio = NULL;
	}
}

static void decklink_output_filter_start(void *data, obs_data_t *settings)
{
	struct decklink_output_filter_context *filter = data;

	obs_log(LOG_INFO, "Filter start requested (active=%s, source_enabled=%s)",
		filter->active ? "true" : "false",
		obs_source_enabled(filter->source) ? "true" : "false");

	if (filter->active)
		return;

	if (!obs_source_enabled(filter->source)) {
		obs_log(LOG_WARNING, "Filter start skipped: filter is disabled");
		return;
	}

	obs_log(LOG_INFO, "Creating decklink output (device='%s')",
		obs_data_get_string(settings, "device_name"));
	filter->output = obs_output_create("decklink_output", "decklink_filter_output", settings, NULL);
	obs_log(LOG_INFO, "decklink output created: %s", filter->output ? "ok" : "FAILED");

	struct obs_video_info ovi;
	obs_get_video_info(&ovi);
	obs_log(LOG_INFO, "Canvas video info: %ux%u @ %u/%u fps, format=%u",
		ovi.base_width, ovi.base_height, ovi.fps_num, ovi.fps_den, ovi.output_format);

	filter->canvas = obs_canvas_create_private(NULL, &ovi, DEVICE);
	obs_log(LOG_INFO, "Canvas created: %s", filter->canvas ? "ok" : "FAILED");

	obs_source_t *parent = obs_filter_get_parent(filter->source);
	obs_log(LOG_INFO, "Setting canvas channel 0 to source: %s (type=%s)",
		parent ? obs_source_get_name(parent) : "NULL",
		parent ? obs_source_get_id(parent) : "NULL");
	obs_canvas_set_channel(filter->canvas, 0, parent);

	audio_t *audio = obs_get_audio();
	if (obs_data_get_bool(settings, "mute_audio")) {
		struct obs_audio_info oai;
		obs_get_audio_info(&oai);
		struct audio_output_info aoi = {
			.name = "decklink_filter_silent",
			.samples_per_sec = oai.samples_per_sec,
			.format = AUDIO_FORMAT_FLOAT_PLANAR,
			.speakers = oai.speakers,
			.input_callback = silent_audio_callback,
		};
		if (audio_output_open(&filter->silent_audio, &aoi) == AUDIO_OUTPUT_SUCCESS) {
			audio = filter->silent_audio;
		} else {
			obs_log(LOG_WARNING, "Failed to create silent audio context, using main audio");
			filter->silent_audio = NULL;
		}
	}

	obs_output_set_media(filter->output, obs_canvas_get_video(filter->canvas), audio);

	obs_source_inc_showing(obs_filter_get_parent(filter->source));

	filter->active = true;

	obs_log(LOG_INFO, "Calling obs_output_start...");
	bool started = obs_output_start(filter->output);
	obs_log(LOG_INFO, "obs_output_start returned: %s", started ? "true" : "false");

	if (!started) {
		const char *last_error = obs_output_get_last_error(filter->output);
		obs_log(LOG_ERROR, "Filter failed to start (last error: %s)", last_error ? last_error : "none");
		decklink_output_filter_stop(filter);
		return;
	}

	obs_log(LOG_INFO, "Filter started successfully");
}

static void decklink_output_filter_update(void *data, obs_data_t *settings)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(settings);
}

static void set_filter_enabled(void *data, calldata_t *calldata)
{
	struct decklink_output_filter_context *filter = data;

	bool enable = calldata_bool(calldata, "enabled");

	obs_log(LOG_INFO, "Filter enable signal received (enable=%s)", enable ? "true" : "false");

	if (!enable) {
		decklink_output_filter_stop(filter);
		return;
	}

	obs_data_t *settings = obs_source_get_settings(filter->source);
	bool auto_start = obs_data_get_bool(settings, "auto_start");

	obs_log(LOG_INFO, "Filter enable signal: auto_start=%s, active=%s", auto_start ? "true" : "false",
		filter->active ? "true" : "false");

	if (auto_start && !filter->active)
		decklink_output_filter_start(filter, settings);

	obs_data_release(settings);
}

static void decklink_frontend_event(enum obs_frontend_event event, void *private_data)
{
	struct decklink_output_filter_context *filter = private_data;

	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		obs_data_t *settings = obs_source_get_settings(filter->source);
		bool auto_start = obs_data_get_bool(settings, "auto_start");
		obs_log(LOG_INFO, "FINISHED_LOADING: auto_start=%s, active=%s, source_enabled=%s",
			auto_start ? "true" : "false", filter->active ? "true" : "false",
			obs_source_enabled(filter->source) ? "true" : "false");
		if (auto_start)
			decklink_output_filter_start(filter, settings);
		obs_data_release(settings);
	} else if (event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN) {
		obs_log(LOG_INFO, "SCRIPTING_SHUTDOWN: stopping filter");
		decklink_output_filter_stop(filter);
	}
}

static void *decklink_output_filter_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);

	struct decklink_output_filter_context *filter = bzalloc(sizeof(struct decklink_output_filter_context));
	filter->source = source;
	filter->active = false;
	filter->silent_audio = NULL;

	signal_handler_t *sh = obs_source_get_signal_handler(filter->source);
	signal_handler_connect(sh, "enable", set_filter_enabled, filter);

	obs_frontend_add_event_callback(decklink_frontend_event, filter);

	return filter;
}

static void decklink_output_filter_destroy(void *data)
{
	struct decklink_output_filter_context *filter = data;

	obs_frontend_remove_event_callback(decklink_frontend_event, filter);

	decklink_output_filter_stop(filter);

	signal_handler_t *sh = obs_source_get_signal_handler(filter->source);
	signal_handler_disconnect(sh, "enable", set_filter_enabled, filter);

	bfree(filter);
}

static bool button_cb(obs_properties_t *properties, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(properties);

	struct decklink_output_filter_context *filter = data;

	obs_data_t *settings = obs_source_get_settings(filter->source);

	obs_property_set_enabled(property, false);

	if (!filter->active)
		decklink_output_filter_start(filter, settings);
	else
		decklink_output_filter_stop(filter);

	obs_property_set_enabled(property, true);

	obs_data_release(settings);

	return true;
}

static obs_properties_t *decklink_output_filter_properties(void *data)
{
	struct decklink_output_filter_context *filter = data;

	obs_properties_t *props = obs_get_output_properties("decklink_output");
	obs_properties_add_bool(props, "auto_start", obs_module_text("AutoStart"));
	obs_properties_add_bool(props, "mute_audio", obs_module_text("MuteAudio"));
	obs_properties_add_button2(props, "Button",
				   filter->active ? obs_module_text("Stop") : obs_module_text("Start"), button_cb,
				   filter);

	return props;
}

struct obs_source_info decklink_output_filter = {.id = "decklink_output_filter",
						 .type = OBS_SOURCE_TYPE_FILTER,
						 .output_flags = OBS_SOURCE_VIDEO,
						 .get_name = decklink_output_filter_get_name,
						 .create = decklink_output_filter_create,
						 .destroy = decklink_output_filter_destroy,
						 .update = decklink_output_filter_update,
						 .get_properties = decklink_output_filter_properties,};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("decklink-output-filter", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Decklink Output Filter";
}

bool obs_module_load(void)
{
	return true;
}

void obs_module_post_load(void)
{
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	obs_register_source(&decklink_output_filter);
}
