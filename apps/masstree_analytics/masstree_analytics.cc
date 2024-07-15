#include "masstree_analytics.h"
#include <signal.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <regex>
#include "util/autorun_helpers.h"
#include "protobuf/message.pb.h"
std::vector<std::vector<std::string>> work_load;
void app_cont_func(void *, void *);  // Forward declaration

static constexpr bool kAppVerbose = false;
int init_workload(std::string path){
  std::ifstream infile(path);
  if (!infile.is_open()) {
      std::cerr << "cant open file" << std::endl;
      return -1;
  }

  // 一次性读取整个文件内容
  std::stringstream buffer;
  buffer << infile.rdbuf();
  std::string content = buffer.str();
  infile.close();

  // 按行分割文件内容
  std::istringstream contentStream(content);
  std::string line;
  const std::string prefix = "READ usertable ";
  const std::string suffix = " [ field0 ]";
  int cnt = 0;
  std::vector<std::string> cur; 
  while (std::getline(contentStream, line)) {
      // 检查行是否以指定的前缀开始和以指定的后缀结束
      if (line.compare(0, prefix.length(), prefix) == 0 && 
          line.compare(line.length() - suffix.length(), suffix.length(), suffix) == 0) {
          // 提取中间的部分
          std::string user_field = line.substr(prefix.length(), 
                                                line.length() - prefix.length() - suffix.length());
          cur.push_back(user_field);
      }
  }
  int minSize = cur.size() / FLAGS_num_client_threads;
  int extra = cur.size() % FLAGS_num_client_threads;

  auto it = cur.begin();
  for (int i = 0; i < FLAGS_num_client_threads; ++i) {
      int currentSize = minSize + (i < extra ? 1 : 0);
      work_load[i].insert(work_load[i].end(), it, it + currentSize);
      it += currentSize;
  }
  printf("work load cnts: %zu",cur.size());
  return 0;
}
int load_workload(const std::string& path,std::vector<std::pair<std::string,std::string>>& data){
  std::cout << path << std::endl;
  std::ifstream infile(path);
  if (!infile.is_open()) {
      std::cerr << "cant open file" << std::endl;
      return -1;
  }

  // 一次性读取整个文件内容
  std::stringstream buffer;
  buffer << infile.rdbuf();
  std::string content = buffer.str();
  infile.close();

  // 按行分割文件内容
  std::istringstream contentStream(content);
  std::string line;
  const std::string prefix = "INSERT usertable ";
  std::regex pattern(R"((user\d+) \[ field0=(.+)\])");
  while (std::getline(contentStream, line)) {
      // 检查行是否以指定的前缀开始和以指定的后缀结束
      if (line.compare(0, prefix.length(), prefix) == 0) {
          // 提取中间的部分
        std::smatch match;
        if (std::regex_search(line, match, pattern) && match.size() > 2) {
            std::string key = match.str(1);
            std::string value = match.str(1);
            value.pop_back();
            data.push_back({key,value});
        }
      }
  }

  printf("work load cnts: %zu\n",data.size());
  return 0;
}
// Generate the key for this key index
// void key_gen(size_t index, uint8_t *key) {
//   static_assert(MtIndex::kKeySize >= 2 * sizeof(uint64_t), "");
//   auto *key_64 = reinterpret_cast<uint64_t *>(key);
//   key_64[0] = 10;
//   key_64[1] = index * 8192;
// }

/// Return the pre-known quantity stored in each 32-bit chunk of the value for
/// the key for this seed
uint32_t get_value32_for_seed(uint32_t seed) { return seed + 1; }

