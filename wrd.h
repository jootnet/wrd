#ifndef WRD_H
#define WRD_H

// WebRTC for Remote Desktop
// 我们把外部接口尽量简单化
// 外部只需要关注以下几点：
//	0:控制端和被控端生命周期（创建和销毁、各种回调和必要信息）
//	1:sdp收发
//	2:candidate收发
//	3:控制端收取被控端桌面画面
//	4:自定义数据收发
// 1和2是必要的，因为需要信令服务器转发，用于建立隧道
// 对于远程桌面，3必须，4是用于传输键鼠动作和共享文件的
// 而webrtc本身的各种细节，比如SetLocalSDP以及隧道建立过程中的状态变化等外部根本无需关心
//	作为控制端，连接1分钟都没有出画面，用户肯定就关窗口重连了（或者由程序提示失败）
//	作为被控端，则直接可以不关心，可以等到控制端动鼠标，用户也就感知了

#include <stdbool.h>

#ifdef _WINDLL
#define EXPORT_DEC __declspec(dllexport)
#else
#define EXPORT_DEC
#endif

typedef struct _tagWRDClient* WRDClient;

typedef void(__cdecl* WRDSDPGenCallback)(const WRDClient client, const char* sdp, void* usr_param);

typedef void(__cdecl* WRDCandidateGatheringCallback)(const WRDClient client, const char* mid, int mline_index, const char* candidate, void* usr_param);

typedef void(__cdecl* WRDRemoteImageReceivedCallback)(const WRDClient client, const unsigned char* argb, int w, int h, void* usr_param);

typedef void(__cdecl* WRDRemoteRawDataReceivedCallback)(const WRDClient client, const unsigned char* data, unsigned int data_len, void* usr_param);

typedef void(__cdecl* WRDRemoteStringReceivedCallback)(const WRDClient client, const char* data, void* usr_param);


extern "C" EXPORT_DEC WRDClient __cdecl wrdCreateViewer();
extern "C" EXPORT_DEC WRDClient __cdecl wrdCreateMaster();

extern "C" EXPORT_DEC void __cdecl wrdGenSDP(WRDClient client, WRDSDPGenCallback gen_done_cb, void* usr_param);

extern "C" EXPORT_DEC void __cdecl wrdOnCandidateGathering(WRDClient client, WRDCandidateGatheringCallback candidate_gathering_cb, void* usr_param);

extern "C" EXPORT_DEC bool __cdecl wrdSetRemoteSDP(WRDClient client, const char* sdp);

extern "C" EXPORT_DEC bool __cdecl wrdAddRemoteCandidate(WRDClient client, const char* mid, int mline_index, const char* candidate);

extern "C" EXPORT_DEC void __cdecl wrdOnRemoteImageReceived(WRDClient client, WRDRemoteImageReceivedCallback image_received_cb, void* usr_param);

extern "C" EXPORT_DEC void __cdecl wrdOnRemoteRawDataReceived(WRDClient client, WRDRemoteRawDataReceivedCallback raw_data_received_cb, void* usr_param);

extern "C" EXPORT_DEC void __cdecl wrdOnRemoteStringReceived(WRDClient client, WRDRemoteStringReceivedCallback string_received_cb, void* usr_param);

extern "C" EXPORT_DEC void __cdecl wrdSendRawData(WRDClient client, const unsigned char* data, unsigned int data_len);

extern "C" EXPORT_DEC void __cdecl wrdSendString(WRDClient client, const char* data);

extern "C" EXPORT_DEC void __cdecl wrdClientDestroy(WRDClient * client);
#endif//WRD_H