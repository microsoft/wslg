/* SPDX-FileCopyrightText: Copyright (c) Microsoft Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <spa/param/audio/format-utils.h>
#include <spa/param/audio/raw.h>
#include <spa/param/latency-utils.h>
#include <spa/node/command.h>
#include <spa/pod/builder.h>
#include <spa/utils/ringbuffer.h>

#include <pipewire/impl.h>

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

static int send_all_worker(int fd, const void *data, size_t bytes)
{
	const uint8_t *ptr = data;
	size_t sent = 0;
	while (sent < bytes) {
		/* Avoid SIGPIPE; caller handles reconnect. */
		ssize_t res = send(fd, ptr + sent, bytes - sent, MSG_NOSIGNAL);
		if (res < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (res == 0)
			return -1;
		sent += (size_t)res;
	}
	return 0;
}

static int set_nonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void sleep_ms(int ms)
{
	struct timespec ts = {
		.tv_sec = ms / 1000,
		.tv_nsec = (long)(ms % 1000) * 1000000L,
	};
	nanosleep(&ts, NULL);
}

/* Lock-free byte ring between the RT process callback (producer) and the
 * socket worker thread (consumer). Size must be a power of two. */
#define RDP_SINK_RING_SIZE (1u << 20)
#define RDP_SOURCE_RING_SIZE (1u << 20)
#define RDP_WORKER_DRAIN_MAX (64u * 1024)
#define RDP_LATENCY_REFRESH_USEC (2u * 1000u * 1000u)

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
	bool core_owned;
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
	/* Reconnect state: after a PipeWire daemon restart the module has to
	 * recreate its core connection and stream. The core error handler only
	 * raises the flag; the timer (main loop context) does the work. */
	struct pw_loop *loop;
	struct spa_source *reconnect_timer;
	struct pw_properties *saved_node_props;
	char *node_name;
	bool reconnect_requested;
	bool reconnect_warned;

	/* Socket I/O runs on a worker thread; the RT process callback only
	 * enqueues audio into the lock-free ring, never touching the socket. */
	pthread_t worker;
	bool worker_running;
	bool worker_started;
	uint8_t *ring_mem;
	uint32_t ring_size;
	struct spa_ringbuffer ring;
	/* Guards latency/flag state below; never taken in the RT callback,
	 * which uses atomics on the same fields. */
	pthread_mutex_t state_lock;
	uint32_t transport_latency_us;
	uint32_t latency_seq;
	uint32_t published_seq;
	bool latency_valid;
	bool reset_requested;
};

struct rdp_source {
	struct pw_context *context;
	struct pw_impl_module *module;
	struct pw_core *core;
	bool core_owned;
	struct pw_stream *stream;
	struct spa_hook module_listener;
	struct spa_hook stream_listener;
	struct spa_hook core_listener;
	struct spa_hook core_proxy_listener;
	struct spa_audio_info_raw info;
	size_t frame_size;
	int fd;
	char *socket_path;
	/* Reconnect state: after a PipeWire daemon restart the module has to
	 * recreate its core connection and stream. The core error handler only
	 * raises the flag; the timer (main loop context) does the work. */
	struct pw_loop *loop;
	struct spa_source *reconnect_timer;
	struct pw_properties *saved_node_props;
	char *node_name;
	bool reconnect_requested;
	bool reconnect_warned;
	/* Socket I/O runs on a worker thread; the RT process callback only
	 * consumes samples from the lock-free ring, never touching the socket. */
	pthread_t worker;
	bool worker_running;
	bool worker_started;
	bool suspended;
	/* Bytes discarded by the overflow path, modulo the frame size: used
	 * to skip the partial frame once the ring drains again so the audio
	 * written to the ring stays sample-aligned. */
	uint32_t drop_phase;
	uint8_t *ring_mem;
	uint32_t ring_size;
	struct spa_ringbuffer ring;
	pthread_mutex_t state_lock;
	bool suspend_requested;
};

static void sink_module_destroy(void *data)
{
	struct rdp_sink *sink = data;
	/* Stop the worker first: it is the only socket user and the only
	 * producer of latency updates. */
	if (sink->worker_started) {
		__atomic_store_n(&sink->worker_running, false, __ATOMIC_RELEASE);
		pthread_join(sink->worker, NULL);
		sink->worker_started = false;
	}
	/* Stop the reconnect timer before tearing down core/stream. */
	if (sink->reconnect_timer) {
		pw_loop_destroy_source(sink->loop, sink->reconnect_timer);
		sink->reconnect_timer = NULL;
	}
	if (sink->stream)
		pw_stream_destroy(sink->stream);
	sink->stream = NULL;
	/* Disconnect only self-created cores; the context core may be shared. */
	if (sink->core && sink->core_owned)
		pw_core_disconnect(sink->core);
	sink->core = NULL;
	if (sink->fd != -1)
		close(sink->fd);
	free(sink->socket_path);
	free(sink->ring_mem);
	pw_properties_free(sink->saved_node_props);
	free(sink->node_name);
	pthread_mutex_destroy(&sink->state_lock);
	free(sink);
}

