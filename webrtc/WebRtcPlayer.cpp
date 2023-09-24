#include <string>
#include <random>
#include <utility>
#include <thread>
#include <algorithm>
#include <atomic>
#include "webrtc/WebRtcPlayer.hpp"
#include "log.h"

static std::string getPlayerId()
{
    static std::atomic<uint64_t> player_key{0};
    return "simple_rtmp_" + std::to_string(player_key);
}

static std::string randomString()
{
    static std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    static thread_local std::mt19937 generator(std::random_device{}());
    std::shuffle(str.begin(), str.end(), generator);
    return str.substr(0, 24);
}

WebRtcPlayer::WebRtcPlayer(const std::shared_ptr<simple_rtmp::webrtc_sdp>& sdp) : id_(randomString())
{
    dtls_transport_ = std::make_shared<RTC::DtlsTransport>(this);
    std::string username_fragment = id_;
    std::string password = id_;
    ice_server_ = std::make_shared<RTC::IceServer>(this, username_fragment, password);
    sdp_ = sdp;
}

void WebRtcPlayer::exchangeSdp()
{
    sdp_->parse();
    std::string algorithm;
    std::string fingerprint;
    int ret = sdp_->fingerprint_algorithm(algorithm, fingerprint);
    if (ret != 0)
    {
        LOG_ERROR("{} fingerprint algorithm failed", id_);
        return;
    }
    RTC::DtlsTransport::Fingerprint remote_fingerprint;
    remote_fingerprint.algorithm = RTC::DtlsTransport::GetFingerprintAlgorithm(algorithm);
    remote_fingerprint.value = fingerprint;
    dtls_transport_->SetRemoteFingerprint(remote_fingerprint);
    //
}

void WebRtcPlayer::OnDtlsTransportConnecting(const RTC::DtlsTransport* dtlsTransport)
{
}
void WebRtcPlayer::OnDtlsTransportConnected(const RTC::DtlsTransport* dtlsTransport,
                                            RTC::SrtpSession::CryptoSuite srtpCryptoSuite,
                                            uint8_t* srtpLocalKey,
                                            size_t srtpLocalKeyLen,
                                            uint8_t* srtpRemoteKey,
                                            size_t srtpRemoteKeyLen,
                                            std::string& remoteCert)
{
}

void WebRtcPlayer::OnDtlsTransportFailed(const RTC::DtlsTransport* dtlsTransport)
{
}
void WebRtcPlayer::OnDtlsTransportClosed(const RTC::DtlsTransport* dtlsTransport)
{
}
void WebRtcPlayer::OnDtlsTransportSendData(const RTC::DtlsTransport* dtlsTransport, const uint8_t* data, size_t len)
{
}
void WebRtcPlayer::OnDtlsTransportApplicationDataReceived(const RTC::DtlsTransport* dtlsTransport, const uint8_t* data, size_t len)
{
}

void WebRtcPlayer::OnIceServerSendStunPacket(const RTC::IceServer* iceServer, const RTC::StunPacket* packet, RTC::TransportTuple* tuple)
{
}
void WebRtcPlayer::OnIceServerLocalUsernameFragmentAdded(const RTC::IceServer* iceServer, const std::string& usernameFragment)
{
}
void WebRtcPlayer::OnIceServerLocalUsernameFragmentRemoved(const RTC::IceServer* iceServer, const std::string& usernameFragment)
{
}
void WebRtcPlayer::OnIceServerTupleAdded(const RTC::IceServer* iceServer, RTC::TransportTuple* tuple)
{
}
void WebRtcPlayer::OnIceServerTupleRemoved(const RTC::IceServer* iceServer, RTC::TransportTuple* tuple)
{
}
void WebRtcPlayer::OnIceServerSelectedTuple(const RTC::IceServer* iceServer, RTC::TransportTuple* tuple)
{
}
void WebRtcPlayer::OnIceServerConnected(const RTC::IceServer* iceServer)
{
}
void WebRtcPlayer::OnIceServerCompleted(const RTC::IceServer* iceServer)
{
}
void WebRtcPlayer::OnIceServerDisconnected(const RTC::IceServer* iceServer)
{
}
