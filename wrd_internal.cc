#include "wrd_internal.h"

#include "api/create_peerconnection_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"

static std::unique_ptr<rtc::Thread> signaling_thread_;
static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;

wrdSession::wrdSession(bool is_master) : is_master_(is_master) {
	if (!signaling_thread_.get()) {
		signaling_thread_ = rtc::Thread::CreateWithSocketServer();
		signaling_thread_->Start();

		peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
			nullptr /* network_thread */, nullptr /* worker_thread */,
			signaling_thread_.get(), nullptr /* default_adm */,
			webrtc::CreateBuiltinAudioEncoderFactory(),
			webrtc::CreateBuiltinAudioDecoderFactory(),
			webrtc::CreateBuiltinVideoEncoderFactory(),
			webrtc::CreateBuiltinVideoDecoderFactory(), nullptr /* audio_mixer */,
			nullptr /* audio_processing */);
	}

	webrtc::PeerConnectionInterface::RTCConfiguration config;
	config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
	{
		webrtc::PeerConnectionInterface::IceServer server;
		server.uri = "stun:openrelay.metered.ca:80";
		config.servers.push_back(server);
	}
	{
		webrtc::PeerConnectionInterface::IceServer server;
		server.uri = "turn:openrelay.metered.ca:80";
		server.username = "openrelayproject";
		server.password = "openrelayproject";
		config.servers.push_back(server);
	}
	{
		webrtc::PeerConnectionInterface::IceServer server;
		server.uri = "turn:openrelay.metered.ca:443";
		server.username = "openrelayproject";
		server.password = "openrelayproject";
		config.servers.push_back(server);
	}
	{
		webrtc::PeerConnectionInterface::IceServer server;
		server.uri = "turn:openrelay.metered.ca:443?transport=tcp";
		server.username = "openrelayproject";
		server.password = "openrelayproject";
		config.servers.push_back(server);
	}

	peer_connection_observer_ = new wrdPeerConnectionObserver(this);
	webrtc::PeerConnectionDependencies pc_dependencies(peer_connection_observer_);
	auto peer_connection_or_error = peer_connection_factory_->CreatePeerConnectionOrError(config, std::move(pc_dependencies));
	if (!peer_connection_or_error.ok()) {
		RTC_LOG(LS_ERROR) << "Failed to Create PeerConnection: "
			<< peer_connection_or_error.error().message();
		return;
	}
	peer_connection_ = peer_connection_or_error.value();

	data_channel_observer_ = new wrdDataChannelObserver(this);
	if (is_master) {
		desktop_capture_ = wrdDesktopCapture::Create(16, 0);
		if (desktop_capture_) {
			rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_(
				peer_connection_factory_->CreateVideoTrack("video_label",
					desktop_capture_.get()));

			auto result_or_error = peer_connection_->AddTrack(video_track_, { "stream_id" });
			if (!result_or_error.ok()) {
				RTC_LOG(LS_ERROR) << "Failed to add video track to PeerConnection: "
					<< result_or_error.error().message();
			}

			webrtc::DataChannelInit data_channel_init;
			auto data_channel_or_error = peer_connection_->CreateDataChannelOrError("data_channel_label", &data_channel_init);
			if (!data_channel_or_error.ok()) {
				RTC_LOG(LS_ERROR) << "Failed to Create DataChannel: "
					<< data_channel_or_error.error().message();
				return;
			}
			data_channel_ = data_channel_or_error.value();
			data_channel_->RegisterObserver(data_channel_observer_);
		}
	}

	create_session_description_observer_ = rtc::make_ref_counted<wrdCreateSessionDescriptionObserver>(this);
	remote_video_renderer_ = new wrdVideoRenderer(this, 1, 1);
}

wrdSession::~wrdSession() {
	if (peer_connection_.get()) {
		peer_connection_->Close();
		peer_connection_.release();
	}

	if (nullptr != peer_connection_observer_) {
		delete peer_connection_observer_;
		peer_connection_observer_ = nullptr;
	}
	if (nullptr != data_channel_observer_) {
		delete data_channel_observer_;
		data_channel_observer_ = nullptr;
	}
	if (create_session_description_observer_.get()) {
		create_session_description_observer_.release();
	}
	if (desktop_capture_.get()) {
		desktop_capture_.release();
	}
	if (nullptr != remote_video_renderer_) {
		delete remote_video_renderer_;
		remote_video_renderer_ = nullptr;
	}
}

