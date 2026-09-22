#include "limestone/sweep.hpp"

#include <algorithm>
#include <mutex>
#include <thread>

namespace limestone {
namespace {

// Per-stop travel times collected by one worker, merged after the threads join.
using Samples = std::vector<std::vector<int>>;

void run_range(const Timetable& timetable, const Transfers& transfers,
               const std::vector<Origin>& origins, const SweepOptions& options, int first, int last,
               Samples& samples) {
    Query query;
    query.target = kNoTarget;
    query.max_trips = options.max_trips;
    query.transfer_buffer = options.transfer_buffer;
    query.origins.resize(origins.size());

    for (int departure = first; departure != last; departure += options.step_seconds) {
        query.departure = departure;
        for (std::size_t i = 0; i != origins.size(); ++i) {
            // An origin's departure field holds the walk from the query point,
            // so the rider leaves at `departure` and arrives that much later.
            query.origins[i] = Origin{origins[i].stop, departure + origins[i].departure};
        }

        const RaptorResult result = run_raptor(timetable, query, &transfers);
        for (std::size_t stop = 0; stop != result.best.size(); ++stop) {
            if (result.best[stop] != kUnreachable) {
                samples[stop].push_back(result.best[stop] - departure);
            }
        }
    }
}

int median_of(std::vector<int>& values) {
    if (values.empty()) {
        return -1;
    }
    const std::size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    return values[middle];
}

}

SweepResult run_sweep(const Timetable& timetable, const Transfers& transfers,
                      const std::vector<Origin>& origins, const SweepOptions& options) {
    const int stop_count = timetable.stop_count;
    const int step = std::max(1, options.step_seconds);

    std::vector<int> departures;
    for (int t = options.first_departure; t <= options.last_departure; t += step) {
        departures.push_back(t);
    }

    SweepResult result;
    result.departures = static_cast<int>(departures.size());
    result.stops.assign(stop_count, StopAccess{});
    if (departures.empty()) {
        return result;
    }

    const int thread_count = std::max(1, options.threads);
    // Chunks keep each worker busy for thousands of queries at a time, so the
    // mutex is taken a few dozen times rather than once per departure.
    const int chunk = std::max(8, static_cast<int>(departures.size()) / (thread_count * 4));

    std::vector<Samples> per_thread(thread_count, Samples(stop_count));
    std::mutex mutex;
    std::size_t next = 0;

    const auto worker = [&](int id) {
        while (true) {
            std::size_t begin = 0;
            std::size_t end = 0;
            {
                const std::lock_guard<std::mutex> lock(mutex);
                if (next == departures.size()) {
                    return;
                }
                begin = next;
                end = std::min(departures.size(), next + static_cast<std::size_t>(chunk));
                next = end;
            }

            run_range(timetable, transfers, origins, options, departures[begin],
                      departures[end - 1] + step, per_thread[id]);
        }
    };

    if (thread_count == 1) {
        worker(0);
    } else {
        std::vector<std::thread> threads;
        threads.reserve(thread_count);
        for (int id = 0; id != thread_count; ++id) {
            threads.emplace_back(worker, id);
        }
        for (std::thread& thread : threads) {
            thread.join();
        }
    }

    std::vector<int> merged;
    for (int stop = 0; stop != stop_count; ++stop) {
        merged.clear();
        for (Samples& samples : per_thread) {
            merged.insert(merged.end(), samples[stop].begin(), samples[stop].end());
        }

        StopAccess& access = result.stops[stop];
        access.samples = result.departures;
        access.reached = static_cast<int>(merged.size());
        if (!merged.empty()) {
            access.best_seconds = *std::min_element(merged.begin(), merged.end());
            access.median_seconds = median_of(merged);
        }
    }

    return result;
}

}