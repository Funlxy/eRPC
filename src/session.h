#ifndef ERPC_SESSION_H
#define ERPC_SESSION_H

#include <limits>
#include <mutex>
#include <queue>

#include "cc/timely.h"
#include "cc/timing_wheel.h"
#include "common.h"
#include "msg_buffer.h"
#include "ops.h"
#include "sm_types.h"
#include "sslot.h"
#include "util/buffer.h"
#include "util/fixed_vector.h"

namespace erpc {

// Forward declarations for friendship. Prevent Nexus's background threads
// from accessing session members.
class IBTransport;
class RawTransport;

template <typename T>
class Rpc;

/// A one-to-one session class for all transports
class Session {
  friend class Rpc<IBTransport>;
  friend class Rpc<RawTransport>;

 public:
  enum class Role : int { kServer, kClient };

 private:
  Session(Role role, conn_req_uniq_token_t uniq_token, double freq_ghz)
      : role(role), uniq_token(uniq_token), freq_ghz(freq_ghz) {
    remote_routing_info =
        is_client() ? &server.routing_info : &client.routing_info;

    if (is_client()) client_info.cc.timely = Timely(freq_ghz);

    // Arrange the free slot vector so that slots are popped in order
    for (size_t i = 0; i < kSessionReqWindow; i++) {
      // Initialize session slot with index = sslot_i
      const size_t sslot_i = (kSessionReqWindow - 1 - i);
      SSlot &sslot = sslot_arr[sslot_i];

      // This buries all MsgBuffers
      memset(static_cast<void *>(&sslot), 0, sizeof(SSlot));

      sslot.prealloc_used = true;  // There's no user-allocated memory to free
      sslot.session = this;
      sslot.is_client = is_client();
      sslot.index = sslot_i;
      sslot.cur_req_num = sslot_i;  // 1st req num = (+kSessionReqWindow)

      if (is_client()) {
        for (auto &x : sslot.client_info.wslot_idx) x = kWheelInvalidWslot;
        sslot.client_info.cont_etid = kInvalidBgETid;  // Continuations in fg
      } else {
        sslot.server_info.req_type = kInvalidReqType;
      }

      client_info.sslot_free_vec.push_back(sslot_i);
    }
  }

  /// All session resources are freed by the owner Rpc
  ~Session() {}

  inline bool is_client() const { return role == Role::kClient; }
  inline bool is_server() const { return role == Role::kServer; }
  inline bool is_connected() const { return state == SessionState::kConnected; }

  /**
   * @brief Get the absolute timestamp for transmission, and update abs_tx_tsc
   *
   * @param ref_tsc A recently-sampled timestamp that acts as the base for the
   * absolute TX timestamp
   * @param pkt_size The size of the packet to transmit
   * @return The absolute TX timestamp
   */
  inline size_t cc_getupdate_tx_tsc(size_t ref_tsc, size_t pkt_size) {
    assert(is_client());
    assert(ref_tsc > 1000000000 && pkt_size <= 8192);  // Args sanity check

    double ns_delta = 1000000000 * (pkt_size / client_info.cc.timely.rate);

    size_t &abs_tx_tsc = client_info.cc.abs_tx_tsc;
    abs_tx_tsc += ns_to_cycles(ns_delta, freq_ghz);
    abs_tx_tsc = std::max(ref_tsc, abs_tx_tsc);  // Jump ahead if we're lagging
    return abs_tx_tsc;
  }

  /// Return true iff this session is uncongested
  inline bool is_uncongested() const {
    return client_info.cc.timely.rate == Timely::kMaxRate;
  }

  const Role role;  ///< The role (server/client) of this session endpoint
  const conn_req_uniq_token_t uniq_token;  ///< A cluster-wide unique token
  const double freq_ghz;                   ///< TSC frequency
  SessionState state;  ///< The management state of this session endpoint
  SessionEndpoint client, server;  ///< Read-only endpoint metadata

  std::array<SSlot, kSessionReqWindow> sslot_arr;  ///< The session slots

  ///@{ Info saved for faster unconditional access
  Transport::RoutingInfo *remote_routing_info;
  uint16_t local_session_num;
  uint16_t remote_session_num;
  ///@}

  /// Information that is required only at the client endpoint
  struct {
    size_t credits = kSessionCredits;  ///< Currently available credits

    /// Free session slots. We could use sslot pointers, but indices are useful
    /// in request number calculation.
    FixedVector<size_t, kSessionReqWindow> sslot_free_vec;

    /// Requests that spill over kSessionReqWindow are queued here
    std::queue<enq_req_args_t> enq_req_backlog;

    // Congestion control
    struct {
      Timely timely;
      size_t abs_tx_tsc;               ///< Last absolute TX timestamp
      size_t num_retransmissions = 0;  ///< Number of retransmissions
    } cc;

    size_t sm_req_ts;  ///< Timestamp of the last session management request
  } client_info;
};

}  // End erpc

#endif  // ERPC_SESSION_H
