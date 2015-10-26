//
// detail/high_precision_timer_scheduler.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//

#ifndef UDT_TIMER_DETAIL_HIGH_PRECISION_TIMER_IPP
#define UDT_TIMER_DETAIL_HIGH_PRECISION_TIMER_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif  // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <chrono>

#include <boost/asio/detail/bind_handler.hpp>
#include <boost/asio/detail/config.hpp>
#include <boost/asio/detail/timer_scheduler.hpp>

#include <boost/log/trivial.hpp>

#include <boost/chrono.hpp>

#include "udt/timer/high_precision_timer_scheduler.hpp"

#include <boost/asio/detail/push_options.hpp>

namespace udt {
namespace timer {

high_precision_timer_scheduler::high_precision_timer_scheduler(
    boost::asio::io_service& io_service)
    : boost::asio::detail::service_base<high_precision_timer_scheduler>(
          io_service),
      io_service_(
          boost::asio::use_service<boost::asio::detail::io_service_impl>(
              io_service)),
      mutex_(),
      timer_queues_(),
      p_thread_(0),
      stop_thread_(false),
      shutdown_(false) {
  p_thread_ = new boost::asio::detail::thread(boost::asio::detail::bind_handler(
      &high_precision_timer_scheduler::call_run_thread, this));
}

high_precision_timer_scheduler::~high_precision_timer_scheduler() {
  shutdown_service();
}

void high_precision_timer_scheduler::shutdown_service() {
  using boost::asio::detail::operation;

  {
    boost::asio::detail::scoped_lock<std::mutex> lock_internals(mutex_);

    shutdown_ = true;
    stop_thread_ = true;
    cv_.notify_all();

    boost::asio::detail::op_queue<operation> ops;
    timer_queues_.get_all_timers(ops);
    io_service_.abandon_operations(ops);
  }

  if (p_thread_) {
    p_thread_->join();
    delete p_thread_;
    p_thread_ = nullptr;
  }
}

void high_precision_timer_scheduler::fork_service(
    boost::asio::io_service::fork_event fork_ev) {}

void high_precision_timer_scheduler::init_task() {}

template <typename Time_Traits>
void high_precision_timer_scheduler::add_timer_queue(
    timer_queue<Time_Traits>& queue) {
  do_add_timer_queue(queue);
}

template <typename Time_Traits>
void high_precision_timer_scheduler::remove_timer_queue(
    timer_queue<Time_Traits>& queue) {
  do_remove_timer_queue(queue);
}

template <typename Time_Traits>
void high_precision_timer_scheduler::schedule_timer(
    timer_queue<Time_Traits>& queue,
    const typename Time_Traits::time_type& time,
    typename timer_queue<Time_Traits>::per_timer_data& timer,
    boost::asio::detail::wait_op* op) {
  boost::asio::detail::scoped_lock<std::mutex> lock_internals(mutex_);

  if (shutdown_) {
    io_service_.post_immediate_completion(op, false);
    return;
  }

  queue.enqueue_timer(time, timer, op);
  io_service_.work_started();

  cv_.notify_all();
}

template <typename Time_Traits>
std::size_t high_precision_timer_scheduler::cancel_timer(
    timer_queue<Time_Traits>& queue,
    typename timer_queue<Time_Traits>::per_timer_data& timer,
    std::size_t max_cancelled) {
  boost::asio::detail::scoped_lock<std::mutex> lock_internals(mutex_);
  boost::asio::detail::op_queue<boost::asio::detail::operation> ops;
  std::size_t n = queue.cancel_timer(timer, ops, max_cancelled);
  io_service_.post_deferred_completions(ops);
  return n;
}

void high_precision_timer_scheduler::run_thread() {
  using Clock = std::chrono::high_resolution_clock;
  using boost::asio::detail::operation;

  std::unique_lock<std::mutex> lock(loop_mutex_);

  while (!stop_thread_) {
    // @todo : how to set condition variable timeout 
    const long kMaxWaitDuration = 15000;
    long wait_duration = 0;

    {
      boost::asio::detail::scoped_lock<std::mutex> lock_internals(mutex_);
      wait_duration = timer_queues_.wait_duration_usec(kMaxWaitDuration);
    }

    if (wait_duration == kMaxWaitDuration) {
      auto timeout = Clock::now() + std::chrono::microseconds(kMaxWaitDuration);
      cv_.wait_until(lock, timeout);
      continue;
    }

    active_wait_usec(wait_duration);

    boost::asio::detail::op_queue<operation> ops;

    {
      boost::asio::detail::scoped_lock<std::mutex> lock_internals(mutex_);
      timer_queues_.get_ready_timers(ops);
    }

    if (!ops.empty()) {
      io_service_.post_deferred_completions(ops);
    }
  }
}

void high_precision_timer_scheduler::call_run_thread(
    high_precision_timer_scheduler* scheduler) {
  scheduler->run_thread();
}

void high_precision_timer_scheduler::do_add_timer_queue(
    timer_queue_base& queue) {
  boost::asio::detail::scoped_lock<std::mutex> lock_internals(mutex_);
  timer_queues_.insert(&queue);
}

void high_precision_timer_scheduler::do_remove_timer_queue(
    timer_queue_base& queue) {
  boost::asio::detail::scoped_lock<std::mutex> lock_internals(mutex_);
  timer_queues_.erase(&queue);
}

void high_precision_timer_scheduler::active_wait_usec(long wait_duration) {
  using Clock = boost::chrono::high_resolution_clock;

  auto timeout = Clock::now() + boost::chrono::microseconds(wait_duration);

  while (Clock::now() < timeout) {
#ifdef BOOST_ASIO_WINDOWS
    std::this_thread::sleep_for(std::chrono::seconds(0));
#else
    // Bad performance on linux with std::this_thread::sleep_for.
    //std::this_thread::sleep_for(std::chrono::seconds(0));
    sleep(0);
#endif
  }
}

}  // timer
}  // scheduler

#include <boost/asio/detail/pop_options.hpp>

#endif  // UDT_TIMER_DETAIL_HIGH_PRECISION_TIMER_IPP
