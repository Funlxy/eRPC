#include "common.h"
#include <iostream>
#include <string>
erpc::Rpc<erpc::CTransport> *rpc;

void req_handler(erpc::ReqHandle *req_handle, void *) {
  auto req = req_handle->get_req_msgbuf();
  std::cout << req->get_data_size() << std::endl;
  // std::cout << req->buf_ << std::endl;
  auto s = std::string(reinterpret_cast<char*>(req->buf_),req->get_data_size());
  auto &resp = req_handle->pre_resp_msgbuf_;
  rpc->resize_msg_buffer(&resp, kMsgSize);
  sprintf(reinterpret_cast<char *>(resp.buf_), "hello");
  rpc->enqueue_response(req_handle, &resp);
}

int main() {
  std::string server_uri = kServerHostname + ":" + std::to_string(kUDPPort);
  erpc::Nexus nexus(server_uri);
  nexus.register_req_func(kReqType, req_handler);

  rpc = new erpc::Rpc<erpc::CTransport>(&nexus, nullptr, 0, nullptr);
  rpc->run_event_loop(100000);
}
