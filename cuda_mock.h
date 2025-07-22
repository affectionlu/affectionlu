// Mock CUDA definitions for analysis
#ifndef __CUDA_MOCK_H__
#define __CUDA_MOCK_H__

// CUDA function attributes
#define __global__ __attribute__((global))
#define __device__ __attribute__((device))
#define __host__ __attribute__((host))

// CUDA built-in variables  
struct __builtin_blockIdx_t {
    unsigned int x, y, z;
};
struct __builtin_blockDim_t {
    unsigned int x, y, z;
};
struct __builtin_threadIdx_t {
    unsigned int x, y, z;
};

extern const __builtin_blockIdx_t blockIdx;
extern const __builtin_blockDim_t blockDim;
extern const __builtin_threadIdx_t threadIdx;

// CUDA runtime functions (simplified)
extern "C" {
    int cudaMalloc(void** ptr, size_t size);
    int cudaMemcpy(void* dst, const void* src, size_t count, int kind);
    int cudaFree(void* ptr);
}

// For kernel launch syntax <<<>>>
#define cudaConfigureCall(grid, block, shmem, stream) /* mock */

#endif
