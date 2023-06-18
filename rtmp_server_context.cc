extern "C"
{
#include "rtmp-internal.h"
#include "rtmp-msgtypeid.h"
#include "rtmp-handshake.h"
#include "rtmp-netstream.h"
#include "rtmp-netconnection.h"
#include "rtmp-control-message.h"
#include "rtmp-chunk-header.h"
#include "rtmp-netconnection.h"
#include "rtmp-event.h"
}
#include "rtmp_server_context.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <ctime>

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
};

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

int rtmp_server_context::rtmp_server_send_onstatus(rtmp_server_context* ctx, double transaction, int r, const char* success, const char* fail, const char* description)
{
    r = (int)(rtmp_netstream_onstatus(ctx->args_->payload, sizeof(ctx->args_->payload), transaction, 0 == r ? RTMP_LEVEL_STATUS : RTMP_LEVEL_ERROR, 0 == r ? success : fail, description) - ctx->args_->payload);
    return rtmp_server_send_control(&ctx->args_->rtmp, ctx->args_->payload, r, ctx->args_->stream_id);
}

// handshake
int rtmp_server_context::rtmp_server_send_handshake(rtmp_server_context* ctx)
{
    int n = rtmp_handshake_s0(ctx->args_->handshake, RTMP_VERSION);
    n += rtmp_handshake_s1(ctx->args_->handshake + n, (uint32_t)time(NULL), ctx->args_->payload, RTMP_HANDSHAKE_SIZE);
    n += rtmp_handshake_s2(ctx->args_->handshake + n, (uint32_t)time(NULL), ctx->args_->payload, RTMP_HANDSHAKE_SIZE);
    assert(n == 1 + RTMP_HANDSHAKE_SIZE + RTMP_HANDSHAKE_SIZE);
    auto frame = std::make_shared<simple_rtmp::frame_buffer>();
    frame->append(ctx->args_->handshake, n);
    int r = ctx->handler_.send(frame);
    return n == r ? 0 : r;
}

/// 5.4.1. Set Chunk Size (1)
int rtmp_server_context::rtmp_server_send_set_chunk_size(rtmp_server_context* ctx)
{
    int n      = rtmp_set_chunk_size(ctx->args_->payload, sizeof(ctx->args_->payload), RTMP_OUTPUT_CHUNK_SIZE);
    auto frame = std::make_shared<simple_rtmp::frame_buffer>();
    frame->append(ctx->args_->payload, n);
    int r                           = ctx->handler_.send(frame);
    ctx->args_->rtmp.out_chunk_size = RTMP_OUTPUT_CHUNK_SIZE;
    return n == r ? 0 : r;
}

/// 5.4.3. Acknowledgement (3)
int rtmp_server_context::rtmp_server_send_acknowledgement(rtmp_server_context* ctx, size_t size)
{
    ctx->args_->recv_bytes[0] += (uint32_t)size;
    if (ctx->args_->rtmp.window_size && ctx->args_->recv_bytes[0] - ctx->args_->recv_bytes[1] > ctx->args_->rtmp.window_size)
    {
        int n      = rtmp_acknowledgement(ctx->args_->payload, sizeof(ctx->args_->payload), ctx->args_->recv_bytes[0]);
        auto frame = std::make_shared<simple_rtmp::frame_buffer>();
        frame->append(ctx->args_->payload, n);
        int r                     = ctx->handler_.send(frame);
        ctx->args_->recv_bytes[1] = ctx->args_->recv_bytes[0];
        return n == r ? 0 : r;
    }
    return 0;
}

/// 5.4.4. Window Acknowledgement Size (5)
int rtmp_server_context::rtmp_server_send_server_bandwidth(rtmp_server_context* ctx)
{
    int n      = rtmp_window_acknowledgement_size(ctx->args_->payload, sizeof(ctx->args_->payload), ctx->args_->rtmp.window_size);
    auto frame = std::make_shared<simple_rtmp::frame_buffer>();
    frame->append(ctx->args_->payload, n);
    int r = ctx->handler_.send(frame);
    return n == r ? 0 : r;
}

