extern "C"
{
#include "rtmp-internal.h"
#include "rtmp-msgtypeid.h"
#include "rtmp-handshake.h"
#include "rtmp-netstream.h"
#include "rtmp-netconnection.h"
#include "rtmp-control-message.h"
#include "rtmp-event.h"
#include "rtmp-chunk-header.h"
}
#include "frame_buffer.h"
#include "rtmp_codec.h"
#include "rtmp_server_context.h"
#include <cstring>
#include <cassert>

#define RTMP_FMSVER "FMS/3,0,1,123"
#define RTMP_CAPABILITIES 31
#define RTMP_OUTPUT_CHUNK_SIZE 65535
#define RTMP_SERVER_ASYNC_START 0x12345678    // magic number, user call rtmp_server_start

enum
{
    RTMP_SERVER_ONPLAY    = 1,
    RTMP_SERVER_ONPUBLISH = 2
};

using simple_rtmp::rtmp_server_context_handler;
using simple_rtmp::rtmp_server_context;

struct simple_rtmp::rtmp_server_context_args
{
    struct rtmp_t rtmp;
    uint32_t recv_bytes[2];
    uint8_t payload[2 * 1024];
    uint8_t handshake[2 * RTMP_HANDSHAKE_SIZE + 1];
    size_t handshake_bytes;
    int handshake_state;
    struct rtmp_connect_t info;
    char stream_name[256];
    char stream_type[18];
    uint32_t stream_id;
    uint8_t receiveAudio;
    uint8_t receiveVideo;
    struct
    {
        double transaction;
        int reset;
        int play;    // RTMP_SERVER_ONPLAY/RTMP_SERVER_ONPUBLISH
    } start;
    rtmp_server_context_handler handler_;
    rtmp_server_context* ctx_;
    static int rtmp_server_send_onstatus(simple_rtmp::rtmp_server_context_args* ctx, double transaction, int r, const char* success, const char* fail, const char* description);
    static int rtmp_server_send_handshake(simple_rtmp::rtmp_server_context_args* ctx);
    static int rtmp_server_send_set_chunk_size(simple_rtmp::rtmp_server_context_args* ctx);
    static int rtmp_server_send_acknowledgement(simple_rtmp::rtmp_server_context_args* ctx, size_t size);
    static int rtmp_server_send_server_bandwidth(simple_rtmp::rtmp_server_context_args* ctx);
    static int rtmp_server_send_client_bandwidth(simple_rtmp::rtmp_server_context_args* ctx);
    static int rtmp_server_send_stream_is_record(simple_rtmp::rtmp_server_context_args* ctx);
    static int rtmp_server_send_stream_begin(simple_rtmp::rtmp_server_context_args* ctx);
    static int rtmp_server_rtmp_sample_access(simple_rtmp::rtmp_server_context_args* ctx);
    static void rtmp_server_onabort(void* param, uint32_t chunk_stream_id);
    static int rtmp_server_onconnect(void* param, int r, double transaction, const struct rtmp_connect_t* connect);
    static int rtmp_server_onaudio(void* param, const uint8_t* data, size_t bytes, uint32_t timestamp);
    static int rtmp_server_onvideo(void* param, const uint8_t* data, size_t bytes, uint32_t timestamp);
    static int rtmp_server_onscript(void* param, const uint8_t* data, size_t bytes, uint32_t timestamp);
    static int rtmp_server_oncreate_stream(void* param, int r, double transaction);
    static int rtmp_server_ondelete_stream(void* param, int r, double transaction, double stream_id);
    static int rtmp_server_onget_stream_length(void* param, int r, double transaction, const char* stream_name);
    static int rtmp_server_onpublish(void* param, int r, double transaction, const char* stream_name, const char* stream_type);
    static int rtmp_server_onplay(void* param, int r, double transaction, const char* stream_name, double start, double duration, uint8_t reset);
    static int rtmp_server_onpause(void* param, int r, double transaction, uint8_t pause, double milliSeconds);
    static int rtmp_server_onseek(void* param, int r, double transaction, double milliSeconds);
    static int rtmp_server_onreceive_audio(void* param, int r, double transaction, uint8_t audio);
    static int rtmp_server_onreceive_video(void* param, int r, double transaction, uint8_t video);
    static int rtmp_server_send(void* param, const uint8_t* header, uint32_t headerBytes, const uint8_t* payload, uint32_t payloadBytes);
    void init(rtmp_server_context* ctx, rtmp_server_context_handler handler);
};
static struct rtmp_packet_t* rtmp_packet_find_help(struct rtmp_t* rtmp, uint32_t cid)
{
    // The protocol supports up to 65597 streams with IDs 3-65599
    assert(cid <= 65535 + 64 && cid >= 2 /* Protocol Control Messages */);
    for (int i = 0; i < N_CHUNK_STREAM; i++)
    {
        struct rtmp_packet_t* pkt = rtmp->out_packets + ((i + cid) % N_CHUNK_STREAM);
        if (pkt->header.cid == cid)
        {
            return pkt;
        }
    }
    return nullptr;
}
static const struct rtmp_chunk_header_t* rtmp_chunk_header_zip_help(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header)
{
    struct rtmp_chunk_header_t h;

    assert(0 != header->cid && 1 != header->cid);
    assert(RTMP_CHUNK_TYPE_0 == header->fmt || RTMP_CHUNK_TYPE_1 == header->fmt);

    memcpy(&h, header, sizeof(h));

    // find previous chunk header
    struct rtmp_packet_t* pkt = rtmp_packet_find_help(rtmp, h.cid);
    if (nullptr == pkt)
    {
        // pkt = rtmp_packet_create(rtmp, h.cid);
        // if (NULL == pkt)
        //	return NULL; // too many chunk stream id
        assert(0);
        return NULL;    // can't find chunk stream id
    }

    h.fmt = RTMP_CHUNK_TYPE_0;
    if (RTMP_CHUNK_TYPE_0 != header->fmt             /* enable compress */
        && header->cid == pkt->header.cid            /* not the first packet */
        && header->timestamp >= pkt->clock           /* timestamp wrap */
        && header->timestamp - pkt->clock < 0xFFFFFF /* timestamp delta < 1 << 24 */
        && header->stream_id == pkt->header.stream_id /* message stream id */)
    {
        h.fmt = RTMP_CHUNK_TYPE_1;
        h.timestamp -= pkt->clock;    // timestamp delta

        if (header->type == pkt->header.type && header->length == pkt->header.length)
        {
            h.fmt = RTMP_CHUNK_TYPE_2;
            if (h.timestamp == pkt->header.timestamp)
                h.fmt = RTMP_CHUNK_TYPE_3;
        }
    }

    memcpy(&pkt->header, &h, sizeof(h));
    pkt->clock = header->timestamp;    // save timestamp
    return &pkt->header;
}

