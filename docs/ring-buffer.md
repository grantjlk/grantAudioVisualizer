# Ring Buffer

The visualizer has two places where audio goes from one thread to another:

- the fill thread hands decoded samples to the audio callback
- the audio callback hands a mono copy of what it just played to the render thread for the FFT

Originally both used PortAudio's `PaUtilRingBuffer`. The analyzer was also peeking into the playback buffer from a third thread, which was a race (the buffer is only safe with one reader and one writer) and also meant the bars showed audio that hadn't played yet. Now both handoffs go through my own [`SpscRingBuffer`](../include/SpscRingBuffer.h), which I built in three steps and benchmarked after each one.

## How it works

It's a single-producer/single-consumer buffer, so one thread only writes and the other only reads, and there are no locks. That matters here because the audio callback isn't allowed to block or allocate, so a mutex isn't really an option.

The producer is the only thread that changes `writeIndex` and the consumer is the only one that changes `readIndex`. The indices just keep counting up and never wrap, and the actual slot is `index & mask`. Because of that, `writeIndex - readIndex` is always how many items are in the buffer (even after `size_t` overflows, unsigned subtraction still works out), so I can use every slot instead of leaving one empty to tell full from empty.

The `& mask` trick only works if the capacity is a power of two. You can do a ring buffer with any size, it just needs a modulo or a compare and reset instead, so this is a speed choice. If you pass a size that isn't a power of two the constructor throws. PortAudio's buffer returns an error code there, and my old code was ignoring it.

`write` and `read` move a whole chunk at a time (up to two copies, in case it wraps around the end), since the app moves 512 samples per callback.

## The three versions

| Version | Commit | Change |
|---|---|---|
| v1 | `1a4baf9` | `std::atomic` with the default `seq_cst` ordering |
| v2 | `7646ef6` | acquire/release ordering |
| v3 | `7460226` | each thread caches the other thread's index, and every index gets its own cache line |

### v1 to v2: memory ordering

The default `seq_cst` store compiles to an `xchg` on x86, which is a full fence, and v1 was doing that on every call. An SPSC buffer only needs one guarantee: the data copied into the slots has to be visible to the other thread before the new index is. A release store on the index plus an acquire load on the other side gives exactly that, and on x86 both are just normal `mov`s.

The rule I ended up using everywhere is relaxed when loading your own index (nobody else changes it), acquire when loading the other thread's index, and release when storing yours.

### v2 to v3: cached indices and padding

Even with cheaper ordering, every call still read the other thread's index. That index lives in the other core's cache, so every single call meant pulling a cache line across cores.

So now the producer keeps its own copy of `readIndex` and only reads the real one when its copy says the buffer is full. The consumer does the same thing with `writeIndex` when it looks empty. This is safe because the copy can only be behind the real value. At worst it thinks there's less room than there really is, never more.

That only helps if the cached copies aren't on the same cache line as the other thread's index. Otherwise every time the other thread writes its index, the copy gets kicked out of cache too (false sharing). So `writeIndex`, `readIndex` and both cached copies each get `alignas(64)`, and the fields that never change (`capacity`, `mask`, the buffer pointer) sit on their own line, which both cores can keep without it bouncing. This is the same layout Erik Rigtorp's [SPSCQueue](https://github.com/rigtorp/SPSCQueue) uses, which is why I added it to the benchmark to compare against.

## Results

These come from [`bench/RingBufferBench.cpp`](../bench/RingBufferBench.cpp), in millions of samples per second. Each number is the median of 15 runs, and I ran the benchmark twice each time. I measured each version on a different day, so I reran PortAudio's buffer in each session to have something to compare against.

Chunk 1 means every sample is its own handoff, so it's basically all synchronization cost:

| | SpscRingBuffer | PaUtil (same session) | rigtorp (same session) |
|---|---|---|---|
| v1 `seq_cst` | 13.8 / 13.8 | 29.3 / 29.7 | |
| v2 acquire/release | 29.8 / 31.8 | 32.4 / 30.4 | |
| v3 cached + padded | 133.8 / 144.1 | 31.3 / 27.6 | 274.5 / 280.6 |

Chunk 512 is what the app actually moves per callback, and it's mostly the cost of copying memory:

