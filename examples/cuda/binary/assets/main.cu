#include <iostream>
#include <cassert>
#include <vector>

#include <cuda_runtime_api.h>


__global__ void squareMatrixMul(
    const int *matrix_A,
    const int *matrix_B,
    int *matrix_C,
    const int d){
  // check kernel shape
  assert(blockDim.x == blockDim.y);
  assert(gridDim.x == gridDim.y);

  // obtain corresponding row and column for current thread
  int row_index = blockIdx.y * blockDim.y + threadIdx.y;
  int col_index = blockIdx.x * blockDim.x + threadIdx.x;

  // initialize destination element
  int dest_index = row_index*d+col_index;
  matrix_C[dest_index] = 0;

  // sum of product
  if(dest_index < d*d){
    for(int i=0; i<d; i++){
      matrix_C[dest_index] += matrix_A[row_index*d+i] * matrix_B[i*d+col_index];
    }
  }
}


int main(){
  // initialize constants
  constexpr int N = 1 << 10;
  constexpr int unit_size = sizeof(int);
  constexpr int matrix_size = N*N*unit_size;

  // create matrices (flat vectors) in host memory
  std::vector<int> matrix_A;
  matrix_A.reserve(N*N);
  std::vector<int> matrix_B;
  matrix_B.reserve(N*N);
  std::vector<int> matrix_C;
  matrix_C.reserve(N*N);

  // initialize random value for each source matrix
  for (int i=0; i<N*N; i++){
      matrix_A.push_back(rand()%100);
      matrix_B.push_back(rand()%100);
  }

  // allocate memory space on device
  int *d_matrix_A, *d_matrix_B, *d_matrix_C;
  cudaMalloc(&d_matrix_A, matrix_size);
  cudaMalloc(&d_matrix_B, matrix_size);
  cudaMalloc(&d_matrix_C, matrix_size);

  // copy data from host memory to device memory
  cudaMemcpy(d_matrix_A, matrix_A.data(), matrix_size, cudaMemcpyHostToDevice);
  cudaMemcpy(d_matrix_B, matrix_B.data(), matrix_size, cudaMemcpyHostToDevice);
  cudaMemcpy(d_matrix_C, matrix_C.data(), matrix_size, cudaMemcpyHostToDevice);

  // initialize kernel configuration
  // number of kernels per block (one dimension)
  int NUM_THREADS_PER_BLOCK = 1 << 5;

  // number of blocks in the grid (one dimension)
  int NUM_BLOCKS =  N % NUM_THREADS_PER_BLOCK == 0 ?
                    N / NUM_THREADS_PER_BLOCK :
                    N / NUM_THREADS_PER_BLOCK + 1;

  // use dim3 struct for block and grid dimensions
  dim3 threads(NUM_THREADS_PER_BLOCK, NUM_THREADS_PER_BLOCK);
  dim3 blocks(NUM_BLOCKS, NUM_BLOCKS);

  // launch kernel
  for(int i=0; i<4; i++)
    squareMatrixMul<<<blocks, threads>>>(d_matrix_A, d_matrix_B, d_matrix_C, N);
  cudaStreamSynchronize(0);

  // copy result back to host memory
  cudaMemcpy(matrix_C.data(), d_matrix_C, matrix_size, cudaMemcpyDeviceToHost);

  // free device memory
  cudaFree(d_matrix_A);
  cudaFree(d_matrix_B);
  cudaFree(d_matrix_C);

  std::cout << "Get correct matrix multiplication result!" << std::endl;

  return 0;
}
