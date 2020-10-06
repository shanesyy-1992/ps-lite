/*!
 *  Copyright (c) 2015 by Contributors
 * @file   ps.h
 * \brief  The parameter server interface
 */
#ifndef PS_PS_H_
#define PS_PS_H_
/** \brief basic setups in ps */
#include "ps/base.h"
/** \brief communicating with a pair of (int, string). */
#include "ps/simple_app.h"
/** \brief communcating with a list of key-value paris. */
#include "ps/kv_app.h"
namespace ps {
/** \brief Returns the number of worker nodes */
inline int NumWorkers() { return Postoffice::Get()->num_workers(); }
/** \brief Returns the number of server nodes */
inline int NumServers() { return Postoffice::Get()->num_servers(); }
/** \brief Returns true if this node is a worker node */
inline bool IsWorker() { return Postoffice::Get()->is_worker(); }
/** \brief Returns true if this node is a server node. */
inline bool IsServer() { return Postoffice::Get()->is_server(); }
/** \brief Returns true if this node is a scheduler node. */
inline bool IsScheduler() { return Postoffice::Get()->is_scheduler(); }
/** \brief Returns the rank of this node in its group
 *
 * Each worker will have a unique rank within [0, NumWorkers()). So are
 * servers. This function is available only after \ref Start has been called.
 */
inline int MyRank() { return Postoffice::Get()->my_rank(); }
/**
 * \brief start the system
 *
 * This function will block until every nodes are started.
 * \param argv0 the program name, used for logging
 */
// inline void Start(int customer_id, const char *argv0 = nullptr) {
//   Postoffice::Get()->Start(customer_id, argv0, true);
// }

inline void StartPS(int customer_id, Node::Role role, const char *argv0 = nullptr) {
  if (role == Node::WORKER) {
    Postoffice::GetWorker()->Start(customer_id, argv0, true, role);
  } else {
    Postoffice::GetServer()->Start(customer_id, argv0, true, role);
  }
}

inline void StartJointPS(int customer_id, const char *argv0 = nullptr, bool include_scheduler = false) {
  const char* val = CHECK_NOTNULL(Environment::Get()->find("DMLC_ROLE"));
  std::string role(val);
  bool is_scheduler = role == "scheduler";

  if (is_scheduler) {
    StartPS(customer_id, Node::SCHEDULER);
  } else {
    std::thread thread_sch;
    if (include_scheduler) {
      thread_sch = std::move(std::thread(StartPS, customer_id, Node::SCHEDULER, nullptr));
      LOG(INFO) << "Postoffice scheduler started.";
    }

    std::thread thread_s(StartPS, customer_id, Node::SERVER, nullptr);
    LOG(INFO) << "Postoffice server started.";

    std::thread thread_w(StartPS, customer_id, Node::WORKER, nullptr);
    LOG(INFO) << "Postoffice worker started.";

    thread_s.join();
    thread_w.join();

    if (include_scheduler) {
      thread_sch.join();
    }
  }
}

/**
 * \brief start the system
 *
 * This function will NOT block.
 * \param argv0 the program name, used for logging
 */
inline void StartAsync(int customer_id, Node::Role role, const char *argv0 = nullptr) {
  if (role == Node::WORKER) {
    Postoffice::GetWorker()->Start(customer_id, argv0, false, role);
  } else {
    Postoffice::GetServer()->Start(customer_id, argv0, false, role);
  }
}
/**
 * \brief terminate the system
 *
 * All nodes should call this function before existing.
 * \param do_barrier whether to block until every node is finalized, default true.
 */
inline void Finalize(int customer_id, const bool do_barrier = true) {
  Postoffice::Get()->Finalize(customer_id, do_barrier);
}
/**
 * \brief Register a callback to the system which is called after Finalize()
 *
 * The following codes are equal
 * \code {cpp}
 * RegisterExitCallback(cb);
 * Finalize();
 * \endcode
 *
 * \code {cpp}
 * Finalize();
 * cb();
 * \endcode
 * \param cb the callback function
 */
inline void RegisterExitCallback(const std::function<void()> &cb) {
  Postoffice::Get()->RegisterExitCallback(cb);
}

}  // namespace ps
#endif  // PS_PS_H_