void wrdSession::GenSDP(WRDSDPGenCallback gen_done_cb, void* usr_param) {
	sdp_gen_callback_ = gen_done_cb;
	sdp_gen_callback_usr_ = usr_param;
	if (is_master_) {
		peer_connection_->CreateOffer(
			create_session_description_observer_.get(), webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
	}
	else {
		peer_connection_->CreateAnswer(
			create_session_description_observer_.get(), webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
	}
}

void wrdSession::OnCandidateGathering(WRDCandidateGatheringCallback candidate_gathering_cb, void* usr_param) {
	candidate_gathering_callback_ = candidate_gathering_cb;
	candidate_gathering_callback_usr_ = usr_param;
}

bool wrdSession::IsMaster() const {
	return is_master_;
}

void wrdSession::SetRemoteSDP(webrtc::SessionDescriptionInterface* session_description) const {
	peer_connection_->SetRemoteDescription(
		DummySetSessionDescriptionObserver::Create().get(),
		session_description);
}

void wrdSession::AddRemoteCandidate(webrtc::IceCandidateInterface* candidate) const {
	if (!peer_connection_->AddIceCandidate(candidate)) {
		RTC_LOG(LS_ERROR) << "Failed to apply the received candidate";
	}
}

void wrdSession::OnRemoteImageReceived(WRDRemoteImageReceivedCallback image_received_cb, void* usr_param) {
	remote_image_received_callback_ = image_received_cb;
	remote_image_received_callback_usr_ = usr_param;
}

void wrdSession::OnRemoteRawDataReceived(WRDRemoteRawDataReceivedCallback raw_data_received_cb, void* usr_param) {
	remote_raw_data_received_callback_ = raw_data_received_cb;
	remote_raw_data_received_callback_usr_ = usr_param;
}

void wrdSession::OnRemoteStringReceived(WRDRemoteStringReceivedCallback string_received_cb, void* usr_param) {
	remote_string_received_callback_ = string_received_cb;
	remote_string_received_callback_usr_ = usr_param;
}

void wrdSession::SendData(const unsigned char* data, unsigned int data_len) const {
	if (data_channel_.get()) {
		data_channel_->Send(webrtc::DataBuffer{ rtc::CopyOnWriteBuffer { data, data_len }, true });
	}
}

void wrdSession::SendString(const std::string& data) const {
	if (data_channel_.get()) {
		data_channel_->Send(webrtc::DataBuffer{ data });
	}
}

const char kHello[] = "hello world!";

void wrdDataChannelObserver::OnStateChange() {
	if (!ses_->is_master_ && ses_->data_channel_->state() == webrtc::DataChannelInterface::DataState::kOpen) {
		ses_->data_channel_->Send(webrtc::DataBuffer{ rtc::CopyOnWriteBuffer { kHello, sizeof kHello }, false });
	}
}

void wrdDataChannelObserver::OnMessage(const webrtc::DataBuffer& buffer) {
	if (ses_->is_master_ && !hello_world_received_ && ::strcmp((const char*)buffer.data.data(), kHello) == 0) {
		ses_->desktop_capture_->StartCapture();
		hello_world_received_ = true;
		return;
	}
	if (buffer.binary && nullptr != ses_->remote_raw_data_received_callback_) {
		ses_->remote_raw_data_received_callback_((WRDClient)ses_, buffer.data.data(), buffer.data.size(), ses_->remote_raw_data_received_callback_usr_);
	}
	if (!buffer.binary && nullptr != ses_->remote_string_received_callback_) {
		ses_->remote_string_received_callback_((WRDClient)ses_, (const char*)buffer.data.data(), ses_->remote_string_received_callback_usr_);
	}
}

void wrdPeerConnectionObserver::OnDataChannel(
	rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
	data_channel->RegisterObserver(ses_->data_channel_observer_);
	ses_->data_channel_ = data_channel;
}

void wrdPeerConnectionObserver::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
	if (nullptr != ses_->candidate_gathering_callback_) {
		std::string sdp;
		candidate->ToString(&sdp);
		ses_->candidate_gathering_callback_((WRDClient)ses_, candidate->sdp_mid().c_str(), candidate->sdp_mline_index(), sdp.c_str(), ses_->candidate_gathering_callback_usr_);
	}
}

void wrdPeerConnectionObserver::OnAddTrack(
	rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
	const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) {
	auto track = receiver->track().release();
	if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
		auto* video_track = static_cast<webrtc::VideoTrackInterface*>(track);
		video_track->AddOrUpdateSink(ses_->remote_video_renderer_, rtc::VideoSinkWants());
	}
	track->Release();
}

void wrdPeerConnectionObserver::OnConnectionChange(
	webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
	RTC_LOG(LS_INFO) << "OnConnectionChange: "
		<< webrtc::PeerConnectionInterface::AsString(new_state);
}

void wrdCreateSessionDescriptionObserver::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
	ses_->peer_connection_->SetLocalDescription(
		DummySetSessionDescriptionObserver::Create().get(), desc);
	if (nullptr != ses_->sdp_gen_callback_) {
		std::string sdp;
		desc->ToString(&sdp);
		ses_->sdp_gen_callback_((WRDClient)ses_, sdp.c_str(), ses_->sdp_gen_callback_usr_);
	}
}

