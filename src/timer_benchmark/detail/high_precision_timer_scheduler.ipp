//
// detail/high_precision_timer_scheduler.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//

#ifndef DETAIL_HIGH_PRECISION_TIMER_IPP
#define DETAIL_HIGH_PRECISION_TIMER_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <chrono>

#include <boost/asio/detail/bind_handler.hpp>
#include <boost/asio/detail/config.hpp>
#include <boost/asio/detail/timer_scheduler.hpp>

#include <boost/chrono.hpp>

#include <timer_benchmark/high_precision_timer_scheduler.hpp>

#include <boost/asio/detail/push_options.hpp>

high_precision_timer_scheduler::high_precision_timer_scheduler(boost::asio::io_service& io_service)
  : boost::asio::detail::service_base<high_precision_timer_scheduler>(io_service),
    io_service_(boost::asio::use_service<boost::asio::detail::io_service_impl>(io_service)),
    mutex_(),
    timer_queues_(),
    thread_(0),
    stop_thread_(false),
    shutdown_(false)
{
  thread_ = new boost::asio::detail::thread(
    boost::asio::detail::bind_handler(&high_precision_timer_scheduler::call_run_thread, this));
}


high_precision_timer_scheduler::~high_precision_timer_scheduler() {
  shutdown_service();
}

void high_precision_timer_scheduler::shutdown_service() {
  using boost::asio::detail::operation;
  std::unique_lock<std::mutex> lock(mutex_);

  shutdown_ = true;
  stop_thread_ = true;
  cv_.notify_all();
  lock.unlock();

  if (thread_) {
    thread_->join();
    delete thread_;
    thread_ = 0;
  }

  boost::asio::detail::op_queue<operation> ops;
  timer_queues_.get_all_timers(ops);
  io_service_.abandon_operations(ops);
}

void high_precision_timer_scheduler::fork_service(boost::asio::io_service::fork_event fork_ev) {

}

void high_precision_timer_scheduler::init_task() {

}

template <typename Time_Traits>
void high_precision_timer_scheduler::add_timer_queue(timer_queue<Time_Traits>& queue) {
  do_add_timer_queue(queue);
}

template <typename Time_Traits>
void high_precision_timer_scheduler::remove_timer_queue(timer_queue<Time_Traits>& queue) {
  do_remove_timer_queue(queue);
}

template <typename Time_Traits>
void high_precision_timer_scheduler::schedule_timer(timer_queue<Time_Traits>& queue,
                    const typename Time_Traits::time_type& time,
                    typename timer_queue<Time_Traits>::per_timer_data& timer,
                    boost::asio::detail::wait_op* op) {
  boost::asio::detail::scoped_lock<std::mutex> lock(mutex_);

  if (shutdown_) {
    io_service_.post_immediate_completion(op, false);
    return;
  }

  bool earliest = queue.enqueue_timer(time, timer, op);
  io_service_.work_started();

  if (earliest) {
    cv_.notify_all();
  }
}

template <typename Time_Traits>
std::size_t high_precision_timer_scheduler::cancel_timer(timer_queue<Time_Traits>& queue,
                      typename timer_queue<Time_Traits>::per_timer_data& timer,
                      std::size_t max_cancelled) {
  boost::asio::detail::scoped_lock<std::mutex> lock(mutex_);
  boost::asio::detail::op_queue<boost::asio::detail::operation> ops;
  std::size_t n = queue.cancel_timer(timer, ops, max_cancelled);
  lock.unlock();
  io_service_.post_deferred_completions(ops);
  return n;
}

void high_precision_timer_scheduler::run_thread() {
  using Clock = std::chrono::high_resolution_clock;
  using boost::asio::detail::operation;

  std::unique_lock<std::mutex> lock(mutex_);

  while (!stop_thread_) {
    const long max_wait_duration = 5 * 60 * 1000000;
    long wait_duration = timer_queues_.wait_duration_usec(max_wait_duration);

    if (wait_duration == max_wait_duration) {
      auto timeout = Clock::now() + std::chrono::microseconds(wait_duration);
      cv_.wait_until(lock, timeout);
      continue;
    }

    active_wait_usec(wait_duration);

    boost::asio::detail::op_queue<operation> ops;
    timer_queues_.get_ready_timers(ops);
    if (!ops.empty()) {
      lock.unlock();
      io_service_.post_deferred_completions(ops);
      lock.lock();
    }
  }
}


void high_precision_timer_scheduler::call_run_thread(high_precision_timer_scheduler* scheduler) {
  scheduler->run_thread();
}

void high_precision_timer_scheduler::do_add_timer_queue(timer_queue_base& queue) {
  std::unique_lock<std::mutex> lock(mutex_);
  timer_queues_.insert(&queue);
}


void high_precision_timer_scheduler::do_remove_timer_queue(timer_queue_base& queue) {
  std::unique_lock<std::mutex> lock(mutex_);
  timer_queues_.erase(&queue);
}


void high_precision_timer_scheduler::active_wait_usec(long wait_duration) {
  using Clock = boost::chrono::high_resolution_clock;

  auto timeout = Clock::now() + boost::chrono::microseconds(wait_duration);

  while (Clock::now() < timeout) {
    #ifdef BOOST_ASIO_WINDOWS
      std::this_thread::sleep_for(std::chrono::seconds(0));
    #else
      //  Bad performance on linux with std::this_thread::sleep_for.
      sleep(0);
    #endif
  }

}

#include <boost/asio/detail/pop_options.hpp>

#endif  // DETAIL_HIGH_PRECISION_TIMER_IPP
