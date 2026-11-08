__global__ void k(int *p){ p[threadIdx.x] += 1; }
int main(){ return 0; }
