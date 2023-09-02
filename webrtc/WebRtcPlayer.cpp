#include "webrtc/WebRtcPlayer.hpp"

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