void wrdVideoRenderer::OnFrame(const webrtc::VideoFrame& video_frame) {
	rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(
		video_frame.video_frame_buffer()->ToI420());
	if (video_frame.rotation() != webrtc::kVideoRotation_0) {
		buffer = webrtc::I420Buffer::Rotate(*buffer, video_frame.rotation());
	}

	auto buffer_length = buffer->width() * buffer->height() * 4;
	if (buffer_length != width_ * height_ * 4) {
		image_.reset(new uint8_t[buffer_length]);
		width_ = buffer->width();
		height_ = buffer->height();
	}

	RTC_DCHECK(image_.get() != NULL);
	libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(), buffer->DataU(),
		buffer->StrideU(), buffer->DataV(), buffer->StrideV(),
		image_.get(),
		width_ * 4,
		buffer->width(), buffer->height());

	if (nullptr != ses_->remote_image_received_callback_) {
		ses_->remote_image_received_callback_((const WRDClient)ses_, image_.get(), width_, height_, ses_->remote_image_received_callback_usr_);
	}
}

wrdDesktopCapture::~wrdDesktopCapture() {
	StopCapture();
	if (!dc_)
		return;

	dc_.reset(nullptr);

}

webrtc::MediaSourceInterface::SourceState wrdDesktopCapture::state() const { return state_; }
bool wrdDesktopCapture::remote() const { return remote_; }

bool wrdDesktopCapture::is_screencast() const { return false; }
absl::optional<bool> wrdDesktopCapture::needs_denoising() const {
	return absl::nullopt;

}

bool wrdDesktopCapture::GetStats(Stats* stats) { return false; }

std::string wrdDesktopCapture::GetWindowTitle() const { return window_title_; }

void wrdDesktopCapture::StartCapture() {
	if (start_flag_) {
		RTC_LOG(LS_WARNING) << "Capture already been running...";
		return;

	}

	start_flag_ = true;

	// Start new thread to capture
	capture_thread_.reset(new std::thread([this]() {
		dc_->Start(this);

		while (start_flag_) {
			dc_->CaptureFrame();
			std::this_thread::sleep_for(std::chrono::milliseconds(1000 / fps_));

		}
		}));
	state_ = SourceState::kLive;

}

void wrdDesktopCapture::StopCapture() {
	start_flag_ = false;
	state_ = SourceState::kEnded;

	if (capture_thread_ && capture_thread_->joinable()) {
		capture_thread_->join();

	}

}

rtc::scoped_refptr<wrdDesktopCapture> wrdDesktopCapture::Create(
	size_t target_fps,
	size_t capture_screen_index) {
	auto dc = webrtc::DesktopCapturer::CreateScreenCapturer(
		webrtc::DesktopCaptureOptions::CreateDefault());

	if (!dc)
		return nullptr;

	webrtc::DesktopCapturer::SourceList sources;
	dc->GetSourceList(&sources);
	if (capture_screen_index > sources.size()) {
		RTC_LOG(LS_WARNING) << "The total sources of screen is " << sources.size()
			<< ", but require source of index at "
			<< capture_screen_index;
		return nullptr;

	}

	RTC_CHECK(dc->SelectSource(sources[capture_screen_index].id));
	auto window_title = sources[capture_screen_index].title;
	auto fps = target_fps;

	RTC_LOG(LS_INFO) << "Init DesktopCapture finish";
	// Start new thread to capture

	return rtc::make_ref_counted<wrdDesktopCapture>(std::move(dc), fps,
		std::move(window_title));
}

void wrdDesktopCapture::OnCaptureResult(
	webrtc::DesktopCapturer::Result result,
	std::unique_ptr<webrtc::DesktopFrame> desktopframe) {
	if (result != webrtc::DesktopCapturer::Result::SUCCESS)
		return;
	int width = desktopframe->size().width();
	int height = desktopframe->size().height();
	// int half_width = (width + 1) / 2;

	if (!i420_buffer_.get() ||
		i420_buffer_->width() * i420_buffer_->height() < width * height) {
		i420_buffer_ = webrtc::I420Buffer::Create(width, height);

	}

	int stride = width;
	uint8_t* yplane = i420_buffer_->MutableDataY();
	uint8_t* uplane = i420_buffer_->MutableDataU();
	uint8_t* vplane = i420_buffer_->MutableDataV();
	libyuv::ConvertToI420(desktopframe->data(), 0, yplane, stride, uplane,
		(stride + 1) / 2, vplane, (stride + 1) / 2, 0, 0,
		width, height, width, height, libyuv::kRotate0,
		libyuv::FOURCC_ARGB);
	webrtc::VideoFrame frame =
		webrtc::VideoFrame(i420_buffer_, 0, 0, webrtc::kVideoRotation_0);
	this->OnFrame(frame);

}