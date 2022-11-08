#include "wrd_internal.h"

WRDClient wrdCreateViewer() {
	rtc::LogMessage::LogToDebug(rtc::LS_WARNING);
	return (WRDClient) new wrdSession(false);
}

WRDClient wrdCreateMaster() {
	rtc::LogMessage::LogToDebug(rtc::LS_WARNING);
	return (WRDClient) new wrdSession(true);
}

void wrdGenSDP(WRDClient client, WRDSDPGenCallback gen_done_cb, void* usr_param) {
	auto session = reinterpret_cast<wrdSession*>(client);
	session->GenSDP(gen_done_cb, usr_param);
}

void wrdOnCandidateGathering(WRDClient client, WRDCandidateGatheringCallback candidate_gathering_cb, void* usr_param) {
	auto session = reinterpret_cast<wrdSession*>(client);
	session->OnCandidateGathering(candidate_gathering_cb, usr_param);
}

bool wrdSetRemoteSDP(WRDClient client, const char* sdp) {
	auto session = reinterpret_cast<wrdSession*>(client);
	webrtc::SdpParseError error;
	std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
		webrtc::CreateSessionDescription(session->IsMaster() ? webrtc::SdpType::kAnswer : webrtc::SdpType::kOffer, std::string(sdp), &error);
	if (!session_description) {
		RTC_LOG(LS_WARNING)
			<< "Can't parse received session description message. "
			"SdpParseError was: "
			<< error.description;
		return false;
	}
	session->SetRemoteSDP(session_description.release());
	return true;
}

bool wrdAddRemoteCandidate(WRDClient client, const char* sdp_mid, int sdp_mlineindex, const char* sdp) {
	auto session = reinterpret_cast<wrdSession*>(client);
	webrtc::SdpParseError error;
	std::unique_ptr<webrtc::IceCandidateInterface> candidate(
		webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, std::string(sdp), &error));
	if (!candidate.get()) {
		RTC_LOG(LS_WARNING) << "Can't parse received candidate message. "
			"SdpParseError was: "
			<< error.description;
		return false;
	}
	session->AddRemoteCandidate(candidate.get());
	return true;
}

void wrdOnRemoteImageReceived(WRDClient client, WRDRemoteImageReceivedCallback image_received_cb, void* usr_param) {
	auto session = reinterpret_cast<wrdSession*>(client);
	session->OnRemoteImageReceived(image_received_cb, usr_param);
}

void wrdOnRemoteRawDataReceived(WRDClient client, WRDRemoteRawDataReceivedCallback raw_data_received_cb, void* usr_param) {
	auto session = reinterpret_cast<wrdSession*>(client);
	session->OnRemoteRawDataReceived(raw_data_received_cb, usr_param);
}

void wrdOnRemoteStringReceived(WRDClient client, WRDRemoteStringReceivedCallback string_received_cb, void* usr_param) {
	auto session = reinterpret_cast<wrdSession*>(client);
	session->OnRemoteStringReceived(string_received_cb, usr_param);
}

void wrdSendRawData(WRDClient client, const unsigned char* data, unsigned int data_len) {
	auto session = reinterpret_cast<wrdSession*>(client);
	session->SendData(data, data_len);
}

void wrdSendString(WRDClient client, const char* data) {
	auto session = reinterpret_cast<wrdSession*>(client);
	session->SendString(data);
}

void wrdClientDestroy(WRDClient* client) {
	auto session = reinterpret_cast<wrdSession**>(client);
	delete* session;
	session = nullptr;
}