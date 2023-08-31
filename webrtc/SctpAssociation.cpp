#define MS_CLASS "RTC::SctpAssociation"
// #define MS_LOG_DEV_LEVEL 3

#include "SctpAssociation.hpp"
#include "log.h"
#include <memory>
#include <cstdio>     // std::snprintf()
#include <cstdlib>    // std::malloc(), std::free()
#include <cstring>    // std::memset(), std::memcpy()
#include <string>

// Free send buffer threshold (in bytes) upon which send_cb will be executed.
static const uint32_t SendBufferThreshold{256u};

/* SCTP events to which we are subscribing. */

/* clang-format off */
const uint16_t EventTypes[] =
{
	SCTP_ADAPTATION_INDICATION,
	SCTP_ASSOC_CHANGE,
	SCTP_ASSOC_RESET_EVENT,
	SCTP_REMOTE_ERROR,
	SCTP_SHUTDOWN_EVENT,
	SCTP_SEND_FAILED_EVENT,
	SCTP_STREAM_RESET_EVENT,
	SCTP_STREAM_CHANGE_EVENT
};
/* clang-format on */

/* Static methods for usrsctp callbacks. */

inline static int onRecvSctpData(struct socket* /*sock*/, union sctp_sockstore /*addr*/, void* data, size_t len, struct sctp_rcvinfo rcv, int flags, void* ulpInfo)
{
    auto* sctpAssociation = static_cast<RTC::SctpAssociation*>(ulpInfo);

    if (!sctpAssociation)
    {
        LOG_WARN("no SctpAssociation found");

        std::free(data);

        return 0;
    }

    if (flags & MSG_NOTIFICATION)
    {
        sctpAssociation->OnUsrSctpReceiveSctpNotification(static_cast<union sctp_notification*>(data), len);
    }
    else
    {
        const uint16_t streamId = rcv.rcv_sid;
        const uint32_t ppid = ntohl(rcv.rcv_ppid);
        const uint16_t ssn = rcv.rcv_ssn;

        LOG_DEBUG("data chunk received [length:{}, streamId:{} SSN:{} TSN:{} PPID:{} context:{} flags:{}]", len, rcv.rcv_sid, rcv.rcv_ssn, rcv.rcv_tsn, ntohl(rcv.rcv_ppid), rcv.rcv_context, flags);

        sctpAssociation->OnUsrSctpReceiveSctpData(streamId, ssn, ppid, flags, static_cast<uint8_t*>(data), len);
    }

    std::free(data);

    return 1;
}
/* Static methods for usrsctp global callbacks. */
inline static int onSendSctpData(void* addr, void* data, size_t len, uint8_t /*tos*/, uint8_t /*setDf*/)
{
    auto* sctpAssociation = static_cast<RTC::SctpAssociation*>(addr);

    if (sctpAssociation == nullptr)
        return -1;

    sctpAssociation->OnUsrSctpSendSctpData(data, len);

    // NOTE: Must not free data, usrsctp lib does it.

    return 0;
}
// Static method for printing usrsctp debug.
inline static void sctpDebug(const char* format, ...)
{
    char buffer[10000] = {0};
    va_list ap;

    va_start(ap, format);
    vsnprintf(buffer, sizeof(buffer), format, ap);

    // Remove the artificial carriage return set by usrsctp.
    buffer[std::strlen(buffer) - 1] = '\0';

    LOG_DEBUG("%s", buffer);

    va_end(ap);
}

namespace RTC
{
class SctpEnv : public std::enable_shared_from_this<SctpEnv>
{
   public:
    ~SctpEnv();
    static SctpEnv& Instance();

   private:
    SctpEnv();
};

SctpEnv::SctpEnv()
{
    usrsctp_init(0, onSendSctpData, sctpDebug);
    // Disable explicit congestion notifications (ecn).
    usrsctp_sysctl_set_sctp_ecn_enable(0);

#ifdef SCTP_DEBUG
    usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_ALL);
#endif
}

SctpEnv::~SctpEnv()
{
    usrsctp_finish();
}

