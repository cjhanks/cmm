#ifndef CMM_MACRO_HH_
#define CMM_MACRO_HH_

#define cmm_likely(x)       __builtin_expect(!!(x), 1)
#define cmm_unlikely(x)     __builtin_expect(!!(x), 0)

#ifdef __CUDACC__
  #define cmm_host    __host__
  #define cmm_device  __device__
  #define cmm_global  __global__
  #define cmm_method  cmm_host cmm_device
#else
  #define cmm_host
  #define cmm_device
  #define cmm_global
  #define cmm_method  cmm_host cmm_device
#endif

#endif // CMM_MACRO_HH_