/// 5.4.5. Set Peer Bandwidth (6)
int rtmp_server_context::rtmp_server_send_client_bandwidth(rtmp_server_context* ctx)
{
    int n      = rtmp_set_peer_bandwidth(ctx->args_->payload, sizeof(ctx->args_->payload), ctx->args_->rtmp.peer_bandwidth, RTMP_BANDWIDTH_LIMIT_DYNAMIC);
    auto frame = std::make_shared<simple_rtmp::frame_buffer>();
    frame->append(ctx->args_->payload, n);
    int r = ctx->handler_.send(frame);
    return n == r ? 0 : r;
}

int rtmp_server_context::rtmp_server_send_stream_is_record(rtmp_server_context* ctx)
{
    int n      = rtmp_event_stream_is_record(ctx->args_->payload, sizeof(ctx->args_->payload), ctx->args_->stream_id);
    auto frame = std::make_shared<simple_rtmp::frame_buffer>();
    frame->append(ctx->args_->payload, n);
    int r = ctx->handler_.send(frame);
    return n == r ? 0 : r;
}

int rtmp_server_context::rtmp_server_send_stream_begin(rtmp_server_context* ctx)
{
    int n      = rtmp_event_stream_begin(ctx->args_->payload, sizeof(ctx->args_->payload), ctx->args_->stream_id);
    auto frame = std::make_shared<simple_rtmp::frame_buffer>();
    frame->append(ctx->args_->payload, n);
    int r = ctx->handler_.send(frame);
    return n == r ? 0 : r;
}

int rtmp_server_context::rtmp_server_rtmp_sample_access(rtmp_server_context* ctx)
{
    struct rtmp_chunk_header_t header;
    int n            = (int)(rtmp_netstream_rtmpsampleaccess(ctx->args_->payload, sizeof(ctx->args_->payload)) - ctx->args_->payload);
    header.fmt       = RTMP_CHUNK_TYPE_0;    // disable compact header
    header.cid       = RTMP_CHANNEL_INVOKE;
    header.timestamp = 0;
    header.length    = n;
    header.type      = RTMP_TYPE_DATA;
    header.stream_id = ctx->args_->stream_id;
    return rtmp_chunk_write(&ctx->args_->rtmp, &header, ctx->args_->payload);
}

void rtmp_server_context::rtmp_server_onabort(void* param, uint32_t chunk_stream_id)
{
    rtmp_server_context* ctx = (rtmp_server_context*)param;
    (void)ctx, (void)chunk_stream_id;
    //	ctx->handler_.onerror -1, "client abort");
}

int rtmp_server_context::rtmp_server_onaudio(void* param, const uint8_t* data, size_t bytes, uint32_t timestamp)
{
    rtmp_server_context* ctx = (rtmp_server_context*)param;
    auto frame               = std::make_shared<simple_rtmp::frame_buffer>();
    frame->append(data, bytes);
    frame->pts = timestamp;
    frame->dts = timestamp;
    return ctx->handler_.onaudio(frame);
}

int rtmp_server_context::rtmp_server_onvideo(void* param, const uint8_t* data, size_t bytes, uint32_t timestamp)
{
    rtmp_server_context* ctx = (rtmp_server_context*)param;
    auto frame               = std::make_shared<simple_rtmp::frame_buffer>();
    frame->append(data, bytes);
    frame->pts = timestamp;
    frame->dts = timestamp;
    return ctx->handler_.onvideo(frame);
}

int rtmp_server_context::rtmp_server_onscript(void* param, const uint8_t* data, size_t bytes, uint32_t timestamp)
{
    rtmp_server_context* ctx = (rtmp_server_context*)param;
    auto frame               = std::make_shared<simple_rtmp::frame_buffer>();
    frame->append(data, bytes);
    frame->pts = timestamp;
    frame->dts = timestamp;
    return ctx->handler_.onscript(frame);
}

