#include "ws_client.h"

#include <iostream>
#include <iomanip>

#include "mongoose.h"

thread_local static std::chrono::steady_clock::time_point last_data_recv_time = std::chrono::steady_clock::now();

void WSClient::ev_handler_mg_(mg_connection *nc, int ev, void *ev_data)
{
    WSClient *client_ = (WSClient *)nc->user_data;
    if (client_ == nullptr)
        return;
    if (client_->nc_ != nc)
    {
        nc->flags |= MG_F_CLOSE_IMMEDIATELY;
        return;
    }
    switch (ev)
    {
    case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
    {
        struct http_message *hm = (struct http_message *)ev_data;
        if (nullptr == hm || hm->resp_code == 101)
        {
            client_->connectedFlag_ = true;
            last_data_recv_time = std::chrono::steady_clock::now();
            if (client_->on_ws_con_state_cb_ != nullptr)
            {
                std::thread([](on_ws_con_state_cb cb)
                            {
                                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                                cb(true);
                            },
                            client_->on_ws_con_state_cb_)
                    .detach();
            }
        }
        break;
    }
    case MG_EV_POLL:
    {
        break;
    }
    case MG_EV_WEBSOCKET_CONTROL_FRAME:
    {
        last_data_recv_time = std::chrono::steady_clock::now();
        break;
    }
    case MG_EV_WEBSOCKET_FRAME:
    {
        last_data_recv_time = std::chrono::steady_clock::now();
        websocket_message *wm = (websocket_message *)ev_data;
        std::string message = std::string((const char *)wm->data, (int)wm->size);
        {
            std::unique_lock<std::mutex> cvlk(client_->mut_);
            client_->recv_ = message;
            client_->cv_.notify_all();
        }
        std::lock_guard<std::mutex> lk(client_->dispatch_mtx_);
        if (client_->wsDatas_.size() > client_->backlog_)
            client_->wsDatas_.clear();
        client_->wsDatas_.push_back(message);
        break;
    }
    case MG_EV_CLOSE:
    {
        client_->connectedFlag_ = false;
        if (client_->on_ws_con_state_cb_ != nullptr)
        {
            std::thread([](on_ws_con_state_cb cb)
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(500));
                            cb(false);
                        },
                        client_->on_ws_con_state_cb_)
                .detach();
        }
        break;
    }
    }
}

WSClient::WSClient(std::string url, bool autoReconnect, size_t backlog)
    : url_(url)
    , autoReconnect_(autoReconnect)
    , backlog_(backlog)
    , exitFlag_(true)
    , connectedFlag_(false)
    , on_ws_data_cb_(nullptr)
    , data_cb_usrParam_(nullptr)
    , on_ws_con_state_cb_(nullptr)
    , con_state_usrParam_(nullptr)
{
}

bool WSClient::Connect()
{
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (connectedFlag_)
            return true;
        if (!exitFlag_)
            return false;
    }
    mg_mgr *mgr_ = (mg_mgr *)malloc(sizeof(mg_mgr));
    mg_mgr_init(mgr_, nullptr);
    exitFlag_ = false;
    mg_run_thd_ = std::thread(&WSClient::mgRun, this, mgr_);
    wsdata_dispatch_thd_ = std::thread(&WSClient::wsDataDispatch, this);
    return true;
}

