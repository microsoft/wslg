/* SPDX-FileCopyrightText: Copyright (c) Microsoft Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <spa/param/audio/format-utils.h>
#include <spa/param/audio/raw.h>
#include <spa/pod/builder.h>

#include <pipewire/impl.h>

static const struct pw_proxy_events core_proxy_events = {
	PW_VERSION_PROXY_EVENTS,
};

#define NAME "wslg-rdp"

PW_LOG_TOPIC_STATIC(mod_topic, "mod." NAME);
#define PW_LOG_TOPIC_DEFAULT mod_topic

static const char *const pulse_module_options =
	"socket=<path to unix socket> "
	"sink_name=<name for the sink> "
	"sink_properties=<properties for the sink> "
	"source_name=<name for the source> "
	"source_properties=<properties for the source> "
	"format=<sample format> "
	"rate=<sample rate> "
	"channels=<number of channels> "
	"channel_map=<channel map> ";

static const struct spa_dict_item sink_module_props[] = {
	{ PW_KEY_MODULE_AUTHOR, "Microsoft" },
	{ PW_KEY_MODULE_DESCRIPTION, "WSLg RDP Sink" },
	{ PW_KEY_MODULE_USAGE, pulse_module_options },
	{ PW_KEY_MODULE_VERSION, "1.0" },
};

static const struct spa_dict_item source_module_props[] = {
	{ PW_KEY_MODULE_AUTHOR, "Microsoft" },
	{ PW_KEY_MODULE_DESCRIPTION, "WSLg RDP Source" },
	{ PW_KEY_MODULE_USAGE, pulse_module_options },
	{ PW_KEY_MODULE_VERSION, "1.0" },
};

#define DEFAULT_RATE 44100
#define DEFAULT_CHANNELS 2
#define DEFAULT_LATENCY_USEC 10000

typedef struct _rdp_audio_cmd_header
{
	uint32_t cmd;
	union {
		uint32_t version;
		struct {
			uint32_t bytes;
			uint64_t timestamp;
		} transfer;
		uint64_t reserved[8];
	};
} rdp_audio_cmd_header;

#define RDP_AUDIO_CMD_VERSION 0
#define RDP_AUDIO_CMD_TRANSFER 1
#define RDP_AUDIO_CMD_GET_LATENCY 2
#define RDP_AUDIO_CMD_RESET_LATENCY 3

#define RDP_SINK_INTERFACE_VERSION 1

static uint64_t rdp_audio_timestamp(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * 1000000ull + (uint64_t)tv.tv_usec;
}

static int send_all(int fd, const void *data, size_t bytes)
{
	const uint8_t *ptr = data;
	size_t sent = 0;
	while (sent < bytes) {
		ssize_t res = send(fd, ptr + sent, bytes - sent, 0);
		if (res <= 0)
			return -1;
		sent += (size_t)res;
	}
	return 0;
}

static int connect_unix_socket(const char *path)
{
	struct sockaddr_un addr;
	int fd = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static size_t calc_frame_size(const struct spa_audio_info_raw *info)
{
	size_t bytes_per_sample = 0;
	switch (info->format) {
	case SPA_AUDIO_FORMAT_S16:
	case SPA_AUDIO_FORMAT_S16_OE:
	case SPA_AUDIO_FORMAT_U16:
		bytes_per_sample = 2;
		break;
	case SPA_AUDIO_FORMAT_S24:
	case SPA_AUDIO_FORMAT_S24_OE:
	case SPA_AUDIO_FORMAT_U24:
		bytes_per_sample = 3;
		break;
	case SPA_AUDIO_FORMAT_S24_32:
	case SPA_AUDIO_FORMAT_S24_32_OE:
	case SPA_AUDIO_FORMAT_S32:
	case SPA_AUDIO_FORMAT_S32_OE:
	case SPA_AUDIO_FORMAT_U32:
	case SPA_AUDIO_FORMAT_U32_OE:
	case SPA_AUDIO_FORMAT_F32:
	case SPA_AUDIO_FORMAT_F32_OE:
		bytes_per_sample = 4;
		break;
	case SPA_AUDIO_FORMAT_F64:
	case SPA_AUDIO_FORMAT_F64_OE:
		bytes_per_sample = 8;
		break;
	default:
		bytes_per_sample = 2;
		break;
	}
	return bytes_per_sample * info->channels;
}

struct rdp_sink {
	struct pw_context *context;
	struct pw_impl_module *module;
	struct pw_core *core;
	struct pw_stream *stream;
	struct spa_hook module_listener;
	struct spa_hook stream_listener;
	struct spa_hook core_listener;
	struct spa_hook core_proxy_listener;
	struct spa_audio_info_raw info;
	size_t frame_size;
	int fd;
	int remote_version;
	char *socket_path;
};

struct rdp_source {
	struct pw_context *context;
	struct pw_impl_module *module;
	struct pw_core *core;
	struct pw_stream *stream;
	struct spa_hook module_listener;
	struct spa_hook stream_listener;
	struct spa_hook core_listener;
	struct spa_hook core_proxy_listener;
	struct spa_audio_info_raw info;
	size_t frame_size;
	int fd;
	char *socket_path;
};

static void sink_module_destroy(void *data)
{
	struct rdp_sink *sink = data;
	if (sink->stream)
		pw_stream_destroy(sink->stream);
	if (sink->core)
		pw_core_disconnect(sink->core);
	if (sink->fd != -1)
		close(sink->fd);
	free(sink->socket_path);
	free(sink);
}

static void source_module_destroy(void *data)
{
	struct rdp_source *source = data;
	if (source->stream)
		pw_stream_destroy(source->stream);
	if (source->core)
		pw_core_disconnect(source->core);
	if (source->fd != -1)
		close(source->fd);
	free(source->socket_path);
	free(source);
}

static const struct pw_impl_module_events sink_module_events = {
	PW_VERSION_IMPL_MODULE_EVENTS,
	.destroy = sink_module_destroy,
};

static const struct pw_impl_module_events source_module_events = {
	PW_VERSION_IMPL_MODULE_EVENTS,
	.destroy = source_module_destroy,
};

static void stream_sink_destroy(void *data)
{
	struct rdp_sink *sink = data;
	spa_hook_remove(&sink->stream_listener);
	sink->stream = NULL;
}

static void stream_source_destroy(void *data)
{
	struct rdp_source *source = data;
	spa_hook_remove(&source->stream_listener);
	source->stream = NULL;
}

static void sink_core_error(void *data, uint32_t id, int seq, int res, const char *message)
{
	struct rdp_sink *sink = data;
	pw_log_error("rdp sink core error: %s", message);
	if (id == PW_ID_CORE && res == -EPIPE)
		pw_impl_module_schedule_destroy(sink->module);
}

static void source_core_error(void *data, uint32_t id, int seq, int res, const char *message)
{
	struct rdp_source *source = data;
	pw_log_error("rdp source core error: %s", message);
	if (id == PW_ID_CORE && res == -EPIPE)
		pw_impl_module_schedule_destroy(source->module);
}

static const struct pw_core_events sink_core_events = {
	PW_VERSION_CORE_EVENTS,
	.error = sink_core_error,
};

static const struct pw_core_events source_core_events = {
	PW_VERSION_CORE_EVENTS,
	.error = source_core_error,
};

static int rdp_sink_connect(struct rdp_sink *sink)
{
	if (sink->fd != -1)
		return 0;

	sink->fd = connect_unix_socket(sink->socket_path);
	if (sink->fd < 0)
		return -1;

	rdp_audio_cmd_header header = {0};
	header.cmd = RDP_AUDIO_CMD_VERSION;
	header.version = RDP_SINK_INTERFACE_VERSION;
	if (send_all(sink->fd, &header, sizeof(header)) < 0)
		return -1;

	if (read(sink->fd, &sink->remote_version, sizeof(sink->remote_version)) != sizeof(sink->remote_version)) {
		close(sink->fd);
		sink->fd = -1;
		return -1;
	}
	return 0;
}

static void sink_stream_process(void *data)
{
	struct rdp_sink *sink = data;
	struct pw_buffer *buf;
	struct spa_data *bd;
	uint32_t size;

	if (rdp_sink_connect(sink) < 0)
		return;

	if ((buf = pw_stream_dequeue_buffer(sink->stream)) == NULL)
		return;

	bd = &buf->buffer->datas[0];
	size = SPA_MIN(bd->chunk->size, bd->maxsize);
	if (size > 0 && bd->data) {
		rdp_audio_cmd_header header = {0};
		header.cmd = RDP_AUDIO_CMD_TRANSFER;
		header.transfer.bytes = size;
		header.transfer.timestamp = rdp_audio_timestamp();
		if (send_all(sink->fd, &header, sizeof(header)) < 0 ||
		    send_all(sink->fd, SPA_PTROFF(bd->data, bd->chunk->offset, void), size) < 0) {
			close(sink->fd);
			sink->fd = -1;
		}
	}

	pw_stream_queue_buffer(sink->stream, buf);
}

static const struct pw_stream_events sink_stream_events = {
	PW_VERSION_STREAM_EVENTS,
	.destroy = stream_sink_destroy,
	.process = sink_stream_process,
};

static int create_sink_stream(struct rdp_sink *sink)
{
	uint32_t n_params = 0;
	const struct spa_pod *params[1];
	uint8_t buffer[256];
	struct spa_pod_builder b;

	sink->stream = pw_stream_new(sink->core, "wslg-rdp-sink", NULL);
	if (sink->stream == NULL)
		return -errno;

	pw_stream_add_listener(sink->stream, &sink->stream_listener, &sink_stream_events, sink);

	spa_pod_builder_init(&b, buffer, sizeof(buffer));
	params[n_params++] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &sink->info);

	return pw_stream_connect(sink->stream,
		PW_DIRECTION_INPUT,
		PW_ID_ANY,
		PW_STREAM_FLAG_AUTOCONNECT |
		PW_STREAM_FLAG_MAP_BUFFERS |
		PW_STREAM_FLAG_RT_PROCESS,
		params, n_params);
}

static int rdp_source_connect(struct rdp_source *source)
{
	if (source->fd != -1)
		return 0;

	source->fd = connect_unix_socket(source->socket_path);
	if (source->fd < 0)
		return -1;

	return 0;
}

static void source_stream_process(void *data)
{
	struct rdp_source *source = data;
	struct pw_buffer *buf;
	struct spa_data *bd;
	uint32_t size;
	ssize_t bytes_read;

	if (rdp_source_connect(source) < 0)
		return;

	if ((buf = pw_stream_dequeue_buffer(source->stream)) == NULL)
		return;

	bd = &buf->buffer->datas[0];
	size = SPA_MIN(bd->maxsize, bd->chunk->size);
	if (size == 0)
		size = bd->maxsize;

	bytes_read = read(source->fd, bd->data, size);
	if (bytes_read <= 0) {
		close(source->fd);
		source->fd = -1;
		pw_stream_queue_buffer(source->stream, buf);
		return;
	}

	bd->chunk->size = (uint32_t)bytes_read;
	bd->chunk->stride = source->frame_size;
	bd->chunk->offset = 0;

	pw_stream_queue_buffer(source->stream, buf);
}

static const struct pw_stream_events source_stream_events = {
	PW_VERSION_STREAM_EVENTS,
	.destroy = stream_source_destroy,
	.process = source_stream_process,
};

static int create_source_stream(struct rdp_source *source)
{
	uint32_t n_params = 0;
	const struct spa_pod *params[1];
	uint8_t buffer[256];
	struct spa_pod_builder b;

	source->stream = pw_stream_new(source->core, "wslg-rdp-source", NULL);
	if (source->stream == NULL)
		return -errno;

	pw_stream_add_listener(source->stream, &source->stream_listener, &source_stream_events, source);

	spa_pod_builder_init(&b, buffer, sizeof(buffer));
	params[n_params++] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &source->info);

	return pw_stream_connect(source->stream,
		PW_DIRECTION_OUTPUT,
		PW_ID_ANY,
		PW_STREAM_FLAG_AUTOCONNECT |
		PW_STREAM_FLAG_MAP_BUFFERS |
		PW_STREAM_FLAG_RT_PROCESS,
		params, n_params);
}

static int parse_audio_info(struct spa_audio_info_raw *info, struct pw_properties *props)
{
	const char *str;
	info->format = SPA_AUDIO_FORMAT_S16;
	info->rate = DEFAULT_RATE;
	info->channels = DEFAULT_CHANNELS;
	info->position[0] = SPA_AUDIO_CHANNEL_FL;
	info->position[1] = SPA_AUDIO_CHANNEL_FR;

	str = pw_properties_get(props, "rate");
	if (str)
		info->rate = (uint32_t)atoi(str);
	str = pw_properties_get(props, "channels");
	if (str)
		info->channels = (uint32_t)atoi(str);
	if (info->channels == 1) {
		info->position[0] = SPA_AUDIO_CHANNEL_MONO;
		info->position[1] = SPA_AUDIO_CHANNEL_UNKNOWN;
	}
	return 0;
}

static int init_sink(struct pw_impl_module *module, const char *args)
{
	struct pw_context *context = pw_impl_module_get_context(module);
	struct pw_properties *props = pw_properties_new_string(args ? args : "");
	const char *sink_props = pw_properties_get(props, "sink_properties");
	const char *socket_path = pw_properties_get(props, "socket");
	struct rdp_sink *sink;
	int res;

	PW_LOG_TOPIC_INIT(mod_topic);

	sink = calloc(1, sizeof(*sink));
	if (sink == NULL) {
		return -errno;
	}

	sink->context = context;
	sink->module = module;
	sink->fd = -1;
	sink->socket_path = strdup(socket_path ? socket_path : "/mnt/wslg/PulseAudioRDPSink");

	res = parse_audio_info(&sink->info, props);
	if (res < 0)
		goto error;

	sink->frame_size = calc_frame_size(&sink->info);

	sink->core = pw_context_get_object(context, PW_TYPE_INTERFACE_Core);
	if (sink->core == NULL) {
		sink->core = pw_context_connect(context, NULL, 0);
	}
	if (sink->core == NULL) {
		res = -errno;
		goto error;
	}

	pw_proxy_add_listener((struct pw_proxy*)sink->core, &sink->core_proxy_listener, &core_proxy_events, sink);
	pw_core_add_listener(sink->core, &sink->core_listener, &sink_core_events, sink);

	res = create_sink_stream(sink);
	if (res < 0)
		goto error;

	if (sink_props) {
		struct pw_properties *update_props = pw_properties_new_string(sink_props);
		if (update_props)
			pw_stream_update_properties(sink->stream, update_props);
	}

	pw_impl_module_add_listener(module, &sink->module_listener, &sink_module_events, sink);
	pw_impl_module_update_properties(module, &SPA_DICT_INIT_ARRAY(sink_module_props));
	pw_impl_module_update_properties(module, &props->dict);
	pw_properties_free(props);
	return 0;

error:
	pw_properties_free(props);
	sink_module_destroy(sink);
	return res;
}

static int init_source(struct pw_impl_module *module, const char *args)
{
	struct pw_context *context = pw_impl_module_get_context(module);
	struct pw_properties *props = pw_properties_new_string(args ? args : "");
	const char *source_props = pw_properties_get(props, "source_properties");
	const char *socket_path = pw_properties_get(props, "socket");
	struct rdp_source *source;
	int res;

	PW_LOG_TOPIC_INIT(mod_topic);

	source = calloc(1, sizeof(*source));
	if (source == NULL) {
		return -errno;
	}

	source->context = context;
	source->module = module;
	source->fd = -1;
	source->socket_path = strdup(socket_path ? socket_path : "/mnt/wslg/PulseAudioRDPSource");

	res = parse_audio_info(&source->info, props);
	if (res < 0)
		goto error;

	source->frame_size = calc_frame_size(&source->info);

	source->core = pw_context_get_object(context, PW_TYPE_INTERFACE_Core);
	if (source->core == NULL) {
		source->core = pw_context_connect(context, NULL, 0);
	}
	if (source->core == NULL) {
		res = -errno;
		goto error;
	}

	pw_proxy_add_listener((struct pw_proxy*)source->core, &source->core_proxy_listener, &core_proxy_events, source);
	pw_core_add_listener(source->core, &source->core_listener, &source_core_events, source);

	res = create_source_stream(source);
	if (res < 0)
		goto error;

	if (source_props) {
		struct pw_properties *update_props = pw_properties_new_string(source_props);
		if (update_props)
			pw_stream_update_properties(source->stream, update_props);
	}

	pw_impl_module_add_listener(module, &source->module_listener, &source_module_events, source);
	pw_impl_module_update_properties(module, &SPA_DICT_INIT_ARRAY(source_module_props));
	pw_impl_module_update_properties(module, &props->dict);
	pw_properties_free(props);
	return 0;

error:
	pw_properties_free(props);
	source_module_destroy(source);
	return res;
}

SPA_EXPORT
int pipewire__module_init(struct pw_impl_module *module, const char *args)
{
#ifdef WSLG_RDP_MODE_SOURCE
	return init_source(module, args);
#else
	return init_sink(module, args);
#endif
}