int rtmp_chunk_write_help(simple_rtmp::rtmp_server_context_args* args, const struct rtmp_chunk_header_t* h, const simple_rtmp::frame_buffer::ptr& frame)
{
    uint8_t p[MAX_CHUNK_HEADER] = {0};
    const struct rtmp_chunk_header_t* header;

    // compression rtmp chunk header
    header = rtmp_chunk_header_zip_help(&args->rtmp, h);
    if ((header == nullptr) || header->length >= 0xFFFFFF)
    {
        return -EINVAL;    // invalid length
    }
    uint32_t headerSize = rtmp_chunk_basic_header_write(p, header->fmt, header->cid);
    headerSize += rtmp_chunk_message_header_write(p + headerSize, header);
    if (header->timestamp >= 0xFFFFFF)
    {
        headerSize += rtmp_chunk_extended_timestamp_write(p + headerSize, header->timestamp);
    }

    const uint8_t* payload = frame->data();
    uint32_t payloadSize   = header->length;

    while (payloadSize > 0)
    {
        uint32_t chunkSize = std::min(payloadSize, args->rtmp.out_chunk_size);
        auto ref_frame     = simple_rtmp::ref_frame_buffer::create(payload, chunkSize, frame);
        auto header_frame  = simple_rtmp::fixed_frame_buffer::create(p, headerSize);
        args->handler_.send(header_frame);
        args->handler_.send(ref_frame);

        payload += chunkSize;
        payloadSize -= chunkSize;

        if (payloadSize > 0)
        {
            headerSize = rtmp_chunk_basic_header_write(p, RTMP_CHUNK_TYPE_3, header->cid);
            if (header->timestamp >= 0xFFFFFF)
            {
                headerSize += rtmp_chunk_extended_timestamp_write(p + headerSize, header->timestamp);
            }
        }
    }

    return 0;
}

