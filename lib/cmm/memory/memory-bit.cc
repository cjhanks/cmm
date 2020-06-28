#include "memory-bit.hh"

#include <functional>

#include "cmm/tools/error.hh"
#include "cmm/tools/stream.hh"

#include "cmm/logging.hh"


namespace cmm {
// -------------------------------------------------------------------------- //

GpuMemorySegmentList::GpuMemorySegmentList(std::size_t size)
  : size(size)
{
}

GpuMemorySegmentList::GpuMemorySegmentList(GpuMemorySegmentList&& rhs)
{
  *this = std::move(rhs);
}

GpuMemorySegmentList&
GpuMemorySegmentList::operator=(GpuMemorySegmentList&& rhs)
{
  size = rhs.size;
  segments = std::move(rhs.segments);
  return *this;
}

GpuMemorySegment
GpuMemorySegmentList::Next()
{
  if (segments.size() == 0) {
    void* ptr = nullptr;
    Error::Check(cudaMalloc(&ptr, size));
    return ptr;
  }

  std::lock_guard<std::mutex> lock(mutex);
  auto elem = segments.front();
  segments.pop_front();
  return elem;
}

void
GpuMemorySegmentList::Return(GpuMemorySegment ptr_gpu)
{
  std::lock_guard<std::mutex> lock(mutex);
  segments.emplace_back(ptr_gpu);
}

// -------------------------------------------------------------------------- //

PinnedMemorySegmentList::PinnedMemorySegmentList(std::size_t size)
  : size(size)
{
}

PinnedMemorySegmentList::PinnedMemorySegmentList(PinnedMemorySegmentList&& rhs)
{
  *this = std::move(rhs);
}

PinnedMemorySegmentList&
PinnedMemorySegmentList::operator=(PinnedMemorySegmentList&& rhs)
{
  size = rhs.size;
  segments = std::move(rhs.segments);
  return *this;
}

PinnedMemorySegment
PinnedMemorySegmentList::Next()
{
  if (segments.size() == 0) {
  }

  std::lock_guard<std::mutex> lock(mutex);
  auto elem = segments.front();
  segments.pop_front();
  return elem;
}

void
PinnedMemorySegmentList::Return(void* ptr_gpu, void* ptr_cpu)
{
  std::lock_guard<std::mutex> lock(mutex);
  segments.emplace_back(
      (PinnedMemorySegment) {.ptr_gpu = ptr_gpu,
                             .ptr_cpu = ptr_cpu}
  );
}

// -------------------------------------------------------------------------- //

namespace {
MemoryManager mm;
} // ns

MemoryManager&
MemoryManager::Instance()
{
  mm.ValidateOrThrow();
  return mm;
}

namespace bit {
void
InstallDiscretizer(std::unique_ptr<Discretizer>&& discretizer)
{
  mm.Install(std::move(discretizer));
}
} // ns bit

MemoryManager::MemoryManager()
  : terminated(false)
{}

MemoryManager::~MemoryManager()
{
  terminated = true;
  thread_handle.join();
}

void
MemoryManager::Install(std::unique_ptr<Discretizer>&& discretizer)
{
  this->discretizer = std::move(discretizer);
  Start();
}

void
MemoryManager::Start()
{
  thread_handle =
      std::move(std::thread(std::bind(&MemoryManager::Loop, this)));
}

PinnedMemory
MemoryManager::NewPinned(std::size_t bytes)
{
  std::size_t size = discretizer->Compute(bytes);
  pin_memory_lock.lock();
  auto allocator = pin_memory.find(size);
  if (allocator == pin_memory.end()) {
    allocator = pin_memory.emplace(
                  size,
                  std::move(PinnedMemorySegmentList(size))).first;
  }

  auto segment = allocator->second.Next();
  pin_memory_lock.unlock();

  return PinnedMemory(segment.ptr_gpu,
                      segment.ptr_cpu,
                      bytes);
}

void
MemoryManager::Free(PinnedMemory& memory)
{
  ReturnRecord record;
  record.ptr_gpu = memory.ptr_gpu;
  record.ptr_cpu = memory.ptr_cpu;
  record.size    = discretizer->Compute(memory.Size());
  record.event   = StreamEvent::Create();
  record.stream  = Stream::This();
  returns.Push(std::move(record));
}

GpuMemory
MemoryManager::NewGpu(std::size_t bytes)
{
  std::size_t size = discretizer->Compute(bytes);
  gpu_memory_lock.lock();
  auto allocator = gpu_memory.find(size);
  if (allocator == gpu_memory.end()) {
    allocator = gpu_memory.emplace(
                  size,
                  std::move(GpuMemorySegmentList(size))).first;
  }

  auto segment = allocator->second.Next();
  gpu_memory_lock.unlock();
  return GpuMemory(segment, bytes);
}

void
MemoryManager::Free(GpuMemory& memory)
{
  ReturnRecord record;
  record.ptr_gpu = memory.ptr;
  record.ptr_cpu = nullptr;
  record.size    = discretizer->Compute(memory.Size());
  record.event   = StreamEvent::Create();
  record.stream  = Stream::This();
  returns.Push(std::move(record));
}

void
MemoryManager::Loop()
{
  using RecordList = std::deque<ReturnRecord>;
  using RecordMap = std::unordered_map<cudaStream_t, RecordList>;

  RecordMap record_map;
  while (!terminated) {
    // Look for records which should be returned to the pool and queue them in
    // their respective stream queue.
    ReturnRecord record;
    while (returns.Pop(record, 0.01))
      record_map[record.stream].emplace_back(std::move(record));

    // Return memory to the pool.
    for (auto& record: record_map) {
      auto& list = record.second;
      while (list.size()
          && list.front().event.IsComplete()) {
        // This element is ready to be returned to the memory pool.
        auto& elem = list.front();

        if (elem.ptr_cpu) {
          // It must be pinned memory.
          pin_memory_lock.lock();
          auto& pm = pin_memory.at(elem.size);
          pin_memory_lock.unlock();

          pm.Return(elem.ptr_gpu, elem.ptr_cpu);
        } else {
          // It must be gpu memory.
          gpu_memory_lock.lock();
          auto& gm = gpu_memory.at(elem.size);
          gpu_memory_lock.unlock();

          gm.Return(elem.ptr_gpu);
        }

        list.pop_front();
      }
    }
  }
}

void
MemoryManager::ValidateOrThrow() const
{
  if (!discretizer)
    throw Error("Discretizer not set on memory manager");
}
} // ns cmm