void point_req_handler(erpc::ReqHandle *req_handle, void *_context) {
  auto *c = static_cast<AppContext *>(_context);

  // Handler for point requests runs in a foreground thread
  const size_t etid = c->rpc_->get_etid();
  assert(etid >= FLAGS_num_server_bg_threads &&
         etid < FLAGS_num_server_bg_threads + FLAGS_num_server_fg_threads);

  if (kBypassMasstree) {
    // Send a garbage response
    c->rpc_->enqueue_response(req_handle, &req_handle->pre_resp_msgbuf_);
    return;
  }

  MtIndex *mti = c->server.mt_index; // 得到index
  threadinfo_t *ti = c->server.ti_arr[etid];
  assert(mti != nullptr && ti != nullptr);

  const auto *req_msgbuf = req_handle->get_req_msgbuf();

  // deserialize
  masstree::Req req;
  req.ParseFromArray(req_msgbuf->buf_, req_msgbuf->get_data_size());
  assert(req.id()==1);
  // assert(req_msgbuf->get_data_size() == sizeof(wire_req_t));

  // // 得到req.
  // auto *req = reinterpret_cast<const wire_req_t *>(req_msgbuf->buf_);
  // assert(req->req_type == kAppPointReqType);


  // uint8_t key_copy[MtIndex::kKeySize];  // mti->get() modifies key
  // memcpy(key_copy, req->point_req.key, MtIndex::kKeySize); // 这里memcpy了？

  // auto *resp =
  //     reinterpret_cast<wire_resp_t *>(req_handle->pre_resp_msgbuf_.buf_);
  masstree::Resp resp;
  std::string value;
  const bool success = mti->get(req.key(), value, ti);
  if(!success){
    printf("error,not found%s\n",req.key().c_str());
  }
  resp.set_id(1);
  resp.set_value(value);
  // resp->resp_type = success ? RespType::kFound : RespType::kNotFound;
  erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&req_handle->pre_resp_msgbuf_,
                                                 resp.ByteSizeLong());
  // serialize
  resp.SerializeToArray(req_handle->pre_resp_msgbuf_.buf_, resp.ByteSizeLong());
  if (kAppVerbose) {
    printf(
        "main: Handled point request in eRPC thread %zu. Key %s, found %s, "
        "value %s\n",
        etid, req.key().c_str(), success ? "yes" : "no",
        success ? resp.value().c_str() : "N/A");
  }

  c->rpc_->enqueue_response(req_handle, &req_handle->pre_resp_msgbuf_);
}

// void range_req_handler(erpc::ReqHandle *req_handle, void *_context) {
//   auto *c = static_cast<AppContext *>(_context);

//   // Range request handler runs in a background thread
//   const size_t etid = c->rpc_->get_etid();
//   assert(etid < FLAGS_num_server_bg_threads);

//   if (kAppVerbose) {
//     printf("main: Handling range request in eRPC thread %zu.\n", etid);
//   }

//   MtIndex *mti = c->server.mt_index;
//   threadinfo_t *ti = c->server.ti_arr[etid];
//   assert(mti != nullptr && ti != nullptr);

//   const auto *req_msgbuf = req_handle->get_req_msgbuf();
//   assert(req_msgbuf->get_data_size() == sizeof(wire_req_t));

//   auto *req = reinterpret_cast<const wire_req_t *>(req_msgbuf->buf_);
//   assert(req->req_type == kAppRangeReqType);
//   uint8_t key_copy[MtIndex::kKeySize];  // mti->sum_in_range() modifies key
//   memcpy(key_copy, req->point_req.key, MtIndex::kKeySize);

//   const size_t count = mti->sum_in_range(key_copy, req->range_req.range, ti);

//   erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&req_handle->pre_resp_msgbuf_,
//                                                  sizeof(wire_resp_t));
//   auto *resp =
//       reinterpret_cast<wire_resp_t *>(req_handle->pre_resp_msgbuf_.buf_);
//   resp->resp_type = RespType::kFound;
//   resp->range_count = count;

//   c->rpc_->enqueue_response(req_handle, &req_handle->pre_resp_msgbuf_);
// }