/* Static. */

static constexpr size_t SctpMtu{1200};
static constexpr uint16_t MaxSctpStreams{65535};

/* Instance methods. */

SctpAssociation::SctpAssociation(Listener* listener, uint16_t os, uint16_t mis, size_t maxSctpMessageSize, size_t sctpSendBufferSize, bool isDataChannel)
    : listener(listener), os(os), mis(mis), maxSctpMessageSize(maxSctpMessageSize), sctpSendBufferSize(sctpSendBufferSize), isDataChannel(isDataChannel)
{
    // Register ourselves in usrsctp.
    // NOTE: This must be done before calling usrsctp_bind().
    usrsctp_register_address(reinterpret_cast<void*>(this));

    int ret;

    this->socket = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP, onRecvSctpData, nullptr, 0, reinterpret_cast<void*>(this));

    if (!this->socket)
    {
        usrsctp_deregister_address(reinterpret_cast<void*>(this));

        LOG_ERROR("usrsctp_socket() failed: {}", std::strerror(errno));
    }

    usrsctp_set_ulpinfo(this->socket, reinterpret_cast<void*>(this));

    // Make the socket non-blocking.
    ret = usrsctp_set_non_blocking(this->socket, 1);

    if (ret < 0)
    {
        usrsctp_deregister_address(reinterpret_cast<void*>(this));

        LOG_ERROR("usrsctp_set_non_blocking() failed: {}", std::strerror(errno));
    }

    // Set SO_LINGER.
    // This ensures that the usrsctp close call deletes the association. This
    // prevents usrsctp from calling the global send callback with references to
    // this class as the address.
    struct linger lingerOpt;    // NOLINT(cppcoreguidelines-pro-type-member-init)

    lingerOpt.l_onoff = 1;
    lingerOpt.l_linger = 0;

    ret = usrsctp_setsockopt(this->socket, SOL_SOCKET, SO_LINGER, &lingerOpt, sizeof(lingerOpt));

    if (ret < 0)
    {
        usrsctp_deregister_address(reinterpret_cast<void*>(this));

        LOG_ERROR("usrsctp_setsockopt(SO_LINGER) failed: {}", std::strerror(errno));
    }

    // Set SCTP_ENABLE_STREAM_RESET.
    struct sctp_assoc_value av;    // NOLINT(cppcoreguidelines-pro-type-member-init)

    av.assoc_value = SCTP_ENABLE_RESET_STREAM_REQ | SCTP_ENABLE_RESET_ASSOC_REQ | SCTP_ENABLE_CHANGE_ASSOC_REQ;

    ret = usrsctp_setsockopt(this->socket, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET, &av, sizeof(av));

    if (ret < 0)
    {
        usrsctp_deregister_address(reinterpret_cast<void*>(this));

        LOG_ERROR("usrsctp_setsockopt(SCTP_ENABLE_STREAM_RESET) failed: {}", std::strerror(errno));
    }

    // Set SCTP_NODELAY.
    const uint32_t noDelay = 1;

    ret = usrsctp_setsockopt(this->socket, IPPROTO_SCTP, SCTP_NODELAY, &noDelay, sizeof(noDelay));

    if (ret < 0)
    {
        usrsctp_deregister_address(reinterpret_cast<void*>(this));

        LOG_ERROR("usrsctp_setsockopt(SCTP_NODELAY) failed: {}", std::strerror(errno));
    }

    // Enable events.
    struct sctp_event event;    // NOLINT(cppcoreguidelines-pro-type-member-init)

    std::memset(&event, 0, sizeof(event));
    event.se_on = 1;

    for (size_t i{0}; i < sizeof(EventTypes) / sizeof(uint16_t); ++i)
    {
        event.se_type = EventTypes[i];

        ret = usrsctp_setsockopt(this->socket, IPPROTO_SCTP, SCTP_EVENT, &event, sizeof(event));

        if (ret < 0)
        {
            usrsctp_deregister_address(reinterpret_cast<void*>(this));

            LOG_ERROR("usrsctp_setsockopt(SCTP_EVENT) failed: {}", std::strerror(errno));
        }
    }

    // Init message.
    struct sctp_initmsg initmsg;    // NOLINT(cppcoreguidelines-pro-type-member-init)

    std::memset(&initmsg, 0, sizeof(initmsg));
    initmsg.sinit_num_ostreams = this->os;
    initmsg.sinit_max_instreams = this->mis;

    ret = usrsctp_setsockopt(this->socket, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg));

    if (ret < 0)
    {
        usrsctp_deregister_address(reinterpret_cast<void*>(this));

        LOG_ERROR("usrsctp_setsockopt(SCTP_INITMSG) failed: {}", std::strerror(errno));
    }

    // Server side.
    struct sockaddr_conn sconn;    // NOLINT(cppcoreguidelines-pro-type-member-init)

    std::memset(&sconn, 0, sizeof(sconn));
    sconn.sconn_family = AF_CONN;
    sconn.sconn_port = htons(5000);
    sconn.sconn_addr = reinterpret_cast<void*>(this);