void simple_rtmp::rtmp_server_context_args::init(rtmp_server_context* ctx, rtmp_server_context_handler handler)
{
    ctx_                                               = ctx;
    handler_                                           = handler;
    stream_id                                          = 0;
    receiveAudio                                       = 1;
    receiveVideo                                       = 1;
    handshake_state                                    = RTMP_HANDSHAKE_UNINIT;
    rtmp.parser.state                                  = RTMP_PARSE_INIT;
    rtmp.in_chunk_size                                 = RTMP_CHUNK_SIZE;
    rtmp.out_chunk_size                                = RTMP_CHUNK_SIZE;
    rtmp.window_size                                   = 5000000;
    rtmp.peer_bandwidth                                = 5000000;
    rtmp.buffer_length_ms                              = 30000;
    rtmp.param                                         = this;
    rtmp.send                                          = rtmp_server_send;
    rtmp.onaudio                                       = rtmp_server_onaudio;
    rtmp.onvideo                                       = rtmp_server_onvideo;
    rtmp.onabort                                       = rtmp_server_onabort;
    rtmp.onscript                                      = rtmp_server_onscript;
    rtmp.server.onconnect                              = rtmp_server_onconnect;
    rtmp.server.oncreate_stream                        = rtmp_server_oncreate_stream;
    rtmp.server.ondelete_stream                        = rtmp_server_ondelete_stream;
    rtmp.server.onget_stream_length                    = rtmp_server_onget_stream_length;
    rtmp.server.onpublish                              = rtmp_server_onpublish;
    rtmp.server.onplay                                 = rtmp_server_onplay;
    rtmp.server.onpause                                = rtmp_server_onpause;
    rtmp.server.onseek                                 = rtmp_server_onseek;
    rtmp.server.onreceive_audio                        = rtmp_server_onreceive_audio;
    rtmp.server.onreceive_video                        = rtmp_server_onreceive_video;
    rtmp.out_packets[RTMP_CHANNEL_PROTOCOL].header.cid = RTMP_CHANNEL_PROTOCOL;
    rtmp.out_packets[RTMP_CHANNEL_INVOKE].header.cid   = RTMP_CHANNEL_INVOKE;
    rtmp.out_packets[RTMP_CHANNEL_AUDIO].header.cid    = RTMP_CHANNEL_AUDIO;
    rtmp.out_packets[RTMP_CHANNEL_VIDEO].header.cid    = RTMP_CHANNEL_VIDEO;
    rtmp.out_packets[RTMP_CHANNEL_DATA].header.cid     = RTMP_CHANNEL_DATA;
}
static int rtmp_server_send_control(struct rtmp_t* rtmp, const uint8_t* payload, uint32_t bytes, uint32_t stream_id)
{
    struct rtmp_chunk_header_t header;
    header.fmt       = RTMP_CHUNK_TYPE_0;    // disable compact header
    header.cid       = RTMP_CHANNEL_INVOKE;
    header.timestamp = 0;
    header.length    = bytes;
    header.type      = RTMP_TYPE_INVOKE;
    header.stream_id = stream_id; /* default 0 */
    return rtmp_chunk_write(rtmp, &header, payload);
}

int simple_rtmp::rtmp_server_context_args::rtmp_server_send_onstatus(simple_rtmp::rtmp_server_context_args* ctx, double transaction, int r, const char* success, const char* fail, const char* description)
{
    r = (int)(rtmp_netstream_onstatus(ctx->payload, sizeof(ctx->payload), transaction, 0 == r ? RTMP_LEVEL_STATUS : RTMP_LEVEL_ERROR, 0 == r ? success : fail, description) - ctx->payload);
    return rtmp_server_send_control(&ctx->rtmp, ctx->payload, r, ctx->stream_id);
}

// handshake
int simple_rtmp::rtmp_server_context_args::rtmp_server_send_handshake(simple_rtmp::rtmp_server_context_args* ctx)
{
    int n = rtmp_handshake_s0(ctx->handshake, RTMP_VERSION);
    n += rtmp_handshake_s1(ctx->handshake + n, (uint32_t)time(NULL), ctx->payload, RTMP_HANDSHAKE_SIZE);
    n += rtmp_handshake_s2(ctx->handshake + n, (uint32_t)time(NULL), ctx->payload, RTMP_HANDSHAKE_SIZE);
    assert(n == 1 + RTMP_HANDSHAKE_SIZE + RTMP_HANDSHAKE_SIZE);
    auto frame = fixed_frame_buffer::create();
    frame->append(ctx->handshake, n);
    int r = ctx->handler_.send(frame);
    return n == r ? 0 : r;
}