// Send one request using this MsgBuffer
// with protobuf
void send_req(AppContext *c, size_t msgbuf_idx) {
  erpc::MsgBuffer &req_msgbuf = c->client.window_[msgbuf_idx].req_msgbuf_;
  auto cur_work = work_load[c->thread_id_];
  // Protobuf req
  masstree::Req req;
  req.set_id(1); // always use 1
  req.set_key(cur_work[c->client.num_send_tot]); // get key
  int len = req.ByteSizeLong();
  if(len!=req_msgbuf.get_data_size()){
     erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&req_msgbuf,len);
  }
  c->client.window_[msgbuf_idx].req_ts_ = erpc::rdtsc(); // 这里开始记录延迟
  // measuer for serialize
  req.SerializeToArray(req_msgbuf.buf_, len);
  if (kAppVerbose) {
    printf("main: Enqueuing request with msgbuf_idx %zu.\n", msgbuf_idx);
    sleep(1);
  }
  c->client.num_send_tot++;
  c->client.num_send_tot%=cur_work.size();
  // always get no scan
  c->rpc_->enqueue_request(0, kAppPointReqType, &req_msgbuf,
                           &c->client.window_[msgbuf_idx].resp_msgbuf_,
                           app_cont_func, reinterpret_cast<void *>(msgbuf_idx));
}


// Call back
void app_cont_func(void *_context, void *_msgbuf_idx) {
  auto *c = static_cast<AppContext *>(_context);
  const auto msgbuf_idx = reinterpret_cast<size_t>(_msgbuf_idx);
  if (kAppVerbose) {
    printf("main: Received response for msgbuf %zu.\n", msgbuf_idx);
  }

  const auto &resp_msgbuf = c->client.window_[msgbuf_idx].resp_msgbuf_;
  
  // deserialize
  masstree::Resp resp;
  resp.ParseFromArray(resp_msgbuf.buf_, resp_msgbuf.get_data_size());
  erpc::rt_assert(resp.value().size() == 64,
                  "Invalid response size");

  // latency
  const double usec =
      erpc::to_usec(erpc::rdtsc() - c->client.window_[msgbuf_idx].req_ts_,
                    c->rpc_->get_freq_ghz());
  assert(usec >= 0);

  // const auto *req = reinterpret_cast<wire_req_t *>(
  //     c->client.window_[msgbuf_idx].req_msgbuf_.buf_);
  // assert(req->req_type == kAppPointReqType ||
  //        req->req_type == kAppRangeReqType);

  // if (req->req_type == kAppPointReqType) {
  c->client.point_latency.update(static_cast<size_t>(usec * 10.0));  // < 1us

  //   // Check the value
  //   {
  //     const auto *wire_resp = reinterpret_cast<wire_resp_t *>(resp_msgbuf.buf_);
  //     const uint32_t recvd_value =
  //         *reinterpret_cast<const uint32_t *>(wire_resp->value);
  //     const uint32_t req_seed = c->client.window_[msgbuf_idx].req_seed_;
  //     if (recvd_value != get_value32_for_seed(req_seed)) {
  //       fprintf(stderr,
  //               "main: Value mismatch. Req seed = %u, recvd_value (first four "
  //               "bytes = %u)\n",
  //               req_seed, recvd_value);
  //     }
  //   }
  // } else {
  //   c->client.range_latency.update(static_cast<size_t>(usec));
  // }

  c->client.num_resps_tot++;
  send_req(c, msgbuf_idx);
}