static void source_module_destroy(void *data)
{
	struct rdp_source *source = data;
	/* Stop the worker first: it is the only socket user. */
	if (source->worker_started) {
		__atomic_store_n(&source->worker_running, false, __ATOMIC_RELEASE);
		pthread_join(source->worker, NULL);
		source->worker_started = false;
	}
	/* Stop the reconnect timer before tearing down core/stream. */
	if (source->reconnect_timer) {
		pw_loop_destroy_source(source->loop, source->reconnect_timer);
		source->reconnect_timer = NULL;
	}
	if (source->stream)
		pw_stream_destroy(source->stream);
	source->stream = NULL;
	/* Disconnect only self-created cores; the context core may be shared. */
	if (source->core && source->core_owned)
		pw_core_disconnect(source->core);
	source->core = NULL;
	if (source->fd != -1)
		close(source->fd);
	free(source->socket_path);
	free(source->ring_mem);
	pw_properties_free(source->saved_node_props);
	free(source->node_name);
	pthread_mutex_destroy(&source->state_lock);
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
	if (id == PW_ID_CORE && res == -EPIPE) {
		/* The daemon went away. Do not tear down here (this runs inside
		 * event dispatch); the reconnect timer picks this up and rebuilds
		 * the core/stream once pipewire-0 returns. */
		sink->reconnect_requested = true;
	}
}

static void source_core_error(void *data, uint32_t id, int seq, int res, const char *message)
{
	struct rdp_source *source = data;
	pw_log_error("rdp source core error: %s", message);
	if (id == PW_ID_CORE && res == -EPIPE) {
		/* The daemon went away. Do not tear down here (this runs inside
		 * event dispatch); the reconnect timer picks this up and rebuilds
		 * the core/stream once pipewire-0 returns. */
		source->reconnect_requested = true;
	}
}

/* The core proxy can be torn down outside of our control (daemon side
 * teardown, error handling). Detach both listener hooks and drop the
 * pointer so the reconnect path never disconnects or reuses hooks of a
 * freed proxy. */
static void sink_core_destroy(void *data)
{
	struct rdp_sink *sink = data;

	spa_hook_remove(&sink->core_listener);
	spa_hook_remove(&sink->core_proxy_listener);
	if (sink->core != NULL) {
		sink->core = NULL;
		sink->core_owned = false;
		sink->reconnect_requested = true;
	}
}

static void source_core_destroy(void *data)
{
	struct rdp_source *source = data;

	spa_hook_remove(&source->core_listener);
	spa_hook_remove(&source->core_proxy_listener);
	if (source->core != NULL) {
		source->core = NULL;
		source->core_owned = false;
		source->reconnect_requested = true;
	}
}

static const struct pw_proxy_events sink_core_proxy_events = {
	PW_VERSION_PROXY_EVENTS,
	.destroy = sink_core_destroy,
};

static const struct pw_proxy_events source_core_proxy_events = {
	PW_VERSION_PROXY_EVENTS,
	.destroy = source_core_destroy,
};

static const struct pw_core_events sink_core_events = {
	PW_VERSION_CORE_EVENTS,
	.error = sink_core_error,
};

static const struct pw_core_events source_core_events = {
	PW_VERSION_CORE_EVENTS,
	.error = source_core_error,
};

static int rdp_sink_connect_worker(struct rdp_sink *sink)
{
	rdp_audio_cmd_header header = {0};
	struct timeval tv;
	int fd;

	if (sink->fd != -1)
		return 0;

	fd = connect_unix_socket(sink->socket_path);
	if (fd < 0)
		return -1;

	tv.tv_sec = 2;
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	header.cmd = RDP_AUDIO_CMD_VERSION;
	header.version = RDP_SINK_INTERFACE_VERSION;
	if (send_all_worker(fd, &header, sizeof(header)) < 0)
		goto fail;

	if (recv(fd, &sink->remote_version, sizeof(sink->remote_version), MSG_WAITALL) !=
	    (ssize_t)sizeof(sink->remote_version))
		goto fail;

	sink->fd = fd;
	return 0;
fail:
	close(fd);
	return -1;
}

static bool rdp_sink_query_latency(struct rdp_sink *sink, uint32_t *latency)
{
	rdp_audio_cmd_header header = {0};

	if (sink->fd == -1)
		return false;
	header.cmd = RDP_AUDIO_CMD_GET_LATENCY;
	if (send_all_worker(sink->fd, &header, sizeof(header)) < 0)
		return false;
	if (recv(sink->fd, latency, sizeof(*latency), MSG_WAITALL) != (ssize_t)sizeof(*latency))
		return false;
	return true;
}

static void sink_cache_latency(struct rdp_sink *sink, uint32_t latency_us)
{
	/* All accesses use atomics: the RT callback loads these fields
	 * concurrently, so plain stores would be a data race. */
	if (!__atomic_load_n(&sink->latency_valid, __ATOMIC_ACQUIRE) ||
	    __atomic_load_n(&sink->transport_latency_us, __ATOMIC_ACQUIRE) != latency_us) {
		__atomic_store_n(&sink->transport_latency_us, latency_us, __ATOMIC_RELEASE);
		__atomic_store_n(&sink->latency_valid, true, __ATOMIC_RELEASE);
		__atomic_add_fetch(&sink->latency_seq, 1, __ATOMIC_RELEASE);
	}
}