// 7.2.1.1. connect (p29)
// _result/_error
int rtmp_server_context::rtmp_server_onconnect(void* param, int r, double transaction, const struct rtmp_connect_t* connect)
{
    rtmp_server_context* ctx = (rtmp_server_context*)param;
    assert((double)RTMP_ENCODING_AMF_0 == connect->encoding || (double)RTMP_ENCODING_AMF_3 == connect->encoding);

    if (0 == r)
    {
        assert(1 == (int)transaction);
        memcpy(&ctx->args_->info, connect, sizeof(ctx->args_->info));
        r = rtmp_server_send_server_bandwidth(ctx);
        r = 0 == r ? rtmp_server_send_client_bandwidth(ctx) : r;
        r = 0 == r ? rtmp_server_send_set_chunk_size(ctx) : r;
    }

    if (0 == r)
    {
        int n = (int)(rtmp_netconnection_connect_reply(ctx->args_->payload, sizeof(ctx->args_->payload), transaction, RTMP_FMSVER, RTMP_CAPABILITIES, "NetConnection.Connect.Success", RTMP_LEVEL_STATUS, "Connection Succeeded.", connect->encoding) - ctx->args_->payload);
        r     = rtmp_server_send_control(&ctx->args_->rtmp, ctx->args_->payload, n, 0);
    }

    return r;
}

// 7.2.1.3. createStream (p36)
// _result/_error
int rtmp_server_context::rtmp_server_oncreate_stream(void* param, int r, double transaction)
{
    rtmp_server_context* ctx = (rtmp_server_context*)param;
    if (0 == r)
    {
        ctx->args_->stream_id = 1;
        // r = ctx->handler.oncreate_stream(ctx->param, &ctx->stream_id);
        if (0 == r)
            r = (int)(rtmp_netconnection_create_stream_reply(ctx->args_->payload, sizeof(ctx->args_->payload), transaction, ctx->args_->stream_id) - ctx->args_->payload);
        else
            r = (int)(rtmp_netconnection_error(ctx->args_->payload, sizeof(ctx->args_->payload), transaction, "NetConnection.CreateStream.Failed", RTMP_LEVEL_ERROR, "createStream failed.") - ctx->args_->payload);
        r = rtmp_server_send_control(&ctx->args_->rtmp, ctx->args_->payload, r, 0 /*ctx->stream_id*/);    // must be 0
    }

    return r;
}

// 7.2.2.3. deleteStream (p43)
// The server does not send any response
int rtmp_server_context::rtmp_server_ondelete_stream(void* param, int r, double transaction, double stream_id)
{
    rtmp_server_context* ctx = (rtmp_server_context*)param;

    if (0 == r)
    {
        stream_id = ctx->args_->stream_id = 0;    // clear stream id
        // r = ctx->handler.ondelete_stream(ctx->param, (uint32_t)stream_id);
        r = rtmp_server_send_onstatus(ctx, transaction, r, "NetStream.DeleteStream.Suceess", "NetStream.DeleteStream.Failed", "");
    }

    return r;
}

int rtmp_server_context::rtmp_server_onget_stream_length(void* param, int r, double transaction, const char* stream_name)
{
    rtmp_server_context* ctx = (rtmp_server_context*)param;

    double duration = -1;
    if (0 == r && ctx->handler_.ongetduration)
    {
        // get duration (seconds)
        r = ctx->handler_.ongetduration(ctx->args_->info.app, stream_name, &duration);
        if (0 == r)
        {
            r = (int)(rtmp_netconnection_get_stream_length_reply(ctx->args_->payload, sizeof(ctx->args_->payload), transaction, duration) - ctx->args_->payload);
            r = rtmp_server_send_control(&ctx->args_->rtmp, ctx->args_->payload, r, ctx->args_->stream_id);
        }
    }

    return r;
}

