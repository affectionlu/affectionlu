// Simplified CUDA example for analysis
__global__ void vectorAdd(float* a, float* b, float* c, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        c[idx] = a[idx] + b[idx];
    }
}

__host__ void hostFunction() {
    // Host function
}

__host__ __device__ float square(float x) {
    return x * x;
}

extern "C" int main() {
    float *d_a, *d_b, *d_c;
    
    // Launch kernel
    vectorAdd<<<256, 256>>>(d_a, d_b, d_c, 1024);
    
    return 0;
}