static void *sink_worker_thread(void *arg)
{
	struct rdp_sink *sink = arg;
	uint8_t *batch = malloc(RDP_WORKER_DRAIN_MAX);
	uint64_t last_query_us = 0;

	if (!batch)
		return NULL;

	while (__atomic_load_n(&sink->worker_running, __ATOMIC_ACQUIRE)) {
		int32_t avail;
		uint32_t ridx, n;
		bool do_reset = false;

		pthread_mutex_lock(&sink->state_lock);
		do_reset = sink->reset_requested;
		sink->reset_requested = false;
		pthread_mutex_unlock(&sink->state_lock);

		if (sink->fd == -1) {
			if (rdp_sink_connect_worker(sink) < 0) {
				sleep_ms(200);
				continue;
			}
			if (__atomic_load_n(&sink->worker_running, __ATOMIC_ACQUIRE)) {
				uint32_t lat = 0;
				if (rdp_sink_query_latency(sink, &lat))
					sink_cache_latency(sink, lat);
				last_query_us = rdp_audio_timestamp();
			}
		}

		if (do_reset) {
			if (sink->fd != -1) {
				rdp_audio_cmd_header hdr = {0};
				hdr.cmd = RDP_AUDIO_CMD_RESET_LATENCY;
				if (send_all_worker(sink->fd, &hdr, sizeof(hdr)) == 0)
					sink_cache_latency(sink, 0);
			} else {
				sink_cache_latency(sink, 0);
			}
		}

		avail = spa_ringbuffer_get_read_index(&sink->ring, &ridx);
		if (avail <= 0) {
			sleep_ms(5);
			continue;
		}
		n = (uint32_t)avail;
		if (n > RDP_WORKER_DRAIN_MAX)
			n = RDP_WORKER_DRAIN_MAX;
		if (sink->frame_size > 0)
			n -= n % (uint32_t)sink->frame_size;
		if (n == 0) {
			sleep_ms(5);
			continue;
		}
		spa_ringbuffer_read_data(&sink->ring, sink->ring_mem, sink->ring_size,
				ridx & (sink->ring_size - 1), batch, n);
		spa_ringbuffer_read_update(&sink->ring, (int32_t)(ridx + n));

		{
			rdp_audio_cmd_header hdr = {0};
			hdr.cmd = RDP_AUDIO_CMD_TRANSFER;
			hdr.transfer.bytes = n;
			hdr.transfer.timestamp = rdp_audio_timestamp();
			if (send_all_worker(sink->fd, &hdr, sizeof(hdr)) < 0 ||
			    send_all_worker(sink->fd, batch, n) < 0) {
				close(sink->fd);
				sink->fd = -1;
				sleep_ms(100);
				continue;
			}
		}

		if (rdp_audio_timestamp() - last_query_us > RDP_LATENCY_REFRESH_USEC) {
			uint32_t lat = 0;
			if (rdp_sink_query_latency(sink, &lat))
				sink_cache_latency(sink, lat);
			else {
				close(sink->fd);
				sink->fd = -1;
			}
			last_query_us = rdp_audio_timestamp();
		}
	}

	free(batch);
	return NULL;
}

static void sink_stream_process(void *data)
{
	struct rdp_sink *sink = data;
	struct pw_buffer *buf;
	struct spa_data *bd;
	uint32_t off, avail, size, widx;
	int32_t fill, free;

	/* RT path enqueues only; the worker thread owns the socket. */
	if ((buf = pw_stream_dequeue_buffer(sink->stream)) == NULL)
		return;

	/* Publish transport latency refreshed by the worker, if changed. */
	{
		uint32_t seq = __atomic_load_n(&sink->latency_seq, __ATOMIC_ACQUIRE);
		if (seq != sink->published_seq) {
			uint32_t us = __atomic_load_n(&sink->transport_latency_us, __ATOMIC_ACQUIRE);
			struct spa_latency_info info;
			uint8_t pbuf[256];
			struct spa_pod_builder b;
			const struct spa_pod *params[1];

			/* Report the transport delay in nanoseconds only. The
			 * quantum and rate fields are alternative units for the
			 * same delay, not additional amounts, so leaving them at
			 * zero avoids inflating the reported latency. */
			info = SPA_LATENCY_INFO(SPA_DIRECTION_INPUT,
					.min_ns = (int64_t)us * 1000,
					.max_ns = (int64_t)us * 1000);
			spa_pod_builder_init(&b, pbuf, sizeof(pbuf));
			params[0] = spa_latency_build(&b, SPA_PARAM_Latency, &info);
			if (params[0])
				pw_stream_update_params(sink->stream, params, 1);
			sink->published_seq = seq;
		}
	}

	bd = &buf->buffer->datas[0];
	/* Cap against bytes remaining after the offset, not the whole buffer. */
	off = bd->chunk->offset;
	avail = (bd->data != NULL && off < bd->maxsize) ? bd->maxsize - off : 0;
	size = SPA_MIN(bd->chunk->size, avail);
	if (size == 0 || sink->ring_mem == NULL) {
		pw_stream_queue_buffer(sink->stream, buf);
		return;
	}

	fill = spa_ringbuffer_get_write_index(&sink->ring, &widx);
	if (fill < 0)
		fill = 0;
	free = (fill >= (int32_t)sink->ring_size) ? 0 : (int32_t)(sink->ring_size - (uint32_t)fill);
	if (size > (uint32_t)free) {
		/* Overrun: drop this quantum to keep latency bounded. */
		pw_stream_queue_buffer(sink->stream, buf);
		return;
	}
	spa_ringbuffer_write_data(&sink->ring, sink->ring_mem, sink->ring_size,
			widx & (sink->ring_size - 1),
			SPA_PTROFF(bd->data, off, const void), size);
	spa_ringbuffer_write_update(&sink->ring, (int32_t)(widx + size));

	pw_stream_queue_buffer(sink->stream, buf);
}

