#include <cstdio>
#include <iostream>
#include "common.h"
#include <gflags/gflags.h>
erpc::Rpc<erpc::CTransport> *rpc;
erpc::MsgBuffer req;
erpc::MsgBuffer resp;
DEFINE_uint32(client_port, 31020, "the number of request per processes\n");

void cont_func(void *, void *) { printf("%s\n", resp.buf_); }

void sm_handler(int, erpc::SmEventType, erpc::SmErrType, void *) {}

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::string client_uri = kClientHostname + ":" + std::to_string(FLAGS_client_port);
  erpc::Nexus nexus(client_uri);

  rpc = new erpc::Rpc<erpc::CTransport>(&nexus, nullptr, 0, sm_handler);

  std::string server_uri = kServerHostname + ":" + std::to_string(kUDPPort);
  int session_num = rpc->create_session(server_uri, 0);

  while (!rpc->is_connected(session_num)) rpc->run_event_loop_once();

  // 这里初始化了需要多少的包头，分配了size+header的空间
  req = rpc->alloc_msg_buffer_or_die(1024*1000);
  resp = rpc->alloc_msg_buffer_or_die(1024*1000);
  auto msg = std::string(1300,'h');
  sprintf(reinterpret_cast<char *>(req.buf_), "%s", msg.c_str());
  // std::cout << req.max_data_size_
  rpc->enqueue_request(session_num, kReqType, &req, &resp, cont_func, nullptr);
  rpc->run_event_loop(100000);

  delete rpc;
}
