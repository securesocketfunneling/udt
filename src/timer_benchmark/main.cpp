#include <future>
#include <iostream>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/chrono.hpp>

#include <boost/asio/basic_waitable_timer.hpp>

#include <boost/thread.hpp>

typedef boost::accumulators::accumulator_set<
    int, boost::accumulators::features<
             boost::accumulators::tag::min, boost::accumulators::tag::max,
             boost::accumulators::tag::mean, boost::accumulators::tag::median>>
    Accumulator;

void DisplayStatistics(const Accumulator& statistics, int expected_wait);

int main(int argc, char* argv[]) {
  namespace chrono = boost::chrono;
  typedef chrono::high_resolution_clock Clock;
  typedef boost::asio::basic_waitable_timer<Clock> ClockTimer;
  typedef chrono::time_point<Clock> ClockTimePoint;
  typedef std::function<void(const boost::system::error_code&)> TimerHandler;

  if (argc < 3) {
    std::cout << "timer_benchmark [wait_usec] [sample_size]" << std::endl;
    return 0;
  }

  boost::asio::io_service io_service;
  int count = 0;
  int wait_usec = atoi(argv[1]);
  int sample_size = atoi(argv[2]);
  std::promise<bool> wait_end;
  Accumulator statistics;
  ClockTimer timer(io_service);
  TimerHandler wait_handler;
  ClockTimePoint last_now;

  if (wait_usec < 1) wait_usec = 50;
  if (sample_size < 1) sample_size = 10000;

  wait_handler = [&](const boost::system::error_code& ec) {
    statistics(static_cast<int>(
        chrono::duration_cast<chrono::microseconds>(Clock::now() - last_now)
            .count()));
    ++count;
    if (!ec && count < sample_size) {
      last_now = Clock::now();
      timer.expires_from_now(chrono::microseconds(wait_usec));
      timer.async_wait(wait_handler);
    } else {
      wait_end.set_value(ec.value() == 0);
    }
  };

  last_now = Clock::now();
  timer.expires_from_now(chrono::microseconds(wait_usec));
  timer.async_wait(wait_handler);

  boost::thread_group threads;
  for (uint16_t i = 1; i <= boost::thread::hardware_concurrency(); ++i) {
    threads.create_thread([&io_service]() { io_service.run(); });
  }

  if (!wait_end.get_future().get()) {
    std::cout << "Collect statistics failed";
    return 1;
  }

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