static void sink_stream_command(void *data, const struct spa_command *command)
{
	struct rdp_sink *sink = data;
	uint32_t id = SPA_NODE_COMMAND_ID(command);

	/* Defer the RESET_LATENCY round-trip to the worker; only flag it here. */
	if (id == SPA_NODE_COMMAND_Suspend || id == SPA_NODE_COMMAND_Pause) {
		pthread_mutex_lock(&sink->state_lock);
		sink->reset_requested = true;
		pthread_mutex_unlock(&sink->state_lock);
	}
}

static const struct pw_stream_events sink_stream_events = {
	PW_VERSION_STREAM_EVENTS,
	.destroy = stream_sink_destroy,
	.process = sink_stream_process,
	.command = sink_stream_command,
};

static struct pw_properties *build_node_props(struct pw_properties *args,
	const char *legacy_name_key, const char *legacy_props_key,
	const char *default_name, const char *default_desc, const char *default_class)
{
	const char *direct_name = pw_properties_get(args, PW_KEY_NODE_NAME);
	const char *legacy_name = legacy_name_key ? pw_properties_get(args, legacy_name_key) : NULL;
	const char *name = direct_name ? direct_name : (legacy_name ? legacy_name : default_name);
	const char *direct_desc = pw_properties_get(args, PW_KEY_NODE_DESCRIPTION);
	const char *direct_class = pw_properties_get(args, PW_KEY_MEDIA_CLASS);

	struct pw_properties *node_props = pw_properties_new(
		PW_KEY_NODE_NAME, name,
		PW_KEY_NODE_DESCRIPTION, direct_desc ? direct_desc : default_desc,
		PW_KEY_MEDIA_CLASS, direct_class ? direct_class : default_class,
		NULL);
	if (!node_props)
		return NULL;

	/* Fold legacy "k=v k=v" properties string in. */
	const char *legacy = legacy_props_key ? pw_properties_get(args, legacy_props_key) : NULL;
	if (legacy) {
		struct pw_properties *over = pw_properties_new_string(legacy);
		if (over) {
			pw_properties_update(node_props, &over->dict);
			/* Keep canonical node name when legacy string lacks it. */
			pw_properties_set(node_props, PW_KEY_NODE_NAME, name);
			pw_properties_free(over);
		}
	}
	return node_props;
}

static int create_sink_stream(struct rdp_sink *sink, struct pw_properties *node_props, const char *name)
{
	uint32_t n_params = 0;
	const struct spa_pod *params[2];
	uint8_t buffer[512];
	struct spa_pod_builder b;
	/* Unknown transport latency until the worker queries Weston; the RT
	 * callback publishes updates via SPA_PARAM_Latency when known. */
	struct spa_latency_info latency = SPA_LATENCY_INFO(SPA_DIRECTION_INPUT);

	/* pw_stream_new takes ownership of node_props. */
	sink->stream = pw_stream_new(sink->core, name, node_props);
	if (sink->stream == NULL)
		return -errno;

	pw_stream_add_listener(sink->stream, &sink->stream_listener, &sink_stream_events, sink);

	spa_pod_builder_init(&b, buffer, sizeof(buffer));
	params[n_params++] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &sink->info);
	params[n_params++] = spa_latency_build(&b, SPA_PARAM_Latency, &latency);

	return pw_stream_connect(sink->stream,
		PW_DIRECTION_INPUT,
		PW_ID_ANY,
		PW_STREAM_FLAG_AUTOCONNECT |
		PW_STREAM_FLAG_MAP_BUFFERS |
		PW_STREAM_FLAG_RT_PROCESS,
		params, n_params);
}

/* Rebuild the core connection. A context core is shared when already
 * present; otherwise a private connection is created and owned. */