void WSClient::mgRun(mg_mgr *mgr_)
{
    //auto last_send_ping = std::chrono::steady_clock::now();
    last_data_recv_time = std::chrono::steady_clock::now();
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 必须休眠，不然锁竞争不会让出当前线程
        std::lock_guard<std::mutex> lk(mutex_);
        if (exitFlag_)
            break;
        mg_mgr_poll(mgr_, 0);
        
        /*if (nc_ != nullptr && connectedFlag_ && std::chrono::steady_clock::now() - last_send_ping > std::chrono::seconds(5)) {
            last_send_ping = std::chrono::steady_clock::now();
            mg_send_websocket_frame(nc_, WEBSOCKET_OP_PING, "", 0);
        }*/

        if (connectedFlag_ && std::chrono::steady_clock::now() - last_data_recv_time > std::chrono::seconds(60)) {
            std::cout << "由于长时间未接收到数据超时，推断websocket已断开！" << std::endl;
            connectedFlag_ = false;
            if (nullptr != nc_) {
                nc_->flags |= MG_F_CLOSE_IMMEDIATELY;
            }
            nc_ = nullptr;
        }

        if (!connectedFlag_)
        {
            if (autoReconnect_)
            {
                mg_connection *$nc_ = mg_connect_ws(mgr_, WSClient::ev_handler_mg_, url_.c_str(), nullptr, nullptr);
                if (nullptr == $nc_)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                }
                else
                {
                    $nc_->user_data = this;
                    nc_ = $nc_;
                    int c = 0;
                    while (!connectedFlag_ && ++c < 10)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(300));
                        mg_mgr_poll(mgr_, 0);
                    }
                }
            }
            else
            {
                exitFlag_ = true;
            }
        }
    }
    mg_mgr_free(mgr_);
    free(mgr_);
}

void WSClient::wsDataDispatch()
{
    while (true)
    {
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::lock_guard<std::mutex> lk(mutex_);
            if (exitFlag_)
                break;
            if (on_ws_data_cb_ == nullptr)
                continue;
        }
        std::string wsData;
        {
            std::lock_guard<std::mutex> lk_dispatch(dispatch_mtx_);
            if (wsDatas_.empty())
                continue;
            wsData = std::move(wsDatas_.front());
            wsDatas_.erase(wsDatas_.begin());
        }
        if (!wsData.empty())
            on_ws_data_cb_(wsData);
    }
}

void WSClient::Close()
{
    if(std::this_thread::get_id() == mg_run_thd_.get_id() || std::this_thread::get_id() == wsdata_dispatch_thd_.get_id()) return;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (exitFlag_)
            return;
        exitFlag_ = true;
        on_ws_data_cb_ = nullptr;
        on_ws_con_state_cb_ = nullptr;
    }
    if (mg_run_thd_.joinable())
        mg_run_thd_.join();
    if (wsdata_dispatch_thd_.joinable())
        wsdata_dispatch_thd_.join();
    connectedFlag_ = false;
    if (nc_ != nullptr)
        nc_->user_data = nullptr;
    nc_ = nullptr;
}

WSClient::~WSClient()
{
    Close();
}

void WSClient::SetConStateCb(on_ws_con_state_cb disconnect_cb)
{
    std::lock_guard<std::mutex> lk(mutex_);
    on_ws_con_state_cb_ = disconnect_cb;
}

void WSClient::SetDataCb(on_ws_data_cb data_cb)
{
    std::lock_guard<std::mutex> lk(mutex_);
    on_ws_data_cb_ = data_cb;
}

void WSClient::Send(const std::string &data2send, bool isBinary)
{
    if (data2send.empty() || data2send.size() < 1 || data2send.size() >= 1024 * 1024 * 10)
    {
        return;
    }

    std::lock_guard<std::mutex> lk(mutex_);
    if (exitFlag_ || !connectedFlag_)
        return;
    mg_send_websocket_frame(nc_, isBinary ? WEBSOCKET_OP_BINARY : WEBSOCKET_OP_TEXT, data2send.data(), data2send.size());
}

void WSClient::Request(const std::string &req, std::string &resp, judge_request_success judge_fun, unsigned long timeout, bool isBinary)
{
    if (req.empty() || req.size() < 1 || req.size() >= 1024 * 1024 * 10)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (exitFlag_ || !connectedFlag_)
            return;
        mg_send_websocket_frame(nc_, isBinary ? WEBSOCKET_OP_BINARY : WEBSOCKET_OP_TEXT, req.data(), req.size());
    }

    recv_.clear();
    std::unique_lock<std::mutex> cvlk(mut_);

    if (!cv_.wait_for(cvlk, std::chrono::milliseconds(timeout),
                      [&]()
                      { return !recv_.empty() && judge_fun(recv_); }))
    {
        return;
    }

    resp = recv_;
    recv_.clear();
}