#ifdef HAVE_SCONN_LEN
    sconn.sconn_len = sizeof(sconn);
#endif

    ret = usrsctp_bind(this->socket, reinterpret_cast<struct sockaddr*>(&sconn), sizeof(sconn));

    if (ret < 0)
    {
        usrsctp_deregister_address(reinterpret_cast<void*>(this));

        LOG_ERROR("usrsctp_bind() failed: {}", std::strerror(errno));
    }

    auto bufferSize = static_cast<int>(sctpSendBufferSize);

    if (usrsctp_setsockopt(this->socket, SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(int)) < 0)
    {
        usrsctp_deregister_address(reinterpret_cast<void*>(this));

        LOG_ERROR("usrsctp_setsockopt(SO_SNDBUF) failed: {}", std::strerror(errno));
    }
}

SctpAssociation::~SctpAssociation()
{
    usrsctp_set_ulpinfo(this->socket, nullptr);
    usrsctp_close(this->socket);

    // Deregister ourselves from usrsctp.
    usrsctp_deregister_address(reinterpret_cast<void*>(this));

    delete[] this->messageBuffer;
}

void SctpAssociation::TransportConnected()
{
    // Just run the SCTP stack if our state is 'new'.
    if (this->state != SctpState::NEW)
        return;

    do
    {
        struct sockaddr_conn rconn;    // NOLINT(cppcoreguidelines-pro-type-member-init)

        std::memset(&rconn, 0, sizeof(rconn));
        rconn.sconn_family = AF_CONN;
        rconn.sconn_port = htons(5000);
        rconn.sconn_addr = reinterpret_cast<void*>(this);
#ifdef HAVE_SCONN_LEN
        rconn.sconn_len = sizeof(rconn);
#endif

        int ret = usrsctp_connect(this->socket, reinterpret_cast<struct sockaddr*>(&rconn), sizeof(rconn));
        if (ret < 0 && errno != EINPROGRESS)
        {
            LOG_ERROR("usrsctp_connect() failed: {}", std::strerror(errno));
            break;
        }

        // Disable MTU discovery.
        sctp_paddrparams peerAddrParams;    // NOLINT(cppcoreguidelines-pro-type-member-init)

        std::memset(&peerAddrParams, 0, sizeof(peerAddrParams));
        std::memcpy(&peerAddrParams.spp_address, &rconn, sizeof(rconn));
        peerAddrParams.spp_flags = SPP_PMTUD_DISABLE;

        // The MTU value provided specifies the space available for chunks in the
        // packet, so let's subtract the SCTP header size.
        peerAddrParams.spp_pathmtu = SctpMtu - sizeof(struct sctp_common_header);

        ret = usrsctp_setsockopt(this->socket, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, &peerAddrParams, sizeof(peerAddrParams));

        if (ret < 0)
        {
            LOG_ERROR("usrsctp_setsockopt(SCTP_PEER_ADDR_PARAMS) failed: {}", std::strerror(errno));
            break;
        }

        // Announce connecting state.
        this->state = SctpState::CONNECTING;
        this->listener->OnSctpAssociationConnecting(this);
        return;
    } while (false);

    this->state = SctpState::FAILED;
    this->listener->OnSctpAssociationFailed(this);
}