/// 5.4.1. Set Chunk Size (1)
int simple_rtmp::rtmp_server_context_args::rtmp_server_send_set_chunk_size(simple_rtmp::rtmp_server_context_args* ctx)
{
    int n      = rtmp_set_chunk_size(ctx->payload, sizeof(ctx->payload), RTMP_OUTPUT_CHUNK_SIZE);
    auto frame = fixed_frame_buffer::create();
    frame->append(ctx->payload, n);
    int r                    = ctx->handler_.send(frame);
    ctx->rtmp.out_chunk_size = RTMP_OUTPUT_CHUNK_SIZE;
    return n == r ? 0 : r;
}

/// 5.4.3. Acknowledgement (3)
int simple_rtmp::rtmp_server_context_args::rtmp_server_send_acknowledgement(simple_rtmp::rtmp_server_context_args* ctx, size_t size)
{
    ctx->recv_bytes[0] += (uint32_t)size;
    if (ctx->rtmp.window_size && ctx->recv_bytes[0] - ctx->recv_bytes[1] > ctx->rtmp.window_size)
    {
        int n      = rtmp_acknowledgement(ctx->payload, sizeof(ctx->payload), ctx->recv_bytes[0]);
        auto frame = fixed_frame_buffer::create();
        frame->append(ctx->payload, n);
        int r              = ctx->handler_.send(frame);
        ctx->recv_bytes[1] = ctx->recv_bytes[0];
        return n == r ? 0 : r;
    }
    return 0;
}

/// 5.4.4. Window Acknowledgement Size (5)
int simple_rtmp::rtmp_server_context_args::rtmp_server_send_server_bandwidth(simple_rtmp::rtmp_server_context_args* ctx)
{
    int n      = rtmp_window_acknowledgement_size(ctx->payload, sizeof(ctx->payload), ctx->rtmp.window_size);
    auto frame = fixed_frame_buffer::create();
    frame->append(ctx->payload, n);
    int r = ctx->handler_.send(frame);
    return n == r ? 0 : r;
}

/// 5.4.5. Set Peer Bandwidth (6)
int simple_rtmp::rtmp_server_context_args::rtmp_server_send_client_bandwidth(simple_rtmp::rtmp_server_context_args* ctx)
{
    int n      = rtmp_set_peer_bandwidth(ctx->payload, sizeof(ctx->payload), ctx->rtmp.peer_bandwidth, RTMP_BANDWIDTH_LIMIT_DYNAMIC);
    auto frame = fixed_frame_buffer::create();
    frame->append(ctx->payload, n);
    int r = ctx->handler_.send(frame);
    return n == r ? 0 : r;
}

int simple_rtmp::rtmp_server_context_args::rtmp_server_send_stream_is_record(simple_rtmp::rtmp_server_context_args* ctx)
{
    int n      = rtmp_event_stream_is_record(ctx->payload, sizeof(ctx->payload), ctx->stream_id);
    auto frame = fixed_frame_buffer::create();
    frame->append(ctx->payload, n);
    int r = ctx->handler_.send(frame);
    return n == r ? 0 : r;
}

int simple_rtmp::rtmp_server_context_args::rtmp_server_send_stream_begin(simple_rtmp::rtmp_server_context_args* ctx)
{
    int n      = rtmp_event_stream_begin(ctx->payload, sizeof(ctx->payload), ctx->stream_id);
    auto frame = fixed_frame_buffer::create();
    frame->append(ctx->payload, n);
    int r = ctx->handler_.send(frame);
    return n == r ? 0 : r;
}

int simple_rtmp::rtmp_server_context_args::rtmp_server_rtmp_sample_access(simple_rtmp::rtmp_server_context_args* ctx)
{
    struct rtmp_chunk_header_t header;
    int n            = (int)(rtmp_netstream_rtmpsampleaccess(ctx->payload, sizeof(ctx->payload)) - ctx->payload);
    header.fmt       = RTMP_CHUNK_TYPE_0;    // disable compact header
    header.cid       = RTMP_CHANNEL_INVOKE;
    header.timestamp = 0;
    header.length    = n;
    header.type      = RTMP_TYPE_DATA;
    header.stream_id = ctx->stream_id;
    auto frame       = fixed_frame_buffer::create();
    frame->append(ctx->payload, n);
    return rtmp_chunk_write_help(ctx, &header, frame);
}

void simple_rtmp::rtmp_server_context_args::rtmp_server_onabort(void* param, uint32_t chunk_stream_id)
{
    simple_rtmp::rtmp_server_context_args* ctx = (simple_rtmp::rtmp_server_context_args*)param;
    (void)ctx, (void)chunk_stream_id;
    //	ctx->handler_.onerror -1, "client abort");
}