| | SpscRingBuffer | PaUtil (same session) |
|---|---|---|
| v1 | 2188 / 2204 | 2377 / 2378 |
| v2 | 2495 / 2533 | 2508 / 2549 |
| v3 | 2417 / 2416 | 2471 / 2373 |

At chunk 1, v2 was about 2.2x faster than v1 just from getting rid of the fence, which put it even with PaUtil. v3 was another 4.5x on top of that, so about 4.5x faster than PaUtil.

At chunk 512 everything ties. Once each call is copying 2 KB, the copy costs way more than the synchronization. So for this app specifically, the speedup doesn't matter. Either buffer is way faster than it needs to be (one callback's handoff takes well under a microsecond, and a callback happens every 5.8 ms). The app's frame times and underruns didn't change after I switched both handoffs over, which is what I expected. The point was getting the synchronization right and understanding what it costs.

Rigtorp's queue is about 2x faster than mine at chunk 1. It only pushes and pops one element per call, so I left it out of the chunk 512 table since looping it 512 times wouldn't be a fair comparison. I haven't figured out the reason for the gap yet.

## Checking that it's correct

The [unit tests](../tests/SpscRingBufferTests.cpp) cover ordering, full and empty, partial reads, and wraparound, plus a stress test where two threads push 2 million values through a 256 slot buffer with different chunk sizes and check every value comes out once and in order. To make sure that test actually catches ordering bugs, I swapped v1 to store the index before copying the data, and it failed 5 out of 5 times.

The benchmark checks itself too. The producer writes a counter and the consumer checks every value, so a buffer that's fast but wrong fails instead of looking good.

I also ran ThreadSanitizer (GCC in WSL2). My buffer (v2 and v3) and rigtorp's came back clean on the tests and the benchmark. PaUtil got three reports: `pa_ringbuffer.c:82` in `GetReadAvailable`, `:193` in `AdvanceReadIndex`, and the `memcpy`. PortAudio synchronizes with `volatile` and its own barriers. That works fine on x86 in practice, but it's outside the C++ memory model, so TSan can't verify it. With `std::atomic` and acquire/release, it can.

## How I measured

- AMD Ryzen 5 7535U, Windows 11, MSVC 2022, Release build. Plugged in with Windows power mode on Best performance (on battery everything was about half as fast).
- The producer and consumer are pinned to logical CPUs 2 and 4. Those are different physical cores, since Windows numbers hyperthread siblings next to each other, and it stays off CPU 0, which handles a lot of interrupts.
- Every buffer gets 8192 slots of `uint32_t`. Rigtorp's queue allocates one extra slot internally, so the usable size is the same.
- There's one warm-up run that isn't counted, then 15 runs per buffer. The buffers take turns (paUtil, spsc, rigtorp, paUtil, ...) so if the CPU speeds up or slows down partway through, it affects all of them about the same.
- I report the median instead of the average so one bad run doesn't throw it off. At chunk 1 a single run sometimes drops way lower, but the medians still land within a few percent between invocations.
- Both threads wait on a start flag once they're pinned, the consumer times itself, and the benchmark's own flags are on a separate cache line so they don't add false sharing of their own.
- PaUtil isn't the same baseline on every platform. On MSVC its barriers are compiler-only, which on x86 is basically acquire/release for free. With GCC on x86 they're real `mfence`/`lfence`/`sfence` instructions, so I'd expect it to be slower there.

### Running it yourself

```bash
# from the build directory (Release)
Release\RingBufferBench.exe                    # all buffers
Release\RingBufferBench.exe spsc rigtorp       # just some of them
Release\RingBufferBench.exe --cpus 2,4 --runs 15
```

ThreadSanitizer from WSL2 (this skips the app since it needs the vcpkg libraries):

```bash
cmake -S /mnt/c/path/to/grantAudioVisualizer -B ~/audiovis-tsan -DAUDIOVIS_BUILD_APP=OFF \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS=-fsanitize=thread -DCMAKE_C_FLAGS=-fsanitize=thread \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST
cmake --build ~/audiovis-tsan -j
cd ~/audiovis-tsan
setarch x86_64 -R ./tests/SpscRingBufferTests      # setarch gets around a TSan/ASLR issue on newer kernels
setarch x86_64 -R ./RingBufferBench --runs 1
```
