#ifndef WRD_INTERNAL_H
#define WRD_INTERNAL_H

#include "wrd.h"

#include <thread>

#include "api/peer_connection_interface.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "media/base/adapted_video_track_source.h"
#include "api/video/i420_buffer.h"
#include "third_party/libyuv/include/libyuv.h"

class wrdPeerConnectionObserver;
class wrdDataChannelObserver;
class wrdCreateSessionDescriptionObserver;
class wrdDesktopCapture;
class wrdVideoRenderer;
class wrdSession {
public:
	explicit wrdSession(bool/*is_master*/);
	~wrdSession();

	void GenSDP(WRDSDPGenCallback, void*);
	void OnCandidateGathering(WRDCandidateGatheringCallback, void*);
	bool IsMaster() const;
	void SetRemoteSDP(webrtc::SessionDescriptionInterface*) const;
	void AddRemoteCandidate(webrtc::IceCandidateInterface*) const;
	void OnRemoteImageReceived(WRDRemoteImageReceivedCallback, void*);
	void OnRemoteRawDataReceived(WRDRemoteRawDataReceivedCallback, void*);
	void OnRemoteStringReceived(WRDRemoteStringReceivedCallback, void*);
	void SendData(const unsigned char*, unsigned int) const;
	void SendString(const std::string&) const;
private:
	bool is_master_;
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
	rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;

	friend class wrdPeerConnectionObserver;
	friend class wrdDataChannelObserver;
	friend class wrdCreateSessionDescriptionObserver;
	friend class wrdDesktopCapture;
	friend class wrdVideoRenderer;
private:
	wrdPeerConnectionObserver* peer_connection_observer_;
	wrdDataChannelObserver* data_channel_observer_;
	rtc::scoped_refptr<wrdCreateSessionDescriptionObserver> create_session_description_observer_;

	rtc::scoped_refptr<wrdDesktopCapture> desktop_capture_;
	wrdVideoRenderer* remote_video_renderer_;

	WRDSDPGenCallback sdp_gen_callback_;
	void* sdp_gen_callback_usr_;
	WRDCandidateGatheringCallback candidate_gathering_callback_;
	void* candidate_gathering_callback_usr_;
	WRDRemoteImageReceivedCallback remote_image_received_callback_;
	void* remote_image_received_callback_usr_;
	WRDRemoteRawDataReceivedCallback remote_raw_data_received_callback_;
	void* remote_raw_data_received_callback_usr_;
	WRDRemoteStringReceivedCallback remote_string_received_callback_;
	void* remote_string_received_callback_usr_;
};

class DummySetSessionDescriptionObserver
	: public webrtc::SetSessionDescriptionObserver {
public:
	static rtc::scoped_refptr<DummySetSessionDescriptionObserver> Create() {
		return rtc::make_ref_counted<DummySetSessionDescriptionObserver>();
	}
	virtual void OnSuccess() { RTC_LOG(LS_INFO) << __FUNCTION__; }
	virtual void OnFailure(webrtc::RTCError error) {
		RTC_LOG(LS_INFO) << __FUNCTION__ << " " << ToString(error.type()) << ": "
			<< error.message();
	}
};

class wrdDataChannelObserver
	: public webrtc::DataChannelObserver {
public:
	explicit wrdDataChannelObserver(wrdSession* ses) : ses_(ses), hello_world_received_(false) {}

	void OnStateChange() override;
	void OnMessage(const webrtc::DataBuffer& buffer) override;
private:
	wrdSession* ses_;
	bool hello_world_received_;
};

class wrdPeerConnectionObserver
	: public webrtc::PeerConnectionObserver {
public:
	explicit wrdPeerConnectionObserver(wrdSession* ses) : ses_(ses) {}

	void OnSignalingChange(
		webrtc::PeerConnectionInterface::SignalingState new_state) override {}
	void OnDataChannel(
		rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;
	void OnIceGatheringChange(
		webrtc::PeerConnectionInterface::IceGatheringState new_state) override {}
	void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
	void OnAddTrack(
		rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
		const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override;
	void OnRemoveTrack(
		rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override {}
	void OnConnectionChange(
		webrtc::PeerConnectionInterface::PeerConnectionState new_state) override;
private:
	wrdSession* ses_;
};

class wrdCreateSessionDescriptionObserver
	: public webrtc::CreateSessionDescriptionObserver {
public:
	explicit wrdCreateSessionDescriptionObserver(wrdSession* ses) : ses_(ses) {}

	void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
	void OnFailure(webrtc::RTCError error) override {
		RTC_LOG(LS_INFO) << __FUNCTION__ << " " << ToString(error.type()) << ": "
			<< error.message();
	}
private:
	wrdSession* ses_;
};

class wrdVideoRenderer :
	public rtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
	explicit wrdVideoRenderer(wrdSession* ses, int width, int height) : ses_(ses), width_(width), height_(height) {
		image_.reset(new uint8_t[width * height * 4]);
	}

	void OnFrame(const webrtc::VideoFrame& video_frame) override;
private:
	wrdSession* ses_;
	int width_;
	int height_;
	std::unique_ptr<uint8_t[]> image_;
};

class wrdDesktopCapture : public rtc::AdaptedVideoTrackSource,
	public webrtc::DesktopCapturer::Callback {
public:
	~wrdDesktopCapture() override;
	//--RefCountedObject

	SourceState state() const override;
	bool remote() const override;

	bool is_screencast() const override;
	absl::optional<bool> needs_denoising() const override;

	bool GetStats(Stats* stats) override;
	//--RefCountedObject

	std::string GetWindowTitle() const;

	void StartCapture();

	void StopCapture();

	void OnCaptureResult(
		webrtc::DesktopCapturer::Result result,
		std::unique_ptr<webrtc::DesktopFrame> desktopframe) override;

	static rtc::scoped_refptr<wrdDesktopCapture> Create(
		size_t target_fps,
		size_t capture_screen_index);

protected:
	explicit wrdDesktopCapture(std::unique_ptr<webrtc::DesktopCapturer> dc,
		size_t fps,
		std::string window_title)
		: dc_(std::move(dc)),
		fps_(fps),
		window_title_(std::move(window_title)),
		start_flag_(false),
		state_(SourceState::kInitializing),
		remote_(false) {}

private:
	std::unique_ptr<webrtc::DesktopCapturer> dc_;
	size_t fps_;
	std::string window_title_;
	rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer_;
	std::unique_ptr<std::thread> capture_thread_;
	std::atomic_bool start_flag_;
	// RefCountedObject
	SourceState state_;
	const bool remote_;
};

#endif//WRD_INTERNAL_H