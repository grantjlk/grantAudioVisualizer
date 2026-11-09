// Ring buffer throughput benchmark.
//
// One producer thread writes a counter (0, 1, 2, ...) into the buffer in fixed size chunks while one
// consumer thread reads it back out, both as fast as they can. The consumer checks every value arrives
// exactly once and in order, so a buffer that's fast but wrong fails instead of winning, and the compiler
// can't optimize the work away. Result is millions of samples per second through the buffer.
//
// usage: RingBufferBench [buffer names...] [--cpus producer,consumer] [--runs N]
//   eg.  RingBufferBench                 (all buffers)
//        RingBufferBench spsc --cpus 2,4

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "BufferAdapters.h"

#if defined(_WIN32)
	#define NOMINMAX
	#include <windows.h>
	#include <intrin.h>
#elif defined(__linux__)
	#include <pthread.h>
	#include <sched.h>
#endif

// same size as the app's ring buffer
constexpr size_t kCapacity = 8192;

// chunk 1 is the worst case, every sample is its own handoff between the threads so it's almost all
// synchronization cost. 512 is what the app actually moves per callback (256 frames x 2 channels)
struct ChunkConfig{
	size_t chunk;
	uint64_t samplesPerRun;		// sized so one run takes roughly 0.2 - 2 s
};
constexpr ChunkConfig kChunks[] = {
	{1, 20'000'000},
	{512, 400'000'000},
};

// pins the calling thread to one logical cpu, so the os can't move it mid run (which would show up as noise)
static bool pinToCpu(int cpu){
	if(cpu < 0 || cpu >= 64){
		return false;
	}
#if defined(_WIN32)
	return SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR(1) << cpu) != 0;
#elif defined(__linux__)
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(cpu, &set);
	return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
#else
	return false;
#endif
}

// tells the cpu we're in a spin loop. on x86 this is the pause instruction, which saves power and stops
// the spinning thread from stealing resources from its hyperthread sibling
static inline void cpuRelax(){
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
	_mm_pause();
#elif defined(__x86_64__) || defined(__i386__)
	__builtin_ia32_pause();
#endif
}

struct RunResult{
	double msamplesPerSec;
	bool inOrder;
	uint64_t failedAt;
};

// the harness's own shared flags, on their own cache line. if they sat next to the buffer's indices,
// every check of them would drag the index cache line between cores too (false sharing in the
// harness itself), and how they happen to land on the stack could favour one buffer over another
struct alignas(64) Control{
	std::atomic<int> ready{0};			// threads that are pinned and waiting
	std::atomic<bool> go{false};
	std::atomic<bool> stop{false};		// consumer sets this if it finds a bad value, so the producer doesn't spin forever
};

template <typename Buffer>
static RunResult runOnce(size_t chunk, uint64_t total, int producerCpu, int consumerCpu){
	// on the heap so it's nowhere near the harness's locals
	auto buffer = std::make_unique<Buffer>(kCapacity);
	Control control;
	RunResult result{0.0, true, 0};

	std::thread producer([&]() {
		pinToCpu(producerCpu);
		std::vector<Sample> data(chunk);
		uint64_t next = 0;
		control.ready.fetch_add(1);
		while(!control.go.load(std::memory_order_acquire)){
			cpuRelax();
		}
		while(next < total){
			size_t count = static_cast<size_t>(std::min<uint64_t>(chunk, total - next));
			for(size_t i = 0; i < count; i++){
				data[i] = static_cast<Sample>(next + i);
			}
			// keep trying until the whole chunk is in, the consumer will make room. stop only gets checked
			// when the buffer is full, so it costs nothing on the normal path
			size_t written = 0;
			while(written < count){
				size_t w = buffer->write(data.data() + written, count - written);
				if(w == 0){
					if(control.stop.load(std::memory_order_relaxed)){
						return;
					}
					cpuRelax();
				}
				written += w;
			}
			next += count;
		}
	});

	std::thread consumer([&]() {
		pinToCpu(consumerCpu);
		std::vector<Sample> out(chunk);
		uint64_t expected = 0;
		control.ready.fetch_add(1);
		while(!control.go.load(std::memory_order_acquire)){
			cpuRelax();
		}
		// the consumer times the run itself (first wait for data to last value checked), so thread
		// join and main thread scheduling aren't part of the number
		auto start = std::chrono::steady_clock::now();
		while(expected < total){
			size_t got = buffer->read(out.data(), chunk);
			if(got == 0){
				cpuRelax();
				continue;
			}
			for(size_t i = 0; i < got; i++){
				if(out[i] != static_cast<Sample>(expected)){
					result.inOrder = false;
					result.failedAt = expected;
					control.stop.store(true, std::memory_order_relaxed);
					return;
				}
				expected++;
			}
		}
		double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
		result.msamplesPerSec = (total / seconds) / 1e6;
	});

	// wait until both threads are actually running and pinned, then release them at the same time
	while(control.ready.load() < 2){
		std::this_thread::yield();
	}
	control.go.store(true, std::memory_order_release);
	consumer.join();
	producer.join();
	return result;
}