void SctpAssociation::ProcessSctpData(const uint8_t* data, size_t len)
{
    usrsctp_conninput(reinterpret_cast<void*>(this), data, len, 0);
}

void SctpAssociation::SendSctpMessage(const RTC::SctpStreamParameters& params, uint32_t ppid, const uint8_t* msg, size_t len)
{
    // This must be controlled by the DataConsumer.
    if (len > this->maxSctpMessageSize)
    {
        LOG_ERROR("given message exceeds max allowed message size [message size:{}, max message size:{}]", len, this->maxSctpMessageSize);
        return;
    }

    // Fill sctp_sendv_spa.
    struct sctp_sendv_spa spa;    // NOLINT(cppcoreguidelines-pro-type-member-init)

    std::memset(&spa, 0, sizeof(spa));
    spa.sendv_flags = SCTP_SEND_SNDINFO_VALID;
    spa.sendv_sndinfo.snd_sid = params.streamId;
    spa.sendv_sndinfo.snd_ppid = htonl(ppid);
    spa.sendv_sndinfo.snd_flags = SCTP_EOR;

    // If ordered it must be reliable.
    if (params.ordered)
    {
        spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_NONE;
        spa.sendv_prinfo.pr_value = 0;
    }
    // Configure reliability: https://tools.ietf.org/html/rfc3758
    else
    {
        spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
        spa.sendv_sndinfo.snd_flags |= SCTP_UNORDERED;

        if (params.maxPacketLifeTime != 0)
        {
            spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_TTL;
            spa.sendv_prinfo.pr_value = params.maxPacketLifeTime;
        }
        else if (params.maxRetransmits != 0)
        {
            spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_RTX;
            spa.sendv_prinfo.pr_value = params.maxRetransmits;
        }
    }

    this->sctpBufferedAmount += len;

    // Notify the listener about the buffered amount increase regardless
    // usrsctp_sendv result.
    // In case of failure the correct value will be later provided by usrsctp
    // via onSendSctpData.
    this->listener->OnSctpAssociationBufferedAmount(this, this->sctpBufferedAmount);

    int ret = usrsctp_sendv(this->socket, msg, len, nullptr, 0, &spa, static_cast<socklen_t>(sizeof(spa)), SCTP_SENDV_SPA, 0);

    if (ret < 0)
    {
        LOG_DEBUG("error sending SCTP message [sid:{} ppid:{} message size:{}]: {}", params.streamId, ppid, len, std::strerror(errno));
    }
}

void SctpAssociation::HandleDataConsumer(const RTC::SctpStreamParameters& params)
{
    auto streamId = params.streamId;

    // We need more OS.
    if (streamId > this->os - 1)
        AddOutgoingStreams(/*force*/ false);
}

void SctpAssociation::DataProducerClosed(const RTC::SctpStreamParameters& params)
{
    auto streamId = params.streamId;

    // Send SCTP_RESET_STREAMS to the remote.
    // https://tools.ietf.org/html/draft-ietf-rtcweb-data-channel-13#section-6.7
    if (this->isDataChannel)
        ResetSctpStream(streamId, StreamDirection::OUTGOING);
    else
        ResetSctpStream(streamId, StreamDirection::INCOMING);
}

void SctpAssociation::DataConsumerClosed(const RTC::SctpStreamParameters& params)
{
    auto streamId = params.streamId;

    // Send SCTP_RESET_STREAMS to the remote.
    ResetSctpStream(streamId, StreamDirection::OUTGOING);
}