int simple_rtmp::rtmp_server_context_args::rtmp_server_onaudio(void* param, const uint8_t* data, size_t bytes, uint32_t timestamp)
{
    simple_rtmp::rtmp_server_context_args* ctx = (simple_rtmp::rtmp_server_context_args*)param;
    auto frame                                 = fixed_frame_buffer::create();
    frame->append(data, bytes);
    frame->set_pts(timestamp);
    frame->set_dts(timestamp);
    frame->set_codec(simple_rtmp::rtmp_tag::audio);
    return ctx->handler_.onaudio(frame);
}

int simple_rtmp::rtmp_server_context_args::rtmp_server_onvideo(void* param, const uint8_t* data, size_t bytes, uint32_t timestamp)
{
    simple_rtmp::rtmp_server_context_args* ctx = (simple_rtmp::rtmp_server_context_args*)param;
    auto frame                                 = fixed_frame_buffer::create();
    frame->append(data, bytes);
    frame->set_pts(timestamp);
    frame->set_dts(timestamp);
    frame->set_codec(simple_rtmp::rtmp_tag::video);

    return ctx->handler_.onvideo(frame);
}

int simple_rtmp::rtmp_server_context_args::rtmp_server_onscript(void* param, const uint8_t* data, size_t bytes, uint32_t timestamp)
{
    simple_rtmp::rtmp_server_context_args* ctx = (simple_rtmp::rtmp_server_context_args*)param;
    auto frame                                 = fixed_frame_buffer::create();
    frame->append(data, bytes);

    frame->set_pts(timestamp);
    frame->set_dts(timestamp);
    frame->set_codec(simple_rtmp::rtmp_tag::script);

    return ctx->handler_.onscript(frame);
}

// 7.2.1.1. connect (p29)
// _result/_error
int simple_rtmp::rtmp_server_context_args::rtmp_server_onconnect(void* param, int r, double transaction, const struct rtmp_connect_t* connect)
{
    simple_rtmp::rtmp_server_context_args* ctx = (simple_rtmp::rtmp_server_context_args*)param;
    assert((double)RTMP_ENCODING_AMF_0 == connect->encoding || (double)RTMP_ENCODING_AMF_3 == connect->encoding);

    if (0 == r)
    {
        assert(1 == (int)transaction);
        memcpy(&ctx->info, connect, sizeof(ctx->info));
        r = rtmp_server_send_server_bandwidth(ctx);
        r = 0 == r ? rtmp_server_send_client_bandwidth(ctx) : r;
        r = 0 == r ? rtmp_server_send_set_chunk_size(ctx) : r;
    }

    if (0 == r)
    {
        int n = (int)(rtmp_netconnection_connect_reply(ctx->payload, sizeof(ctx->payload), transaction, RTMP_FMSVER, RTMP_CAPABILITIES, "NetConnection.Connect.Success", RTMP_LEVEL_STATUS, "Connection Succeeded.", connect->encoding) - ctx->payload);
        r     = rtmp_server_send_control(&ctx->rtmp, ctx->payload, n, 0);
    }

    return r;
}

// 7.2.1.3. createStream (p36)
// _result/_error
int simple_rtmp::rtmp_server_context_args::rtmp_server_oncreate_stream(void* param, int r, double transaction)
{
    simple_rtmp::rtmp_server_context_args* ctx = (simple_rtmp::rtmp_server_context_args*)param;
    if (0 == r)
    {
        ctx->stream_id = 1;
        if (0 == r)
        {
            r = (int)(rtmp_netconnection_create_stream_reply(ctx->payload, sizeof(ctx->payload), transaction, ctx->stream_id) - ctx->payload);
        }
        else
        {
            r = (int)(rtmp_netconnection_error(ctx->payload, sizeof(ctx->payload), transaction, "NetConnection.CreateStream.Failed", RTMP_LEVEL_ERROR, "createStream failed.") - ctx->payload);
        }
        r = rtmp_server_send_control(&ctx->rtmp, ctx->payload, r, 0 /*ctx->stream_id*/);    // must be 0
    }

    return r;
}

// 7.2.2.3. deleteStream (p43)
// The server does not send any response
int simple_rtmp::rtmp_server_context_args::rtmp_server_ondelete_stream(void* param, int r, double transaction, double stream_id)
{
    simple_rtmp::rtmp_server_context_args* ctx = (simple_rtmp::rtmp_server_context_args*)param;

    if (0 == r)
    {
        stream_id = ctx->stream_id = 0;    // clear stream id
        r                          = rtmp_server_send_onstatus(ctx, transaction, r, "NetStream.DeleteStream.Suceess", "NetStream.DeleteStream.Failed", "");
    }

    return r;
}

