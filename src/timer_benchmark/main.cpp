#include <future>
#include <iostream>
#include <vector>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/chrono.hpp>
#include <boost/thread.hpp>

#include "udt/timer/basic_waitable_timer.hpp"
#include "udt/timer/high_precision_timer_scheduler.hpp"

using Accumulator = boost::accumulators::accumulator_set<
    int, boost::accumulators::features<
             boost::accumulators::tag::min, boost::accumulators::tag::max,
             boost::accumulators::tag::mean, boost::accumulators::tag::median>>;

void DisplayStatistics(const Accumulator& statistics, int expected_wait);

int main(int argc, char* argv[]) {
  namespace chrono = boost::chrono;
  using Clock = chrono::high_resolution_clock;
  using ClockTimer = udt::timer::basic_waitable_timer<
      Clock, boost::asio::wait_traits<Clock>,
      udt::timer::high_precision_timer_scheduler>;
  //using ClockTimer = boost::asio::basic_waitable_timer<Clock>;
  using TimerPtr = std::shared_ptr<ClockTimer>;
  using ClockTimePoint = chrono::time_point<Clock>;
  using TimerHandler =
      std::function<void(uint32_t, const boost::system::error_code&)>;

  if (argc < 3) {
    std::cout << "timer_benchmark [wait_usec] [sample_size]" << std::endl;
    return 0;
  }

  boost::asio::io_service io_service;
  std::unique_ptr<boost::asio::io_service::work> p_worker(
      new boost::asio::io_service::work(io_service));
  std::atomic<int> timeout_count(0);
  bool wait_set = false;
  std::promise<bool> wait_end;
  Accumulator statistics;
  boost::recursive_mutex count_mutex;
  boost::recursive_mutex stat_mutex;
  TimerHandler wait_handler;

  int wait_usec = atoi(argv[1]);
  int sample_size = atoi(argv[2]);

  if (wait_usec < 1) wait_usec = 50;
  if (sample_size < 1) sample_size = 10000;

  std::vector<TimerPtr> p_timers;
  std::vector<ClockTimePoint> time_points(sample_size);

  for (int i = 0; i < sample_size; ++i) {
    p_timers.push_back(std::make_shared<ClockTimer>(io_service));
  }

  wait_handler =
      [&](uint32_t timer_index, const boost::system::error_code& ec) {
        ClockTimePoint now = Clock::now();

        boost::recursive_mutex::scoped_lock lock(count_mutex);
        ++timeout_count;

        statistics(static_cast<int>(chrono::duration_cast<chrono::microseconds>(
                                        now - time_points[timer_index])
                                        .count()));
        if (!wait_set && (ec || timeout_count == sample_size)) {
          wait_set = true;
          wait_end.set_value(ec.value() == 0);
        }
      };

  boost::thread_group threads;
  for (uint16_t i = 1; i <= boost::thread::hardware_concurrency(); ++i) {
    threads.create_thread([&io_service]() { io_service.run(); });
  }

  uint32_t i = 0;
  for (auto& p_timer : p_timers) {
    time_points[i] = Clock::now();
    p_timer->expires_from_now(chrono::microseconds(wait_usec));
    p_timer->async_wait(boost::bind(wait_handler, i, _1));
    ++i;
  }

  if (!wait_end.get_future().get()) {
    std::cout << "Collect statistics failed";
    return 1;
  }

  p_worker.reset();
  threads.join_all();

  DisplayStatistics(statistics, wait_usec);

  return 0;
}

void DisplayStatistics(const Accumulator& statistics, int expected_wait) {
  std::cout << "Number of samples    : "
            << boost::accumulators::count(statistics) << std::endl;
  std::cout << "Expected wait (usec) : " << expected_wait << std::endl;
  std::cout << "Mean (usec)          : "
            << boost::accumulators::mean(statistics) << std::endl;
  std::cout << "Median (usec)        : "
            << boost::accumulators::median(statistics) << std::endl;
  std::cout << "Min (usec)           : " << boost::accumulators::min(statistics)
            << std::endl;
  std::cout << "Max (usec)           : " << boost::accumulators::max(statistics)
            << std::endl;
}