void SctpAssociation::ResetSctpStream(uint16_t streamId, StreamDirection direction)
{
    // Do nothing if an outgoing stream that could not be allocated by us.
    if (direction == StreamDirection::OUTGOING && streamId > this->os - 1)
        return;

    int ret;
    struct sctp_assoc_value av;    // NOLINT(cppcoreguidelines-pro-type-member-init)
    socklen_t len = sizeof(av);

    ret = usrsctp_getsockopt(this->socket, IPPROTO_SCTP, SCTP_RECONFIG_SUPPORTED, &av, &len);

    if (ret == 0)
    {
        if (av.assoc_value != 1)
        {
            LOG_DEBUG("stream reconfiguration not negotiated");

            return;
        }
    }
    else
    {
        LOG_WARN("could not retrieve whether stream reconfiguration has been negotiated: {}", std::strerror(errno));

        return;
    }

    // As per spec: https://tools.ietf.org/html/rfc6525#section-4.1
    len = sizeof(sctp_assoc_t) + (2 + 1) * sizeof(uint16_t);

    auto* srs = static_cast<struct sctp_reset_streams*>(std::malloc(len));

    switch (direction)
    {
        case StreamDirection::INCOMING:
            srs->srs_flags = SCTP_STREAM_RESET_INCOMING;
            break;

        case StreamDirection::OUTGOING:
            srs->srs_flags = SCTP_STREAM_RESET_OUTGOING;
            break;
    }

    srs->srs_number_streams = 1;
    srs->srs_stream_list[0] = streamId;    // No need for htonl().

    ret = usrsctp_setsockopt(this->socket, IPPROTO_SCTP, SCTP_RESET_STREAMS, srs, len);

    if (ret == 0)
    {
        LOG_DEBUG("SCTP_RESET_STREAMS sent [streamId:{} ]", streamId);
    }
    else
    {
        LOG_WARN("usrsctp_setsockopt(SCTP_RESET_STREAMS) failed: {}", std::strerror(errno));
    }

    std::free(srs);
}

void SctpAssociation::AddOutgoingStreams(bool force)
{
    uint16_t additionalOs{0};

    if (MaxSctpStreams - this->os >= 32)
        additionalOs = 32;
    else
        additionalOs = MaxSctpStreams - this->os;

    if (additionalOs == 0)
    {
        LOG_WARN("cannot add more outgoing streams [OS:%{}]", this->os);

        return;
    }

    auto nextDesiredOs = this->os + additionalOs;

    // Already in progress, ignore (unless forced).
    if (!force && nextDesiredOs == this->desiredOs)
        return;

    // Update desired value.
    this->desiredOs = nextDesiredOs;

    // If not connected, defer it.
    if (this->state != SctpState::CONNECTED)
    {
        LOG_DEBUG("SCTP not connected, deferring OS increase");

        return;
    }

    struct sctp_add_streams sas;    // NOLINT(cppcoreguidelines-pro-type-member-init)

    std::memset(&sas, 0, sizeof(sas));
    sas.sas_instrms = 0;
    sas.sas_outstrms = additionalOs;

    LOG_DEBUG("adding {} outgoing streams", additionalOs);

    int ret = usrsctp_setsockopt(this->socket, IPPROTO_SCTP, SCTP_ADD_STREAMS, &sas, static_cast<socklen_t>(sizeof(sas)));

    if (ret < 0)
        LOG_WARN("usrsctp_setsockopt(SCTP_ADD_STREAMS) failed: {}", std::strerror(errno));
}

void SctpAssociation::OnUsrSctpSendSctpData(void* buffer, size_t len)
{
    const uint8_t* data = static_cast<uint8_t*>(buffer);

#if MS_LOG_DEV_LEVEL == 3
    MS_DUMP_DATA(data, len);
#endif

    this->listener->OnSctpAssociationSendData(this, data, len);
}