int simple_rtmp::rtmp_server_context_args::rtmp_server_onget_stream_length(void* param, int r, double transaction, const char* stream_name)
{
    simple_rtmp::rtmp_server_context_args* ctx = (simple_rtmp::rtmp_server_context_args*)param;

    double duration = -1;
    if (0 == r && ctx->handler_.ongetduration)
    {
        // get duration (seconds)
        r = ctx->handler_.ongetduration(ctx->info.app, stream_name, &duration);
        if (0 == r)
        {
            r = (int)(rtmp_netconnection_get_stream_length_reply(ctx->payload, sizeof(ctx->payload), transaction, duration) - ctx->payload);
            r = rtmp_server_send_control(&ctx->rtmp, ctx->payload, r, ctx->stream_id);
        }
    }

    return r;
}

// 7.2.2.6. publish (p45)
// The server responds with the onStatus command
int simple_rtmp::rtmp_server_context_args::rtmp_server_onpublish(void* param, int r, double transaction, const char* stream_name, const char* stream_type)
{
    simple_rtmp::rtmp_server_context_args* ctx = (simple_rtmp::rtmp_server_context_args*)param;
    if (0 == r)
    {
        ctx->start.play        = RTMP_SERVER_ONPUBLISH;
        ctx->start.transaction = transaction;
        snprintf(ctx->stream_name, sizeof(ctx->stream_name) - 1, "%s", stream_name != nullptr ? stream_name : "");
        snprintf(ctx->stream_type, sizeof(ctx->stream_type) - 1, "%s", stream_type != nullptr ? stream_type : "");

        r = ctx->handler_.onpublish(ctx->info.app, stream_name, stream_type);
        if (RTMP_SERVER_ASYNC_START == r || 0 == ctx->start.play)
        {
            return RTMP_SERVER_ASYNC_START == r ? 0 : r;
        }

        r = ctx->ctx_->rtmp_server_start(r, "");
    }

    return r;
}

// 7.2.2.1. play (p38)
// reply onStatus NetStream.Play.Start & NetStream.Play.Reset
int simple_rtmp::rtmp_server_context_args::rtmp_server_onplay(void* param, int r, double transaction, const char* stream_name, double start, double duration, uint8_t reset)
{
    simple_rtmp::rtmp_server_context_args* ctx = (simple_rtmp::rtmp_server_context_args*)param;
    if (0 == r)
    {
        ctx->start.play        = RTMP_SERVER_ONPLAY;
        ctx->start.reset       = reset;
        ctx->start.transaction = transaction;
        snprintf(ctx->stream_name, sizeof(ctx->stream_name) - 1, "%s", stream_name != nullptr ? stream_name : "");
        snprintf(ctx->stream_type, sizeof(ctx->stream_type) - 1, "%s", -1 == start ? RTMP_STREAM_LIVE : RTMP_STREAM_RECORD);
        r = ctx->handler_.onplay(ctx->info.app, stream_name, start, duration, reset);
        if (RTMP_SERVER_ASYNC_START == r || 0 == ctx->start.play)
        {
            return RTMP_SERVER_ASYNC_START == r ? 0 : r;
        }

        r = ctx->ctx_->rtmp_server_start(r, "");
    }

    return r;
}

// 7.2.2.8. pause (p47)
// sucessful: NetStream.Pause.Notify/NetStream.Unpause.Notify
// failure: _error message
int simple_rtmp::rtmp_server_context_args::rtmp_server_onpause(void* param, int r, double transaction, uint8_t pause, double milliSeconds)
{
    simple_rtmp::rtmp_server_context_args* ctx = (simple_rtmp::rtmp_server_context_args*)param;
    if (0 == r)
    {
        r = ctx->handler_.onpause(pause, (uint32_t)milliSeconds);
        r = rtmp_server_send_onstatus(ctx, transaction, r, pause ? "NetStream.Pause.Notify" : "NetStream.Unpause.Notify", "NetStream.Pause.Failed", "");
    }

    return r;
}