static int sink_connect_core(struct rdp_sink *sink, bool reuse_existing)
{
	if (sink->core != NULL)
		return 0;

	if (reuse_existing) {
		sink->core = pw_context_get_object(sink->context, PW_TYPE_INTERFACE_Core);
		if (sink->core != NULL) {
			pw_proxy_add_listener((struct pw_proxy*)sink->core, &sink->core_proxy_listener, &sink_core_proxy_events, sink);
			pw_core_add_listener(sink->core, &sink->core_listener, &sink_core_events, sink);
			return 0;
		}
	}

	sink->core = pw_context_connect(sink->context, NULL, 0);
	if (sink->core == NULL) {
		int res = -errno;
		return res < 0 ? res : -EIO;
	}
	sink->core_owned = true;
	pw_proxy_add_listener((struct pw_proxy*)sink->core, &sink->core_proxy_listener, &sink_core_proxy_events, sink);
	pw_core_add_listener(sink->core, &sink->core_listener, &sink_core_events, sink);
	return 0;
}

static int sink_rebuild_stream(struct rdp_sink *sink)
{
	struct pw_properties *node_props;

	node_props = sink->saved_node_props ?
		pw_properties_copy(sink->saved_node_props) :
		pw_properties_new(NULL, NULL);
	if (node_props == NULL)
		return errno ? -errno : -ENOMEM;

	return create_sink_stream(sink, node_props, sink->node_name);
}

/* Runs on the context main loop; safe place to tear down and recreate
 * objects (unlike the core error callback, which runs during dispatch). */
static void sink_reconnect_timer(void *data, uint64_t expirations)
{
	struct rdp_sink *sink = data;

	if (sink->reconnect_requested) {
		sink->reconnect_requested = false;
		pw_log_info("rdp sink: pipewire core went away, reconnecting");

		if (sink->stream) {
			pw_stream_destroy(sink->stream);
			sink->stream = NULL;
		}
		if (sink->core) {
			struct pw_core *core = sink->core;
			bool owned = sink->core_owned;

			/* Detach before disconnecting: the proxy destroy handler
			 * treats a core that is still installed as an unexpected
			 * teardown and would re-request a reconnect. */
			sink->core = NULL;
			sink->core_owned = false;
			if (owned)
				pw_core_disconnect(core);
			else {
				spa_hook_remove(&sink->core_listener);
				spa_hook_remove(&sink->core_proxy_listener);
			}
		}
		/* Make the fresh stream republish latency once known. */
		__atomic_store_n(&sink->latency_valid, false, __ATOMIC_RELEASE);
		sink->published_seq = 0;
	}

	if (sink->core == NULL) {
		int res = sink_connect_core(sink, false);
		if (res < 0) {
			if (!sink->reconnect_warned) {
				pw_log_warn("rdp sink: failed to reconnect to pipewire: %d", res);
				sink->reconnect_warned = true;
			}
			return;
		}
		sink->reconnect_warned = false;
		if (sink_rebuild_stream(sink) < 0) {
			if (sink->stream) {
				pw_stream_destroy(sink->stream);
				sink->stream = NULL;
			}
			if (sink->core_owned)
				pw_core_disconnect(sink->core);
			sink->core = NULL;
			sink->core_owned = false;
			return;
		}
		pw_log_info("rdp sink: reconnected to pipewire core");
	}
}

static void source_buffer_mark_empty(struct pw_buffer *buf)
{
	if (buf == NULL)
		return;
	/* pw_buffer::size is the number of valid frames queued; keep it
	 * consistent with the (empty) chunk. */
	buf->size = 0;
	if (buf->buffer && buf->buffer->datas && buf->buffer->datas[0].chunk) {
		buf->buffer->datas[0].chunk->size = 0;
		buf->buffer->datas[0].chunk->offset = 0;
		buf->buffer->datas[0].chunk->stride = 0;
	}
}

