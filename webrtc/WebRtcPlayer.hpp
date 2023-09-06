#ifndef SIMPLE_RTMP_WEBRTCPLAYER_HPP
#define SIMPLE_RTMP_WEBRTCPLAYER_HPP

#include <memory>
#include "webrtc/DtlsTransport.hpp"
#include "webrtc/IceServer.hpp"
#include "webrtc_sdp.h"

class WebRtcPlayer : public RTC::DtlsTransport::Listener, public RTC::IceServer::Listener, public std::enable_shared_from_this<WebRtcPlayer>
{
   public:
    explicit WebRtcPlayer(const std::shared_ptr<simple_rtmp::webrtc_sdp>& sdp);
    ~WebRtcPlayer() override = default;

   public:
    void OnDtlsTransportConnecting(const RTC::DtlsTransport* dtlsTransport) override;
    void OnDtlsTransportConnected(const RTC::DtlsTransport* dtlsTransport,
                                  RTC::SrtpSession::CryptoSuite srtpCryptoSuite,
                                  uint8_t* srtpLocalKey,
                                  size_t srtpLocalKeyLen,
                                  uint8_t* srtpRemoteKey,
                                  size_t srtpRemoteKeyLen,
                                  std::string& remoteCert) override;

    void OnDtlsTransportFailed(const RTC::DtlsTransport* dtlsTransport) override;
    void OnDtlsTransportClosed(const RTC::DtlsTransport* dtlsTransport) override;
    void OnDtlsTransportSendData(const RTC::DtlsTransport* dtlsTransport, const uint8_t* data, size_t len) override;
    void OnDtlsTransportApplicationDataReceived(const RTC::DtlsTransport* dtlsTransport, const uint8_t* data, size_t len) override;

   public:
    void OnIceServerSendStunPacket(const RTC::IceServer* iceServer, const RTC::StunPacket* packet, RTC::TransportTuple* tuple) override;
    void OnIceServerLocalUsernameFragmentAdded(const RTC::IceServer* iceServer, const std::string& usernameFragment) override;
    void OnIceServerLocalUsernameFragmentRemoved(const RTC::IceServer* iceServer, const std::string& usernameFragment) override;
    void OnIceServerTupleAdded(const RTC::IceServer* iceServer, RTC::TransportTuple* tuple) override;
    void OnIceServerTupleRemoved(const RTC::IceServer* iceServer, RTC::TransportTuple* tuple) override;
    void OnIceServerSelectedTuple(const RTC::IceServer* iceServer, RTC::TransportTuple* tuple) override;
    void OnIceServerConnected(const RTC::IceServer* iceServer) override;
    void OnIceServerCompleted(const RTC::IceServer* iceServer) override;
    void OnIceServerDisconnected(const RTC::IceServer* iceServer) override;

   public:
    void exchangeSdp();

   private:
    std::string id_;
    std::shared_ptr<RTC::DtlsTransport> dtls_transport_;
    std::shared_ptr<RTC::IceServer> ice_server_;
    std::shared_ptr<simple_rtmp::webrtc_sdp> sdp_;
};

#endif