// 7.2.2.7. seek (p46)
// successful : NetStream.Seek.Notify
// failure:  _error message
int simple_rtmp::rtmp_server_context_args::rtmp_server_onseek(void* param, int r, double transaction, double milliSeconds)
{
    simple_rtmp::rtmp_server_context_args* ctx = (simple_rtmp::rtmp_server_context_args*)param;
    if (0 == r)
    {
        r = ctx->handler_.onseek((uint32_t)milliSeconds);
        r = rtmp_server_send_onstatus(ctx, transaction, r, "NetStream.Seek.Notify", "NetStream.Seek.Failed", "");
    }

    return r;
}

// 7.2.2.4. receiveAudio (p44)
// false: The server does not send any response,
// true: server responds with status messages NetStream.Seek.Notify and NetStream.Play.Start
int simple_rtmp::rtmp_server_context_args::rtmp_server_onreceive_audio(void* param, int r, double transaction, uint8_t audio)
{
    simple_rtmp::rtmp_server_context_args* ctx = (simple_rtmp::rtmp_server_context_args*)param;

    if (0 == r)
    {
        ctx->receiveAudio = audio;
        if (audio)
        {
            r = rtmp_server_send_onstatus(ctx, transaction, r, "NetStream.Seek.Notify", "NetStream.Seek.Failed", "");
            r = rtmp_server_send_onstatus(ctx, transaction, r, "NetStream.Play.Start", "NetStream.Play.Failed", "");
        }
    }

    return r;
}

int simple_rtmp::rtmp_server_context_args::rtmp_server_onreceive_video(void* param, int r, double transaction, uint8_t video)
{
    simple_rtmp::rtmp_server_context_args* ctx = (simple_rtmp::rtmp_server_context_args*)param;
    if (0 == r)
    {
        ctx->receiveVideo = video;
        if (video)
        {
            r = rtmp_server_send_onstatus(ctx, transaction, r, "NetStream.Seek.Notify", "NetStream.Seek.Failed", "");
            r = rtmp_server_send_onstatus(ctx, transaction, r, "NetStream.Play.Start", "NetStream.Play.Failed", "");
        }
    }

    return r;
}

int simple_rtmp::rtmp_server_context_args::rtmp_server_send(void* param, const uint8_t* header, uint32_t headerBytes, const uint8_t* payload, uint32_t payloadBytes)
{
    simple_rtmp::rtmp_server_context_args* ctx = (simple_rtmp::rtmp_server_context_args*)param;
    auto frame                                 = fixed_frame_buffer::create();
    frame->append(header, headerBytes);
    frame->append(payload, payloadBytes);
    int r = ctx->handler_.send(frame);
    return (r == (int)(payloadBytes + headerBytes)) ? 0 : -1;
}

rtmp_server_context::rtmp_server_context(rtmp_server_context_handler handler) : args_(new simple_rtmp::rtmp_server_context_args())
{
    args_->init(this, handler);
}

int rtmp_server_context::rtmp_server_start(int r, const std::string& msg)
{
    if (RTMP_SERVER_ONPLAY == args_->start.play)
    {
        if (0 == r)
        {
            // User Control (StreamBegin)
            r = 0 == r ? args_->rtmp_server_send_stream_begin(args_) : r;

            // NetStream.Play.Reset
            if (args_->start.reset)
            {
                r = 0 == r ? args_->rtmp_server_send_onstatus(args_, args_->start.transaction, 0, "NetStream.Play.Reset", "NetStream.Play.Failed", "") : r;
            }

            if (0 != r)
            {
                return r;
            }
        }

        r = args_->rtmp_server_send_onstatus(args_, args_->start.transaction, r, "NetStream.Play.Start", !msg.empty() ? msg.data() : "NetStream.Play.Failed", "Start video on demand");

        // User Control (StreamIsRecorded)
        r = 0 == r ? args_->rtmp_server_send_stream_is_record(args_) : r;
        r = 0 == r ? args_->rtmp_server_rtmp_sample_access(args_) : r;
    }
    else if (RTMP_SERVER_ONPUBLISH == args_->start.play)
    {
        if (0 == r)
        {
            // User Control (StreamBegin)
            r = args_->rtmp_server_send_stream_begin(args_);
            if (0 != r)
            {
                return r;
            }
        }

        r = args_->rtmp_server_send_onstatus(args_, args_->start.transaction, r, "NetStream.Publish.Start", !msg.empty() ? msg.data() : "NetStream.Publish.BadName", "");
    }

    args_->start.play = 0;
    return r;
}