static void *source_worker_thread(void *arg)
{
	struct rdp_source *source = arg;
	uint8_t tmp[8192];

	while (__atomic_load_n(&source->worker_running, __ATOMIC_ACQUIRE)) {
		int32_t fill;
		uint32_t widx, free;
		struct pollfd pfd;
		bool susp = false;
		int r;

		pthread_mutex_lock(&source->state_lock);
		if (source->suspend_requested) {
			source->suspend_requested = false;
			source->suspended = true;
			/* Pause/Suspend: stop mic redirection and drop stale audio.
			 * The RT callback is idle while suspended, so resetting
			 * the ring here cannot race it. */
			if (source->fd != -1) {
				close(source->fd);
				source->fd = -1;
			}
			spa_ringbuffer_init(&source->ring);
		}
		susp = source->suspended;
		pthread_mutex_unlock(&source->state_lock);

		if (susp) {
			sleep_ms(200);
			continue;
		}

		if (source->fd == -1) {
			int fd = connect_unix_socket(source->socket_path);
			if (fd < 0) {
				sleep_ms(200);
				continue;
			}
			if (set_nonblocking(fd) < 0) {
				close(fd);
				sleep_ms(200);
				continue;
			}
			source->fd = fd;
			/* The peer starts a fresh stream on a frame boundary. */
			source->drop_phase = 0;
		}

		fill = spa_ringbuffer_get_write_index(&source->ring, &widx);
		if (fill < 0) {
			spa_ringbuffer_init(&source->ring);
			continue;
		}
		free = (fill >= (int32_t)source->ring_size) ? 0 : source->ring_size - (uint32_t)fill;

		pfd.fd = source->fd;
		pfd.events = POLLIN;
		r = poll(&pfd, 1, 50);
		if (r <= 0)
			continue;
		if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
			close(source->fd);
			source->fd = -1;
			continue;
		}
		if (!(pfd.revents & POLLIN))
			continue;

		if (free == 0) {
			/* Overrun with no capture client draining: recv and drop
			 * so the peer keeps flowing and latency stays bounded.
			 * Track the dropped byte count modulo the frame size so
			 * retained audio resumes on a frame boundary instead of
			 * mid-sample. */
			ssize_t n = recv(source->fd, tmp, sizeof(tmp), 0);
			if (n <= 0) {
				close(source->fd);
				source->fd = -1;
			} else if (source->frame_size > 0) {
				source->drop_phase = (source->drop_phase + (uint32_t)n) %
					(uint32_t)source->frame_size;
				sleep_ms(5);
			}
			continue;
		}

		{
			size_t want = free < sizeof(tmp) ? free : sizeof(tmp);
			ssize_t n = recv(source->fd, tmp, want, 0);
			if (n < 0) {
				if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
					close(source->fd);
					source->fd = -1;
				}
				continue;
			}
			if (n == 0) {
				close(source->fd);
				source->fd = -1;
				continue;
			}
			/* Skip the remainder of the partial frame dropped by the
			 * overflow path before writing to the ring. */
			if (source->drop_phase > 0 && source->frame_size > 0) {
				size_t frame_size = source->frame_size;
				size_t skip = frame_size - source->drop_phase;

				if ((size_t)n <= skip) {
					source->drop_phase = (source->drop_phase + (uint32_t)n) %
						(uint32_t)frame_size;
					continue;
				}
				memmove(tmp, tmp + skip, (size_t)n - skip);
				n -= (ssize_t)skip;
				source->drop_phase = 0;
			}
			spa_ringbuffer_write_data(&source->ring, source->ring_mem, source->ring_size,
					widx & (source->ring_size - 1), tmp, (uint32_t)n);
			spa_ringbuffer_write_update(&source->ring, (int32_t)(widx + (uint32_t)n));
		}
	}

	return NULL;
}

static void source_stream_process(void *data)
{
	struct rdp_source *source = data;
	struct pw_buffer *buf;
	struct spa_data *bd;
	size_t capacity;
	size_t frame_size;
	int32_t avail;
	uint32_t ridx, n;

	/* RT path consumes only; the worker thread owns the socket. */
	if ((buf = pw_stream_dequeue_buffer(source->stream)) == NULL)
		return;

	bd = &buf->buffer->datas[0];
	frame_size = source->frame_size ? source->frame_size : 1;

	/* Size from requested frames, frame-aligned; ignore stale chunk size. */
	if (buf->requested > 0)
		capacity = (size_t)buf->requested * frame_size;
	else
		capacity = bd->maxsize;
	capacity -= capacity % frame_size;
	if (capacity > bd->maxsize)
		capacity = bd->maxsize - (bd->maxsize % frame_size);
	if (capacity == 0 || bd->data == NULL || source->ring_mem == NULL) {
		source_buffer_mark_empty(buf);
		pw_stream_queue_buffer(source->stream, buf);
		return;
	}

	/* Never wait for microphone data; publish silence when none is staged. */
	avail = spa_ringbuffer_get_read_index(&source->ring, &ridx);
	if (avail <= 0) {
		source_buffer_mark_empty(buf);
		pw_stream_queue_buffer(source->stream, buf);
		return;
	}
	n = (uint32_t)avail;
	if (n > capacity)
		n = (uint32_t)capacity;
	n -= n % (uint32_t)frame_size;
	if (n == 0) {
		source_buffer_mark_empty(buf);
		pw_stream_queue_buffer(source->stream, buf);
		return;
	}
	spa_ringbuffer_read_data(&source->ring, source->ring_mem, source->ring_size,
			ridx & (source->ring_size - 1), bd->data, n);
	spa_ringbuffer_read_update(&source->ring, (int32_t)(ridx + n));

	bd->chunk->size = n;
	bd->chunk->stride = source->frame_size;
	bd->chunk->offset = 0;
	/* pw_buffer::size is the queued frame count used for buffer timing. */
	buf->size = n / frame_size;

	pw_stream_queue_buffer(source->stream, buf);
}

static void source_stream_command(void *data, const struct spa_command *command)
{
	struct rdp_source *source = data;
	uint32_t id = SPA_NODE_COMMAND_ID(command);

	/* Defer socket teardown to the worker; only flag it here. */
	if (id == SPA_NODE_COMMAND_Suspend || id == SPA_NODE_COMMAND_Pause) {
		pthread_mutex_lock(&source->state_lock);
		source->suspend_requested = true;
		pthread_mutex_unlock(&source->state_lock);
	} else if (id == SPA_NODE_COMMAND_Start) {
		pthread_mutex_lock(&source->state_lock);
		/* A rapid Pause->Start may race the worker: drop any suspend
		 * that has not been consumed yet, otherwise the worker could
		 * still park the stream after it has been started again. */
		source->suspend_requested = false;
		source->suspended = false;
		pthread_mutex_unlock(&source->state_lock);
	}
}

