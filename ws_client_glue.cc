#include "wrd.h"

#include "ws_client.h"

WebsocketClient WebsocketClientCreate(const char* url) {
	WSClient* ws = new WSClient(url);
	return (WebsocketClient)ws;
}

void WebsocketClientOnStateChanged(WebsocketClient client, WebsocketClientStateCallback state_cb, void* usr_param) {
	auto ws = reinterpret_cast<WSClient*>(client);
	ws->SetConStateCb(std::bind([](bool state, WebsocketClient client, WebsocketClientStateCallback state_cb, void* usr_param) {
		state_cb(client, state, usr_param);
	}, std::placeholders::_1, client, state_cb, usr_param));
}

void WebsocketClientOnRecvChanged(WebsocketClient client, WebsocketClientRecvCallback recv_cb, void* usr_param) {
	auto ws = reinterpret_cast<WSClient*>(client);
	ws->SetDataCb(std::bind([](const std::string& data, WebsocketClient client, WebsocketClientRecvCallback recv_cb, void* usr_param) {
		recv_cb(client, data.data(), data.size(), usr_param);
	}, std::placeholders::_1, client, recv_cb, usr_param));
}

void WebsocketClientStart(WebsocketClient client) {
	auto ws = reinterpret_cast<WSClient*>(client);
	ws->Connect();
}

bool WebsocketClientIsConnect(WebsocketClient client) {
	auto ws = reinterpret_cast<WSClient*>(client);
	return ws->IsConnected();
}

void WebsocketClientSend(WebsocketClient client, const char* data) {
	auto ws = reinterpret_cast<WSClient*>(client);
	ws->Send(data);
}

void WebsocketClientCreateDestroy(WebsocketClient* client) {
	auto ws = reinterpret_cast<WSClient**>(client);
	delete* ws;
	ws = nullptr;
}