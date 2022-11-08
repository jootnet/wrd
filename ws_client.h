#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <atomic>
#include <vector>

using on_ws_con_state_cb = std::function<void(bool)>;

using on_ws_data_cb = std::function<void(const std::string &)>;

using judge_request_success = std::function<bool(const std::string &)>;

struct mg_connection;
struct mg_mgr;
/**
 * @brief websocket客户端类
 */
class WSClient
{
public:
   /**
    * @brief 构造函数
    * @param url 服务器地址，以ws://或wss://开头
    * @param backlog 最多积压的任务数（即从服务器收到的数据最多存储条目数，超过这个数量则丢弃新的数据）。这个任务数通过on_ws_data_cb回调的执行来消减。若未设置回调，则直接丢弃所有数据（Request方法执行过程中数据除外）
    */
   WSClient(std::string url, bool autoReconnect = true, size_t backlog = 4);
   /**
    * @brief 启动连接
    * @note 此函数不会真的尝试连接，连接在后台线程中，此处只是分配内存。一般不会返回false
    */
   bool Connect();
   /**
    * @brief 断开与服务器连接
    */
   void Close();
   ~WSClient();
   /**
    * @brief 为连接设置状态回调
    */
   void SetConStateCb(on_ws_con_state_cb);
   /**
    * @brief 为连接设置数据回调
    * @note 注意：这个回调收到的数据不仅仅包括服务端主动发送或通过Send发送的数据的异步响应，还包括Request函数的响应。因为实在无法进行区分。
    */
   void SetDataCb(on_ws_data_cb);
   /**
    * @brief 发送数据到服务区端，并不关心其返回（或稍后在on_ws_data_cb中处理）
    */
   void Send(const std::string &, bool isBinary = false);
   /**
    * @brief 当前是否与服务端网络畅通。这个值不一定有实际意义，因为没有对接收心跳进行处理
    *
    * mongoose现在是5秒会发一次PING
    *
    * 如果15秒未收到任何数据（包括PONG或者对方的PING）则会重连
    */
   inline bool IsConnected() const {
       return connectedFlag_;
   }

   /**
    * @brief 尝试将数据发送到服务器并且等待返回
    * @param isBinary 是否以OP_BINARY形式发送数据
    *
    * 这个方法可靠性较低，它将在发送完数据之后第一次收到数据即返回，并未做数据有效性的确认。即可能收到的数据是服务端的另一个业务流程数据
    */
   template <class _Rep, class _Period>
   void Request(const std::string& req, std::string& resp, judge_request_success judge_fun, const std::chrono::duration<_Rep, _Period>& timeout = std::chrono::seconds(3), bool isBinary = false) {
       Request(req, resp, judge_fun, std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count(), isBinary);
   }
private:
   std::string url_;
   bool autoReconnect_;
   size_t backlog_;
   bool exitFlag_;
   bool connectedFlag_;
   on_ws_data_cb on_ws_data_cb_;
   void *data_cb_usrParam_;
   on_ws_con_state_cb on_ws_con_state_cb_;
   void *con_state_usrParam_;
   std::mutex mutex_;
   static void ev_handler_mg_(mg_connection *nc, int ev, void *ev_data);
   mg_connection *nc_;

   void Request(const std::string&, std::string&, judge_request_success, unsigned long timeout = 3000, bool isBinary = false);

   std::thread mg_run_thd_;
   void mgRun(mg_mgr *);

   std::mutex dispatch_mtx_;
   std::vector<std::string> wsDatas_;
   std::thread wsdata_dispatch_thd_;
   void wsDataDispatch();

   std::string recv_;
   std::mutex mut_;
   std::condition_variable cv_;
};

#endif//WS_CLIENT_H