static const struct pw_stream_events source_stream_events = {
	PW_VERSION_STREAM_EVENTS,
	.destroy = stream_source_destroy,
	.process = source_stream_process,
	.command = source_stream_command,
};

static int create_source_stream(struct rdp_source *source, struct pw_properties *node_props, const char *name)
{
	uint32_t n_params = 0;
	const struct spa_pod *params[1];
	uint8_t buffer[256];
	struct spa_pod_builder b;

	/* pw_stream_new takes ownership of node_props. */
	source->stream = pw_stream_new(source->core, name, node_props);
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

/* Rebuild the core connection. A context core is shared when already
 * present; otherwise a private connection is created and owned. */
static int source_connect_core(struct rdp_source *source, bool reuse_existing)
{
	if (source->core != NULL)
		return 0;

	if (reuse_existing) {
		source->core = pw_context_get_object(source->context, PW_TYPE_INTERFACE_Core);
		if (source->core != NULL) {
			pw_proxy_add_listener((struct pw_proxy*)source->core, &source->core_proxy_listener, &source_core_proxy_events, source);
			pw_core_add_listener(source->core, &source->core_listener, &source_core_events, source);
			return 0;
		}
	}

	source->core = pw_context_connect(source->context, NULL, 0);
	if (source->core == NULL) {
		int res = -errno;
		return res < 0 ? res : -EIO;
	}
	source->core_owned = true;
	pw_proxy_add_listener((struct pw_proxy*)source->core, &source->core_proxy_listener, &source_core_proxy_events, source);
	pw_core_add_listener(source->core, &source->core_listener, &source_core_events, source);
	return 0;
}

static int source_rebuild_stream(struct rdp_source *source)
{
	struct pw_properties *node_props;

	node_props = source->saved_node_props ?
		pw_properties_copy(source->saved_node_props) :
		pw_properties_new(NULL, NULL);
	if (node_props == NULL)
		return errno ? -errno : -ENOMEM;

	return create_source_stream(source, node_props, source->node_name);
}

/* Runs on the context main loop; safe place to tear down and recreate
 * objects (unlike the core error callback, which runs during dispatch). */
static void source_reconnect_timer(void *data, uint64_t expirations)
{
	struct rdp_source *source = data;

	if (source->reconnect_requested) {
		source->reconnect_requested = false;
		pw_log_info("rdp source: pipewire core went away, reconnecting");

		if (source->stream) {
			pw_stream_destroy(source->stream);
			source->stream = NULL;
		}
		if (source->core) {
			struct pw_core *core = source->core;
			bool owned = source->core_owned;

			/* Detach before disconnecting: the proxy destroy handler
			 * treats a core that is still installed as an unexpected
			 * teardown and would re-request a reconnect. */
			source->core = NULL;
			source->core_owned = false;
			if (owned)
				pw_core_disconnect(core);
			else {
				spa_hook_remove(&source->core_listener);
				spa_hook_remove(&source->core_proxy_listener);
			}
		}
		/* The new stream starts fresh; do not let a suspend that was
		 * queued for the old stream keep the worker parked. */
		pthread_mutex_lock(&source->state_lock);
		source->suspend_requested = false;
		source->suspended = false;
		pthread_mutex_unlock(&source->state_lock);
	}

	if (source->core == NULL) {
		int res = source_connect_core(source, false);
		if (res < 0) {
			if (!source->reconnect_warned) {
				pw_log_warn("rdp source: failed to reconnect to pipewire: %d", res);
				source->reconnect_warned = true;
			}
			return;
		}
		source->reconnect_warned = false;
		if (source_rebuild_stream(source) < 0) {
			if (source->stream) {
				pw_stream_destroy(source->stream);
				source->stream = NULL;
			}
			if (source->core_owned)
				pw_core_disconnect(source->core);
			source->core = NULL;
			source->core_owned = false;
			return;
		}
		pw_log_info("rdp source: reconnected to pipewire core");
	}
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
	const char *socket_path = pw_properties_get(props, "socket");
	struct rdp_sink *sink;
	struct pw_properties *node_props;
	const char *node_name;
	int res;

	PW_LOG_TOPIC_INIT(mod_topic);

	sink = calloc(1, sizeof(*sink));
	if (sink == NULL) {
		pw_properties_free(props);
		return -errno;
	}

	sink->context = context;
	sink->module = module;
	sink->fd = -1;
	sink->socket_path = strdup(socket_path ? socket_path : "/mnt/wslg/PulseAudioRDPSink");
	pthread_mutex_init(&sink->state_lock, NULL);

	sink->ring_size = RDP_SINK_RING_SIZE;
	sink->ring_mem = malloc(sink->ring_size);
	if (sink->ring_mem == NULL) {
		res = -errno;
		goto error;
	}
	spa_ringbuffer_init(&sink->ring);

	res = parse_audio_info(&sink->info, props);
	if (res < 0)
		goto error;

	sink->frame_size = calc_frame_size(&sink->info);

	/* Set device identity before connect. */
	node_props = build_node_props(props, "sink_name", "sink_properties",
		"RDPSink", "WSLg RDP Sink", "Audio/Sink");
	if (!node_props) {
		res = -errno;
		goto error;
	}
	node_name = pw_properties_get(node_props, PW_KEY_NODE_NAME);

	/* Keep a copy: the original is handed to the stream, and the reconnect
	 * path needs enough state to rebuild the stream from scratch. */
	sink->saved_node_props = pw_properties_copy(node_props);
	sink->node_name = node_name ? strdup(node_name) : NULL;
	if (sink->saved_node_props == NULL || sink->node_name == NULL) {
		res = -errno;
		pw_properties_free(node_props);
		goto error;
	}

	res = sink_connect_core(sink, true);
	if (res < 0) {
		pw_properties_free(node_props);
		goto error;
	}

	res = create_sink_stream(sink, node_props, node_name);
	if (res < 0)
		goto error;

	/* Socket I/O lives on the worker; the RT callback only enqueues. */
	__atomic_store_n(&sink->worker_running, true, __ATOMIC_RELEASE);
	/* pthread_create returns its error directly; errno is untouched. */
	res = -pthread_create(&sink->worker, NULL, sink_worker_thread, sink);
	if (res != 0) {
		__atomic_store_n(&sink->worker_running, false, __ATOMIC_RELEASE);
		goto error;
	}
	sink->worker_started = true;

	/* Periodic watchdog that rebuilds the core/stream after a pipewire
	 * daemon restart, when pipewire-pulse itself stayed alive. */
	sink->loop = pw_context_get_main_loop(context);
	sink->reconnect_timer = pw_loop_add_timer(sink->loop, sink_reconnect_timer, sink);
	if (sink->reconnect_timer == NULL) {
		res = -errno;
		goto error;
	}
	{
		struct timespec value = { .tv_sec = 1, .tv_nsec = 0 };
		struct timespec interval = { .tv_sec = 1, .tv_nsec = 0 };
		pw_loop_update_timer(sink->loop, sink->reconnect_timer, &value, &interval, false);
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
	const char *socket_path = pw_properties_get(props, "socket");
	struct rdp_source *source;
	struct pw_properties *node_props;
	const char *node_name;
	int res;

	PW_LOG_TOPIC_INIT(mod_topic);

	source = calloc(1, sizeof(*source));
	if (source == NULL) {
		pw_properties_free(props);
		return -errno;
	}

	source->context = context;
	source->module = module;
	source->fd = -1;
	source->socket_path = strdup(socket_path ? socket_path : "/mnt/wslg/PulseAudioRDPSource");
	pthread_mutex_init(&source->state_lock, NULL);

	source->ring_size = RDP_SOURCE_RING_SIZE;
	source->ring_mem = malloc(source->ring_size);
	if (source->ring_mem == NULL) {
		res = -errno;
		goto error;
	}
	spa_ringbuffer_init(&source->ring);

	res = parse_audio_info(&source->info, props);
	if (res < 0)
		goto error;

	source->frame_size = calc_frame_size(&source->info);

	/* Set device identity before connect. */
	node_props = build_node_props(props, "source_name", "source_properties",
		"RDPSource", "WSLg RDP Source", "Audio/Source");
	if (!node_props) {
		res = -errno;
		goto error;
	}
	node_name = pw_properties_get(node_props, PW_KEY_NODE_NAME);

	/* Keep a copy: the original is handed to the stream, and the reconnect
	 * path needs enough state to rebuild the stream from scratch. */
	source->saved_node_props = pw_properties_copy(node_props);
	source->node_name = node_name ? strdup(node_name) : NULL;
	if (source->saved_node_props == NULL || source->node_name == NULL) {
		res = -errno;
		pw_properties_free(node_props);
		goto error;
	}

	res = source_connect_core(source, true);
	if (res < 0) {
		pw_properties_free(node_props);
		goto error;
	}

	res = create_source_stream(source, node_props, node_name);
	if (res < 0)
		goto error;

	/* Socket I/O lives on the worker; the RT callback only consumes. */
	__atomic_store_n(&source->worker_running, true, __ATOMIC_RELEASE);
	/* pthread_create returns its error directly; errno is untouched. */
	res = -pthread_create(&source->worker, NULL, source_worker_thread, source);
	if (res != 0) {
		__atomic_store_n(&source->worker_running, false, __ATOMIC_RELEASE);
		goto error;
	}
	source->worker_started = true;

	/* Periodic watchdog that rebuilds the core/stream after a pipewire
	 * daemon restart, when pipewire-pulse itself stayed alive. */
	source->loop = pw_context_get_main_loop(context);
	source->reconnect_timer = pw_loop_add_timer(source->loop, source_reconnect_timer, source);
	if (source->reconnect_timer == NULL) {
		res = -errno;
		goto error;
	}
	{
		struct timespec value = { .tv_sec = 1, .tv_nsec = 0 };
		struct timespec interval = { .tv_sec = 1, .tv_nsec = 0 };
		pw_loop_update_timer(source->loop, source->reconnect_timer, &value, &interval, false);
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