void SctpAssociation::OnUsrSctpReceiveSctpData(uint16_t streamId, uint16_t ssn, uint32_t ppid, int flags, const uint8_t* data, size_t len)
{
    // Ignore WebRTC DataChannel Control DATA chunks.
    if (ppid == 50)
    {
        LOG_WARN("ignoring SCTP data with ppid:50 (WebRTC DataChannel Control)");

        return;
    }

    if (this->messageBufferLen != 0 && ssn != this->lastSsnReceived)
    {
        LOG_WARN("message chunk received with different SSN while buffer not empty, buffer discarded [ssn:{}, last ssn received:{}]", ssn, this->lastSsnReceived);

        this->messageBufferLen = 0;
    }

    // Update last SSN received.
    this->lastSsnReceived = ssn;

    auto eor = static_cast<bool>(flags & MSG_EOR);

    if (this->messageBufferLen + len > this->maxSctpMessageSize)
    {
        LOG_WARN("ongoing received message exceeds max allowed message size [message size:{}, max message size:{}, eor:{}]", this->messageBufferLen + len, this->maxSctpMessageSize, eor ? 1 : 0);

        this->lastSsnReceived = 0;

        return;
    }

    // If end of message and there is no buffered data, notify it directly.
    if (eor && this->messageBufferLen == 0)
    {
        LOG_DEBUG("directly notifying listener [eor:1, buffer len:0]");

        this->listener->OnSctpAssociationMessageReceived(this, streamId, ppid, data, len);
    }
    // If end of message and there is buffered data, append data and notify buffer.
    else if (eor && this->messageBufferLen != 0)
    {
        std::memcpy(this->messageBuffer + this->messageBufferLen, data, len);
        this->messageBufferLen += len;

        LOG_DEBUG("notifying listener [eor:1, buffer len:{}]", this->messageBufferLen);

        this->listener->OnSctpAssociationMessageReceived(this, streamId, ppid, this->messageBuffer, this->messageBufferLen);

        this->messageBufferLen = 0;
    }
    // If non end of message, append data to the buffer.
    else if (!eor)
    {
        // Allocate the buffer if not already done.
        if (!this->messageBuffer)
            this->messageBuffer = new uint8_t[this->maxSctpMessageSize];

        std::memcpy(this->messageBuffer + this->messageBufferLen, data, len);
        this->messageBufferLen += len;

        LOG_DEBUG("data buffered [eor:0, buffer len:{}]", this->messageBufferLen);
    }
}