void client_print_stats(AppContext &c) {
  const double seconds = c.client.tput_timer.get_us() / 1e6;
  const double tput_mrps = c.client.num_resps_tot / (seconds * 1000000);
  app_stats_t &stats = c.client.app_stats[c.thread_id_];
  stats.mrps = tput_mrps;
  stats.lat_us_50 = c.client.point_latency.perc(0.50) / 10.0;
  stats.lat_us_90 = c.client.point_latency.perc(0.90) / 10.0;
  stats.lat_us_99 = c.client.point_latency.perc(0.99) / 10.0;

  printf(
      "Client %zu. Tput = %.3f Mrps. "
      "Point-query latency (us) = {%.1f 50th, %.1f 90th, %.1f 99th}. "
      "Range-query latency (us) = {%zu 99th}.\n",
      c.thread_id_, tput_mrps, stats.lat_us_50, stats.lat_us_90,
      stats.lat_us_99, c.client.range_latency.perc(.99));

  if (c.thread_id_ == 0) {
    app_stats_t accum;
    for (size_t i = 0; i < FLAGS_num_client_threads; i++) {
      accum += c.client.app_stats[i];
    }
    accum.lat_us_50 /= FLAGS_num_client_threads;
    accum.lat_us_90 /= FLAGS_num_client_threads;
    accum.lat_us_99 /= FLAGS_num_client_threads;
    c.tmp_stat_->write(accum.to_string());
  }

  c.client.num_resps_tot = 0;
  c.client.point_latency.reset();
  c.client.range_latency.reset();

  c.client.tput_timer.reset();
}

// 每个创建一个新的RPC
void client_thread_func(size_t thread_id, app_stats_t *app_stats,
                        erpc::Nexus *nexus) {
  AppContext c;
  c.thread_id_ = thread_id;
  c.client.app_stats = app_stats;

  if (thread_id == 0) {
    c.tmp_stat_ = new TmpStat(app_stats_t::get_template_str());
  }

  std::vector<size_t> port_vec = flags_get_numa_ports(FLAGS_numa_node);
  erpc::rt_assert(port_vec.size() > 0);
  const uint8_t phy_port = port_vec.at(thread_id % port_vec.size());

  erpc::Rpc<erpc::CTransport> rpc(nexus, static_cast<void *>(&c),
                                  static_cast<uint8_t>(thread_id),
                                  basic_sm_handler, phy_port);
  rpc.retry_connect_on_invalid_rpc_id_ = true;
  c.rpc_ = &rpc;

  // Each client creates a session to only one server thread
  const size_t client_gid =
      (FLAGS_process_id * FLAGS_num_client_threads) + thread_id;
  const size_t server_tid =
      client_gid % FLAGS_num_server_fg_threads;  // eRPC TID

  c.session_num_vec_.resize(1);
  c.session_num_vec_[0] =
      rpc.create_session(erpc::get_uri_for_process(0), server_tid);
  assert(c.session_num_vec_[0] >= 0);

  while (c.num_sm_resps_ != 1) {
    rpc.run_event_loop(200);  // 200 milliseconds
    if (ctrl_c_pressed == 1) return;
  }
  assert(c.rpc_->is_connected(c.session_num_vec_[0]));
  fprintf(stderr, "main: Thread %zu: Connected. Sending requests.\n",
          thread_id);


  // alloc buffer
  alloc_req_resp_msg_buffers(&c);
  // 从这里开始记录throughput
  c.client.tput_timer.reset();
  for (size_t i = 0; i < FLAGS_req_window; i++) send_req(&c, i);

  for (size_t i = 0; i < FLAGS_test_ms; i += kAppEvLoopMs) {
    c.rpc_->run_event_loop(kAppEvLoopMs);
    if (ctrl_c_pressed == 1) break;
    client_print_stats(c);
  }
}

void server_thread_func(size_t thread_id, erpc::Nexus *nexus, MtIndex *mti,
                        threadinfo_t **ti_arr) {
  AppContext c;
  c.thread_id_ = thread_id;
  c.server.mt_index = mti;
  c.server.ti_arr = ti_arr;

  std::vector<size_t> port_vec = flags_get_numa_ports(FLAGS_numa_node);
  erpc::rt_assert(port_vec.size() > 0);
  uint8_t phy_port = port_vec.at(thread_id % port_vec.size());

  erpc::Rpc<erpc::CTransport> rpc(nexus, static_cast<void *>(&c),
                                  static_cast<uint8_t>(thread_id),
                                  basic_sm_handler, phy_port);
  c.rpc_ = &rpc;
  while (ctrl_c_pressed == 0) rpc.run_event_loop(200);
}