// 7.2.2.6. publish (p45)
// The server responds with the onStatus command
int rtmp_server_context::rtmp_server_onpublish(void* param, int r, double transaction, const char* stream_name, const char* stream_type)
{
    rtmp_server_context* ctx = (rtmp_server_context*)param;
    if (0 == r)
    {
        ctx->args_->start.play        = RTMP_SERVER_ONPUBLISH;
        ctx->args_->start.transaction = transaction;
        snprintf(ctx->args_->stream_name, sizeof(ctx->args_->stream_name) - 1, "%s", stream_name ? stream_name : "");
        snprintf(ctx->args_->stream_type, sizeof(ctx->args_->stream_type) - 1, "%s", stream_type ? stream_type : "");

        r = ctx->handler_.onpublish(ctx->args_->info.app, stream_name, stream_type);
        if (RTMP_SERVER_ASYNC_START == r || 0 == ctx->args_->start.play)
            return RTMP_SERVER_ASYNC_START == r ? 0 : r;

        r = ctx->rtmp_server_start(r, "");
    }

    return r;
}

// 7.2.2.1. play (p38)
// reply onStatus NetStream.Play.Start & NetStream.Play.Reset
int rtmp_server_context::rtmp_server_onplay(void* param, int r, double transaction, const char* stream_name, double start, double duration, uint8_t reset)
{
    rtmp_server_context* ctx = (rtmp_server_context*)param;
    if (0 == r)
    {
        ctx->args_->start.play        = RTMP_SERVER_ONPLAY;
        ctx->args_->start.reset       = reset;
        ctx->args_->start.transaction = transaction;
        snprintf(ctx->args_->stream_name, sizeof(ctx->args_->stream_name) - 1, "%s", stream_name ? stream_name : "");
        snprintf(ctx->args_->stream_type, sizeof(ctx->args_->stream_type) - 1, "%s", -1 == start ? RTMP_STREAM_LIVE : RTMP_STREAM_RECORD);
        r = ctx->handler_.onplay(ctx->args_->info.app, stream_name, start, duration, reset);
        if (RTMP_SERVER_ASYNC_START == r || 0 == ctx->args_->start.play)
            return RTMP_SERVER_ASYNC_START == r ? 0 : r;

        r = ctx->rtmp_server_start(r, "");
    }

    return r;
}