int rtmp_server_context::rtmp_server_input(const uint8_t* data, size_t bytes)
{
    int r;
    size_t n;
    const uint8_t* p;

    p = data;
    while (bytes > 0)
    {
        switch (args_->handshake_state)
        {
            case RTMP_HANDSHAKE_UNINIT:    // C0: version
                args_->handshake_state = RTMP_HANDSHAKE_0;
                args_->handshake_bytes = 0;    // clear buffer
                assert(*p <= RTMP_VERSION);
                bytes -= 1;
                p += 1;
                break;

            case RTMP_HANDSHAKE_0:    // C1: 4-time + 4-zero + 1528-random
                assert(RTMP_HANDSHAKE_SIZE > args_->handshake_bytes);
                n = RTMP_HANDSHAKE_SIZE - args_->handshake_bytes;
                n = n <= bytes ? n : bytes;
                memcpy(args_->payload + args_->handshake_bytes, p, n);
                args_->handshake_bytes += n;
                bytes -= n;
                p += n;

                if (args_->handshake_bytes == RTMP_HANDSHAKE_SIZE)
                {
                    args_->handshake_state = RTMP_HANDSHAKE_1;
                    args_->handshake_bytes = 0;    // clear buffer
                    r                      = args_->rtmp_server_send_handshake(args_);
                    if (0 != r)
                    {
                        return r;
                    }
                }
                break;

            case RTMP_HANDSHAKE_1:    // C2: 4-time + 4-time2 + 1528-echo
                assert(RTMP_HANDSHAKE_SIZE > args_->handshake_bytes);
                n = RTMP_HANDSHAKE_SIZE - args_->handshake_bytes;
                n = n <= bytes ? n : bytes;
                memcpy(args_->payload + args_->handshake_bytes, p, n);
                args_->handshake_bytes += n;
                bytes -= n;
                p += n;

                if (args_->handshake_bytes == RTMP_HANDSHAKE_SIZE)
                {
                    args_->handshake_state = RTMP_HANDSHAKE_2;
                    args_->handshake_bytes = 0;    // clear buffer
                }
                break;

            case RTMP_HANDSHAKE_2:
            default:
                args_->rtmp_server_send_acknowledgement(args_, bytes);
                return rtmp_chunk_read(&args_->rtmp, (const uint8_t*)p, bytes);
        }
    }

    return 0;
}

int rtmp_server_context::rtmp_server_stop()
{
    static_assert(sizeof(args_->rtmp.in_packets) == sizeof(args_->rtmp.out_packets));
    for (int i = 0; i < N_CHUNK_STREAM; i++)
    {
        assert(NULL == args_->rtmp.out_packets[i].payload);
        if (args_->rtmp.in_packets[i].payload)
        {
            free(args_->rtmp.in_packets[i].payload);
        }
    }

    delete args_;

    return 0;
}

int rtmp_server_context::rtmp_server_send_audio(const simple_rtmp::frame_buffer::ptr& frame)
{
    struct rtmp_chunk_header_t header;
    if (0 == args_->receiveAudio)
    {
        return 0;    // client don't want receive audio
    }

    header.fmt       = RTMP_CHUNK_TYPE_1;    // enable compact header
    header.cid       = RTMP_CHANNEL_AUDIO;
    header.timestamp = frame->pts();
    header.length    = frame->size();
    header.type      = RTMP_TYPE_AUDIO;
    header.stream_id = args_->stream_id;

    return rtmp_chunk_write_help(args_, &header, frame);
}

int rtmp_server_context::rtmp_server_send_video(const simple_rtmp::frame_buffer::ptr& frame)
{
    struct rtmp_chunk_header_t header;
    if (0 == args_->receiveVideo)
    {
        return 0;    // client don't want receive video
    }

    header.fmt       = RTMP_CHUNK_TYPE_1;    // enable compact header
    header.cid       = RTMP_CHANNEL_VIDEO;
    header.timestamp = frame->pts();
    header.length    = frame->size();
    header.type      = RTMP_TYPE_VIDEO;
    header.stream_id = args_->stream_id;

    return rtmp_chunk_write_help(args_, &header, frame);
}

int rtmp_server_context::rtmp_server_send_script(const simple_rtmp::frame_buffer::ptr& frame)
{
    struct rtmp_chunk_header_t header;
    header.fmt       = RTMP_CHUNK_TYPE_1;    // enable compact header
    header.cid       = RTMP_CHANNEL_INVOKE;
    header.timestamp = frame->pts();
    header.length    = frame->size();
    header.type      = RTMP_TYPE_DATA;
    header.stream_id = args_->stream_id;
    return rtmp_chunk_write_help(args_, &header, frame);
}