void SctpAssociation::OnUsrSctpReceiveSctpNotification(union sctp_notification* notification, size_t len)
{
    if (notification->sn_header.sn_length != (uint32_t)len)
        return;

    switch (notification->sn_header.sn_type)
    {
        case SCTP_ADAPTATION_INDICATION:
        {
            LOG_DEBUG("SCTP adaptation indication [{}]", notification->sn_adaptation_event.sai_adaptation_ind);

            break;
        }

        case SCTP_ASSOC_CHANGE:
        {
            switch (notification->sn_assoc_change.sac_state)
            {
                case SCTP_COMM_UP:
                {
                    LOG_DEBUG("SCTP association connected, streams [out:{}, in:{}]", notification->sn_assoc_change.sac_outbound_streams, notification->sn_assoc_change.sac_inbound_streams);

                    // Update our OS.
                    this->os = notification->sn_assoc_change.sac_outbound_streams;

                    // Increase if requested before connected.
                    if (this->desiredOs > this->os)
                        AddOutgoingStreams(/*force*/ true);

                    if (this->state != SctpState::CONNECTED)
                    {
                        this->state = SctpState::CONNECTED;
                        this->listener->OnSctpAssociationConnected(this);
                    }

                    break;
                }

                case SCTP_COMM_LOST:
                {
                    if (notification->sn_header.sn_length > 0)
                    {
                        static const size_t BufferSize{1024};
                        thread_local static char buffer[BufferSize];

                        const uint32_t len = notification->sn_assoc_change.sac_length - sizeof(struct sctp_assoc_change);

                        for (uint32_t i{0}; i < len; ++i)
                        {
                            std::snprintf(buffer, BufferSize, " 0x%02x", notification->sn_assoc_change.sac_info[i]);
                        }

                        LOG_DEBUG("SCTP communication lost [info:{}]", buffer);
                    }
                    else
                    {
                        LOG_DEBUG("SCTP communication lost");
                    }

                    if (this->state != SctpState::CLOSED)
                    {
                        this->state = SctpState::CLOSED;
                        this->listener->OnSctpAssociationClosed(this);
                    }

                    break;
                }

                case SCTP_RESTART:
                {
                    LOG_DEBUG("SCTP remote association restarted, streams [out:{}, int:{}]", notification->sn_assoc_change.sac_outbound_streams, notification->sn_assoc_change.sac_inbound_streams);

                    // Update our OS.
                    this->os = notification->sn_assoc_change.sac_outbound_streams;

                    // Increase if requested before connected.
                    if (this->desiredOs > this->os)
                        AddOutgoingStreams(/*force*/ true);

                    if (this->state != SctpState::CONNECTED)
                    {
                        this->state = SctpState::CONNECTED;
                        this->listener->OnSctpAssociationConnected(this);
                    }

                    break;
                }

                case SCTP_SHUTDOWN_COMP:
                {
                    LOG_DEBUG("SCTP association gracefully closed");

                    if (this->state != SctpState::CLOSED)
                    {
                        this->state = SctpState::CLOSED;
                        this->listener->OnSctpAssociationClosed(this);
                    }

                    break;
                }

                case SCTP_CANT_STR_ASSOC:
                {
                    if (notification->sn_header.sn_length > 0)
                    {
                        static const size_t BufferSize{1024};
                        thread_local static char buffer[BufferSize];

                        const uint32_t len = notification->sn_assoc_change.sac_length - sizeof(struct sctp_assoc_change);

                        for (uint32_t i{0}; i < len; ++i)
                        {
                            std::snprintf(buffer, BufferSize, " 0x%02x", notification->sn_assoc_change.sac_info[i]);
                        }

                        LOG_WARN("SCTP setup failed: {}", buffer);
                    }

                    if (this->state != SctpState::FAILED)
                    {
                        this->state = SctpState::FAILED;
                        this->listener->OnSctpAssociationFailed(this);
                    }

                    break;
                }

                default:;
            }

            break;
        }

        // https://tools.ietf.org/html/rfc6525#section-6.1.2.
        case SCTP_ASSOC_RESET_EVENT:
        {
            LOG_DEBUG("SCTP association reset event received");

            break;
        }

        // An Operation Error is not considered fatal in and of itself, but may be
        // used with an ABORT chunk to report a fatal condition.
        case SCTP_REMOTE_ERROR:
        {
            static const size_t BufferSize{1024};
            thread_local static char buffer[BufferSize];

            const uint32_t len = notification->sn_remote_error.sre_length - sizeof(struct sctp_remote_error);

            for (uint32_t i{0}; i < len; i++)
            {
                std::snprintf(buffer, BufferSize, "0x%02x", notification->sn_remote_error.sre_data[i]);
            }

            LOG_WARN("remote SCTP association error [type:{}, data:{}]", notification->sn_remote_error.sre_error, buffer);

            break;
        }

        // When a peer sends a SHUTDOWN, SCTP delivers this notification to
        // inform the application that it should cease sending data.
        case SCTP_SHUTDOWN_EVENT:
        {
            LOG_DEBUG("remote SCTP association shutdown");

            if (this->state != SctpState::CLOSED)
            {
                this->state = SctpState::CLOSED;
                this->listener->OnSctpAssociationClosed(this);
            }

            break;
        }

        case SCTP_SEND_FAILED_EVENT:
        {
            static const size_t BufferSize{1024};
            thread_local static char buffer[BufferSize];

            const uint32_t len = notification->sn_send_failed_event.ssfe_length - sizeof(struct sctp_send_failed_event);

            for (uint32_t i{0}; i < len; ++i)
            {
                std::snprintf(buffer, BufferSize, "0x%02x", notification->sn_send_failed_event.ssfe_data[i]);
            }

            LOG_WARN("SCTP message sent failure [streamId:{}, ppid:{}, sent:{}, error:{}, info:{}]",
                     notification->sn_send_failed_event.ssfe_info.snd_sid,
                     ntohl(notification->sn_send_failed_event.ssfe_info.snd_ppid),
                     (notification->sn_send_failed_event.ssfe_flags & SCTP_DATA_SENT) ? "yes" : "no",
                     notification->sn_send_failed_event.ssfe_error,
                     buffer);

            break;
        }

        case SCTP_STREAM_RESET_EVENT:
        {
            bool incoming{false};
            bool outgoing{false};
            const uint16_t numStreams = (notification->sn_strreset_event.strreset_length - sizeof(struct sctp_stream_reset_event)) / sizeof(uint16_t);

            if (notification->sn_strreset_event.strreset_flags & SCTP_STREAM_RESET_INCOMING_SSN)
                incoming = true;

            if (notification->sn_strreset_event.strreset_flags & SCTP_STREAM_RESET_OUTGOING_SSN)
                outgoing = true;

            if (false)
            {
                std::string streamIds;

                for (uint16_t i{0}; i < numStreams; ++i)
                {
                    auto streamId = notification->sn_strreset_event.strreset_stream_list[i];

                    // Don't log more than 5 stream ids.
                    if (i > 4)
                    {
                        streamIds.append("...");

                        break;
                    }

                    if (i > 0)
                        streamIds.append(",");

                    streamIds.append(std::to_string(streamId));
                }

                LOG_DEBUG("SCTP stream reset event [flags:{}, i|o:{}|{}, num streams:{}, stream ids:{}]",
                          notification->sn_strreset_event.strreset_flags,
                          incoming ? "true" : "false",
                          outgoing ? "true" : "false",
                          numStreams,
                          streamIds);
            }

            // Special case for WebRTC DataChannels in which we must also reset our
            // outgoing SCTP stream.
            if (incoming && !outgoing && this->isDataChannel)
            {
                for (uint16_t i{0}; i < numStreams; ++i)
                {
                    auto streamId = notification->sn_strreset_event.strreset_stream_list[i];

                    ResetSctpStream(streamId, StreamDirection::OUTGOING);
                }
            }

            break;
        }

        case SCTP_STREAM_CHANGE_EVENT:
        {
            if (notification->sn_strchange_event.strchange_flags == 0)
            {
                LOG_DEBUG("SCTP stream changed, streams [out:{}, in:{}, flags:{}]",
                          notification->sn_strchange_event.strchange_outstrms,
                          notification->sn_strchange_event.strchange_instrms,
                          notification->sn_strchange_event.strchange_flags);
            }
            else if (notification->sn_strchange_event.strchange_flags & SCTP_STREAM_RESET_DENIED)
            {
                LOG_WARN("SCTP stream change denied, streams [out:{}, in:{}, flags:{}]",
                         notification->sn_strchange_event.strchange_outstrms,
                         notification->sn_strchange_event.strchange_instrms,
                         notification->sn_strchange_event.strchange_flags);

                break;
            }
            else if (notification->sn_strchange_event.strchange_flags & SCTP_STREAM_RESET_FAILED)
            {
                LOG_WARN("SCTP stream change failed, streams [out:{}, in:{}, flags:{}]",
                         notification->sn_strchange_event.strchange_outstrms,
                         notification->sn_strchange_event.strchange_instrms,
                         notification->sn_strchange_event.strchange_flags);

                break;
            }

            // Update OS.
            this->os = notification->sn_strchange_event.strchange_outstrms;

            break;
        }

        default:
        {
            LOG_WARN("unhandled SCTP event received [type:{}]", notification->sn_header.sn_type);
        }
    }
}

void SctpAssociation::OnUsrSctpSentData(uint32_t freeBuffer)
{
    auto previousSctpBufferedAmount = this->sctpBufferedAmount;

    this->sctpBufferedAmount = this->sctpSendBufferSize - freeBuffer;

    if (this->sctpBufferedAmount != previousSctpBufferedAmount)
    {
        this->listener->OnSctpAssociationBufferedAmount(this, this->sctpBufferedAmount);
    }
}
}    // namespace RTC