// one buffer in the comparison, runOnce<Adapter> for that buffer's type
struct BenchEntry{
	const char* name;
	RunResult (*run)(size_t chunk, uint64_t total, int producerCpu, int consumerCpu);
	std::vector<double> results;
	bool failed;
};

template <typename Buffer>
static BenchEntry makeEntry(){
	return BenchEntry{Buffer::name, &runOnce<Buffer>, {}, false};
}

static void reportFailure(BenchEntry& entry, size_t chunk, const RunResult& run){
	std::printf("%-8s chunk %4zu  FAILED: wrong value at sample %llu\n", entry.name, chunk, (unsigned long long)run.failedAt);
	entry.failed = true;
}

int main(int argc, char* argv[]){
	std::vector<std::string> selected;
	// defaults avoid cpu 0 (where a lot of os interrupts land) and use two different physical cores.
	// on windows hyperthread siblings are numbered next to each other (0+1 = core 0, 2+3 = core 1, ...)
	int producerCpu = 2;
	int consumerCpu = 4;
	// more runs = a median that moves less between invocations. 15 keeps a full run around a minute
	int runs = 15;

	for(int i = 1; i < argc; i++){
		if(std::strcmp(argv[i], "--cpus") == 0 && i + 1 < argc){
			if(std::sscanf(argv[++i], "%d,%d", &producerCpu, &consumerCpu) != 2){
				std::printf("--cpus expects two numbers like 2,4\n");
				return 1;
			}
		} else if(std::strcmp(argv[i], "--runs") == 0 && i + 1 < argc){
			runs = std::max(1, std::atoi(argv[++i]));
		} else {
			selected.push_back(argv[i]);
		}
	}

	std::vector<BenchEntry> all = {
		makeEntry<PaUtilAdapter>(),
		makeEntry<SpscAdapter>(),
	};
	std::vector<BenchEntry> entries;
	for(const BenchEntry& entry : all){
		if(selected.empty() || std::find(selected.begin(), selected.end(), entry.name) != selected.end()){
			entries.push_back(entry);
		}
	}
	if(entries.empty()){
		std::printf("no buffer with that name, options are: paUtil spsc\n");
		return 1;
	}

	std::printf("capacity %zu, producer on cpu %d, consumer on cpu %d, median of %d runs after 1 warm up\n\n",
		kCapacity, producerCpu, consumerCpu, runs);

	bool allOk = true;
	for(const ChunkConfig& config : kChunks){
		for(BenchEntry& entry : entries){
			entry.results.clear();
			entry.failed = false;
			// one untimed warm up run first (caches, page faults on the fresh buffer, cpu clocking up)
			RunResult warmup = entry.run(config.chunk, config.samplesPerRun, producerCpu, consumerCpu);
			if(!warmup.inOrder){
				reportFailure(entry, config.chunk, warmup);
			}
		}

		// take turns (paUtil, spsc, paUtil, spsc, ...) instead of all of one buffer then all of the next.
		// laptop cpus change clock speed constantly, so if the machine speeds up or slows down partway
		// through, it hits every buffer about equally instead of looking like one buffer is faster
		for(int r = 0; r < runs; r++){
			for(BenchEntry& entry : entries){
				if(entry.failed){
					continue;
				}
				RunResult run = entry.run(config.chunk, config.samplesPerRun, producerCpu, consumerCpu);
				if(!run.inOrder){
					reportFailure(entry, config.chunk, run);
					continue;
				}
				entry.results.push_back(run.msamplesPerSec);
			}
		}

		// median instead of mean so one run hit by a background process doesn't drag the result around.
		// spread shows how much the runs disagreed, if it's big the number isn't trustworthy
		for(BenchEntry& entry : entries){
			if(entry.failed){
				allOk = false;
				continue;
			}
			std::sort(entry.results.begin(), entry.results.end());
			double median = entry.results[entry.results.size() / 2];
			double spreadPct = (entry.results.back() - entry.results.front()) / median * 100.0;
			std::printf("%-8s chunk %4zu  median %9.1f Msamples/s  (min %9.1f, max %9.1f, spread %4.1f%%)\n",
				entry.name, config.chunk, median, entry.results.front(), entry.results.back(), spreadPct);
		}
		std::printf("\n");
	}

	return allOk ? 0 : 1;
}
