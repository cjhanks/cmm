#ifndef CMM_OPAQUE_POINTER_HH_
#define CMM_OPAQUE_POINTER_HH_

#include <cstring>

namespace cmm {
class MemoryManager;

///
/// @class PinnedMemory
///
class PinnedMemory {
 public:
  PinnedMemory();
  PinnedMemory(std::size_t size);
  ~PinnedMemory();

  /// {
  PinnedMemory(PinnedMemory&& rhs);
  PinnedMemory&
  operator=(PinnedMemory&& rhs);

  PinnedMemory(const PinnedMemory&) = delete;
  PinnedMemory&
  operator=(const PinnedMemory&) = delete;
  /// }

  /// {
  const void*
  PointerGPU() const;

  void*
  PointerGPU();

  const void*
  PointerCPU() const;

  void*
  PointerCPU();
  /// }

  /// {
  void
  TransferToGPU(bool async=true);

  void
  TransferToCPU(bool async=false);
  /// }

  std::size_t
  Size() const;

 private:
  enum class Device {
    GPU,
    CPU,
  };

  Device device;
  bool   dirty;

  void* ptr_gpu;
  void* ptr_cpu;
  std::size_t size;

  friend class MemoryManager;
  PinnedMemory(void* ptr_gpu, void* ptr_cpu, std::size_t size);
};

///
/// @class GpuMemory
///
class GpuMemory {
 public:
  GpuMemory();
  GpuMemory(std::size_t size);
  ~GpuMemory();

  /// {
  GpuMemory(GpuMemory&& rhs);
  GpuMemory&
  operator=(GpuMemory&& rhs);

  GpuMemory(const GpuMemory&) = delete;
  GpuMemory&
  operator=(const GpuMemory&) = delete;
  /// }

  void
  Load(void* cpu_ptr);

  const void*
  PointerGPU() const;

  void*
  PointerGPU();

  std::size_t
  Size() const;

 private:
  void* ptr;
  std::size_t size;

  friend class MemoryManager;
  GpuMemory(void* ptr, std::size_t size);
};
} // ns cmm

#endif // CMM_POINTER_HH_
