#ifndef WRD_H
#define WRD_H

// WebRTC for Remote Desktop
// ���ǰ��ⲿ�ӿھ����򵥻�
// �ⲿֻ��Ҫ��ע���¼��㣺
//	0:���ƶ˺ͱ��ض��������ڣ����������١����ֻص��ͱ�Ҫ��Ϣ��
//	1:sdp�շ�
//	2:candidate�շ�
//	3:���ƶ���ȡ���ض����滭��
//	4:�Զ��������շ�
// 1��2�Ǳ�Ҫ�ģ���Ϊ��Ҫ���������ת�������ڽ������
// ����Զ�����棬3���룬4�����ڴ���������͹����ļ���
// ��webrtc����ĸ���ϸ�ڣ�����SetLocalSDP�Լ�������������е�״̬�仯���ⲿ�����������
//	��Ϊ���ƶˣ�����1���Ӷ�û�г����棬�û��϶��͹ش��������ˣ������ɳ�����ʾʧ�ܣ�
//	��Ϊ���ضˣ���ֱ�ӿ��Բ����ģ����Եȵ����ƶ˶���꣬�û�Ҳ�͸�֪��

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