// 7.2.2.8. pause (p47)
// sucessful: NetStream.Pause.Notify/NetStream.Unpause.Notify
// failure: _error message
int rtmp_server_context::rtmp_server_onpause(void* param, int r, double transaction, uint8_t pause, double milliSeconds)
{
    rtmp_server_context* ctx = (rtmp_server_context*)param;
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
int rtmp_server_context::rtmp_server_onseek(void* param, int r, double transaction, double milliSeconds)
{
    rtmp_server_context* ctx = (rtmp_server_context*)param;
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
int rtmp_server_context::rtmp_server_onreceive_audio(void* param, int r, double transaction, uint8_t audio)
{
    rtmp_server_context* ctx = (rtmp_server_context*)param;

    if (0 == r)
    {
        ctx->args_->receiveAudio = audio;
        if (audio)
        {
            r = rtmp_server_send_onstatus(ctx, transaction, r, "NetStream.Seek.Notify", "NetStream.Seek.Failed", "");
            r = rtmp_server_send_onstatus(ctx, transaction, r, "NetStream.Play.Start", "NetStream.Play.Failed", "");
        }
    }

    return r;
}

int rtmp_server_context::rtmp_server_onreceive_video(void* param, int r, double transaction, uint8_t video)
{
    rtmp_server_context* ctx = (rtmp_server_context*)param;
    if (0 == r)
    {
        ctx->args_->receiveVideo = video;
        if (video)
        {
            r = rtmp_server_send_onstatus(ctx, transaction, r, "NetStream.Seek.Notify", "NetStream.Seek.Failed", "");
            r = rtmp_server_send_onstatus(ctx, transaction, r, "NetStream.Play.Start", "NetStream.Play.Failed", "");
        }
    }

    return r;
}

int rtmp_server_context::rtmp_server_send(void* param, const uint8_t* header, uint32_t headerBytes, const uint8_t* payload, uint32_t payloadBytes)
{
    rtmp_server_context* ctx = (rtmp_server_context*)param;
    auto frame               = std::make_shared<simple_rtmp::frame_buffer>();
    frame->append(header, headerBytes);
    frame->append(payload, payloadBytes);
    int r = ctx->handler_.send(frame);
    return (r == (int)(payloadBytes + headerBytes)) ? 0 : -1;
}

rtmp_server_context::rtmp_server_context(rtmp_server_context_handler handler) : handler_(std::move(handler)), args_(std::make_shared<simple_rtmp::rtmp_server_context_args>())
{
    args_->stream_id                                          = 0;
    args_->receiveAudio                                       = 1;
    args_->receiveVideo                                       = 1;
    args_->handshake_state                                    = RTMP_HANDSHAKE_UNINIT;
    args_->rtmp.parser.state                                  = RTMP_PARSE_INIT;
    args_->rtmp.in_chunk_size                                 = RTMP_CHUNK_SIZE;
    args_->rtmp.out_chunk_size                                = RTMP_CHUNK_SIZE;
    args_->rtmp.window_size                                   = 5000000;
    args_->rtmp.peer_bandwidth                                = 5000000;
    args_->rtmp.buffer_length_ms                              = 30000;
    args_->rtmp.param                                         = this;
    args_->rtmp.send                                          = rtmp_server_send;
    args_->rtmp.onaudio                                       = rtmp_server_onaudio;
    args_->rtmp.onvideo                                       = rtmp_server_onvideo;
    args_->rtmp.onabort                                       = rtmp_server_onabort;
    args_->rtmp.onscript                                      = rtmp_server_onscript;
    args_->rtmp.server.onconnect                              = rtmp_server_onconnect;
    args_->rtmp.server.oncreate_stream                        = rtmp_server_oncreate_stream;
    args_->rtmp.server.ondelete_stream                        = rtmp_server_ondelete_stream;
    args_->rtmp.server.onget_stream_length                    = rtmp_server_onget_stream_length;
    args_->rtmp.server.onpublish                              = rtmp_server_onpublish;
    args_->rtmp.server.onplay                                 = rtmp_server_onplay;
    args_->rtmp.server.onpause                                = rtmp_server_onpause;
    args_->rtmp.server.onseek                                 = rtmp_server_onseek;
    args_->rtmp.server.onreceive_audio                        = rtmp_server_onreceive_audio;
    args_->rtmp.server.onreceive_video                        = rtmp_server_onreceive_video;
    args_->rtmp.out_packets[RTMP_CHANNEL_PROTOCOL].header.cid = RTMP_CHANNEL_PROTOCOL;
    args_->rtmp.out_packets[RTMP_CHANNEL_INVOKE].header.cid   = RTMP_CHANNEL_INVOKE;
    args_->rtmp.out_packets[RTMP_CHANNEL_AUDIO].header.cid    = RTMP_CHANNEL_AUDIO;
    args_->rtmp.out_packets[RTMP_CHANNEL_VIDEO].header.cid    = RTMP_CHANNEL_VIDEO;
    args_->rtmp.out_packets[RTMP_CHANNEL_DATA].header.cid     = RTMP_CHANNEL_DATA;
}

int rtmp_server_context::rtmp_server_start(int r, const std::string& msg)
{
    return 0;
}

int rtmp_server_context::rtmp_server_input(const uint8_t* data, size_t bytes)
{
    return 0;
}

int rtmp_server_context::rtmp_server_stop()
{
    return 0;
}