/**
 * @brief Populate Masstree in parallel from multiple threads
 *
 * @param thread_id Index of this thread among the threads doing population
 * @param mti The Masstree index
 * @param ti The Masstree threadinfo for this thread
 * @param shuffled_key_indices Indexes of the keys to be inserted, in order
 * @param num_cores Number of threads doing population
 */
void masstree_populate_func(size_t thread_id, MtIndex *mti, threadinfo_t *ti,
                            const std::vector<std::pair<std::string,std::string>> *workload,
                            size_t num_cores) {
  const size_t num_keys_to_insert_this_thread = workload->size() / num_cores;
  size_t num_keys_inserted_this_thread = 0;

  for (size_t i = 0; i < workload->size(); i++) {
    if (i % num_cores != thread_id) continue;  // Not this thread's job
    // generate key
    // const uint32_t key_index = shuffled_key_indices->at(i);
    // get key
    std::string key = workload->at(i).first;
    std::string value = workload->at(i).second;
    // uint8_t key[MtIndex::kKeySize];
    // uint8_t value[MtIndex::kValueSize];
    // key_gen(key_index, key);
    // auto *value_32 = reinterpret_cast<uint32_t *>(value);
    // for (size_t j = 0; j < MtIndex::kValueSize / sizeof(uint32_t); j++) {
    //   value_32[j] = get_value32_for_seed(key_index);
    // }
    // --------------------------------------------------
    if (kAppVerbose) {
      fprintf(stderr, "PUT: Key: [%s]\n",key.c_str());
      fprintf(stderr, "PUT: Value: [%s]\n",value.c_str());

      // const uint64_t *key_64 = reinterpret_cast<uint64_t *>(key);
      // for (size_t j = 0; j < MtIndex::kKeySize / sizeof(uint64_t); j++) {
      //   fprintf(stderr, "%zu ", key_64[j]);
      // }
      // fprintf(stderr, "] Value: [");
      // for (size_t j = 0; j < MtIndex::kValueSize; j++) { //68
      //   fprintf(stderr, "%u ", value[j]);
      // }
      // fprintf(stderr, "]\n");
    }

    mti->put(key,value,ti);
    num_keys_inserted_this_thread++;

    // Progress bar
    {
      if (thread_id == 0) {
        const size_t by_20 = num_keys_to_insert_this_thread / 20;
        if (by_20 > 0 && num_keys_inserted_this_thread % by_20 == 0) {
          const double progress_percent = 100.0 *
                                          num_keys_inserted_this_thread /
                                          num_keys_to_insert_this_thread;
          printf("Percent done = %.1f\n", progress_percent);
        }
      }
    }

    if (ctrl_c_pressed == 1) break;
  }
}

