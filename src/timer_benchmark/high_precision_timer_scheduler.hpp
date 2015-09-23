//
// high_precision_timer_scheduler.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//


#ifndef HIGH_PRECISION_TIMER_SCHEDULER_HPP
#define HIGH_PRECISION_TIMER_SCHEDULER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>

#include <cstddef>
#include <mutex>
#include <condition_variable>

#include <boost/asio/detail/event.hpp>
#include <boost/asio/detail/limits.hpp>
#include <boost/asio/detail/mutex.hpp>
#include <boost/asio/detail/op_queue.hpp>
#include <boost/asio/detail/thread.hpp>
#include <boost/asio/detail/timer_queue_base.hpp>
#include <boost/asio/detail/timer_queue_set.hpp>
#include <boost/asio/detail/wait_op.hpp>
#include <boost/asio/io_service.hpp>

#if defined(BOOST_ASIO_HAS_IOCP)
# include <boost/asio/detail/thread.hpp>
#endif // defined(BOOST_ASIO_HAS_IOCP)

#include <boost/asio/detail/push_options.hpp>


class high_precision_timer_scheduler
  : public boost::asio::detail::service_base<high_precision_timer_scheduler> {

public:
  template <typename Time_trait>
  using timer_queue = boost::asio::detail::timer_queue<Time_trait>;

  using timer_queue_base = boost::asio::detail::timer_queue_base;

  using timer_queue_set = boost::asio::detail::timer_queue_set;

  // Constructor
  BOOST_ASIO_DECL high_precision_timer_scheduler(boost::asio::io_service& io_service);

  // Destructor
  BOOST_ASIO_DECL ~high_precision_timer_scheduler();

  // Destroy all user-defined handler objects owned by the service.
  BOOST_ASIO_DECL void shutdown_service();

  // Recreate internal descriptors following a fork.
  BOOST_ASIO_DECL void fork_service(boost::asio::io_service::fork_event fork_ev);

  // Initialise the task. No effect as this class uses its own thread.
  BOOST_ASIO_DECL void init_task();

  // Add a new timer queue to the reactor.
  template <typename Time_Traits>
  void add_timer_queue(timer_queue<Time_Traits>& queue);

  // Remove a timer queue from the reactor.
  template <typename Time_Traits>
  void remove_timer_queue(timer_queue<Time_Traits>& queue);

  // Schedule a new operation in the given timer queue to expire at the
  // specified absolute time.
  template <typename Time_Traits>
  void schedule_timer(timer_queue<Time_Traits>& queue,
    const typename Time_Traits::time_type& time,
    typename timer_queue<Time_Traits>::per_timer_data& timer, boost::asio::detail::wait_op* op);

  // Cancel the timer operations associated with the given token. Returns the
  // number of operations that have been posted or dispatched.
  template <typename Time_Traits>
  std::size_t cancel_timer(timer_queue<Time_Traits>& queue,
    typename timer_queue<Time_Traits>::per_timer_data& timer,
    std::size_t max_cancelled = (std::numeric_limits<std::size_t>::max)());


private:
  // Run the select loop in the thread.
  BOOST_ASIO_DECL void run_thread();

  // Entry point for the select loop thread.
  BOOST_ASIO_DECL static void call_run_thread(high_precision_timer_scheduler* reactor);

  // Helper function to add a new timer queue.
  BOOST_ASIO_DECL void do_add_timer_queue(timer_queue_base& queue);

  // Helper function to remove a timer queue.
  BOOST_ASIO_DECL void do_remove_timer_queue(timer_queue_base& queue);

  void active_wait_usec(long wait_duration);

  // The io_service implementation used to post completions.
  boost::asio::detail::io_service_impl& io_service_;

  // Mutex used to protect internal variables.
  std::mutex mutex_;

  // Condition variable used to notify_all to wake up background thread.
  std::condition_variable cv_;

  // The timer queues.
  timer_queue_set timer_queues_;

  // The background thread that is waiting for timers to expire.
  boost::asio::detail::thread* thread_;

  // Does the background thread need to stop.
  bool stop_thread_;

  // Whether the service has been shut down.
  bool shutdown_;
};


#include <boost/asio/detail/pop_options.hpp>

#include <timer_benchmark/detail/high_precision_timer_scheduler.ipp>

#endif  // HIGH_PRECISION_TIMER_SCHEDULER_HPP