#ifndef CMM_ERROR_HH_
#define CMM_ERROR_HH_

#include <stdexcept>
#include <cuda_runtime.h>

#include "cmm/macro.hh"

namespace cmm {
///
/// @class Error
///
/// Base class for all cmm exception types.
///
class Error : public std::runtime_error {
 public:
  ///
  /// Check the cudaError_t return code.  If the return code is not "success",
  /// throw an `cmm::Error`.
  ///
  static void
  Check(cudaError_t rc) {
    if (cmm_unlikely(rc != cudaSuccess)) {
      Throw(rc);
    }
  }

  explicit Error(std::string message);

 private:
  static void
  Throw(cudaError_t rc);
};
} // ns cmm

#endif // CMM_ERROR_HH_