int main(int argc, char **argv) {
  signal(SIGINT, ctrl_c_handler); // ctrl_c_handler取消
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  erpc::rt_assert(FLAGS_req_window <= kAppMaxReqWindow, "Invalid req window");
  // range scan的百分比
  erpc::rt_assert(FLAGS_range_req_percent <= 100, "Invalid range req percent");

  if (FLAGS_num_server_bg_threads == 0) {
    printf(
        "main: Warning: No background threads. "
        "Range queries will run in foreground.\n");
  }


  // 服务端
  if (is_server()) {
    erpc::rt_assert(FLAGS_process_id == 0, "Invalid server process ID");

    // Create the Masstree using the main thread and insert keys
    threadinfo_t *ti = threadinfo::make(threadinfo::TI_MAIN, -1);
    MtIndex mti;
    mti.setup(ti);

    // Create a thread_info for every core, we'll use only some of them for
    // the actual benchmark
    const size_t num_cores = static_cast<size_t>(sysconf(_SC_NPROCESSORS_ONLN));
    auto ti_arr = new threadinfo_t *[num_cores];
    for (size_t i = 0; i < num_cores; i++) {
      ti_arr[i] = threadinfo::make(threadinfo::TI_PROCESS, i);
    }

    // Populate the tree in parallel to reduce initialization time
    {
      printf("main: Populating masstree with %zu keys from %zu cores\n",
             FLAGS_num_keys, FLAGS_num_population_threads);

      // 这里读取.
      std::vector<std::pair<std::string,std::string>> server_workload;
      load_workload("./ycsb_load.txt", server_workload);
      // std::vector<size_t> shuffled_key_indices;
      // shuffled_key_indices.reserve(FLAGS_num_keys);

      // // Populate and shuffle the order in which keys will be inserted
      // {
      //   for (size_t i = 0; i < FLAGS_num_keys; i++) {
      //     shuffled_key_indices.push_back(i);
      //   }
      //   auto rng = std::default_random_engine{};
      //   std::shuffle(std::begin(shuffled_key_indices),
      //                std::end(shuffled_key_indices), rng);
      // }

      printf("main: Launching threads to populate Masstree\n");
      std::vector<std::thread> populate_thread_arr(
          FLAGS_num_population_threads);
      for (size_t i = 0; i < FLAGS_num_population_threads; i++) {
        populate_thread_arr[i] =
            std::thread(masstree_populate_func, i, &mti, ti_arr[i],
                        &server_workload, FLAGS_num_population_threads);
      }
      for (size_t i = 0; i < FLAGS_num_population_threads; i++)
        populate_thread_arr[i].join();
    }

    // eRPC stuff
    erpc::Nexus nexus(erpc::get_uri_for_process(FLAGS_process_id),
                      FLAGS_numa_node, FLAGS_num_server_bg_threads);

    nexus.register_req_func(kAppPointReqType, point_req_handler,
                            erpc::ReqFuncType::kForeground);

    // auto range_handler_type = FLAGS_num_server_bg_threads > 0
    //                               ? erpc::ReqFuncType::kBackground
    //                               : erpc::ReqFuncType::kForeground;
    // nexus.register_req_func(kAppRangeReqType, range_req_handler,
    //                         range_handler_type);

    std::vector<std::thread> thread_arr(FLAGS_num_server_fg_threads);
    for (size_t i = 0; i < FLAGS_num_server_fg_threads; i++) {
      thread_arr[i] = std::thread(server_thread_func, i, &nexus, &mti,
                                  static_cast<threadinfo_t **>(ti_arr));
      erpc::bind_to_core(thread_arr[i], FLAGS_numa_node, i);
    }

    for (auto &thread : thread_arr) thread.join();
    delete[] ti_arr;

  } 
  else { // 客户端
    work_load.resize(FLAGS_num_client_threads);
    int ret = init_workload("ycsb_run.txt");
    if(ret==-1){
      printf("error!\n");
      exit(-1);
    }
    erpc::rt_assert(FLAGS_process_id > 0, "Invalid process ID");
    erpc::Nexus nexus(erpc::get_uri_for_process(FLAGS_process_id),
                      FLAGS_numa_node, FLAGS_num_server_bg_threads);
    // clinet 线程数
    std::vector<std::thread> thread_arr(FLAGS_num_client_threads);
    auto *app_stats = new app_stats_t[FLAGS_num_client_threads];
    // 线程启动
    for (size_t i = 0; i < FLAGS_num_client_threads; i++) {
      thread_arr[i] = std::thread(client_thread_func, i, app_stats, &nexus);
      erpc::bind_to_core(thread_arr[i], FLAGS_numa_node, i);
    }

    for (auto &thread : thread_arr) thread.join();
  }
}
