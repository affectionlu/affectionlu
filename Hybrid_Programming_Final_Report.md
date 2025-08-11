# Pthread+MPI混合并行编程完整技术报告

## 1. 混合编程模型概述

### 1.1 混合编程简介

Pthread+MPI混合编程结合了共享内存并行（Pthread）和分布式内存并行（MPI）的优势，实现两级并行：
- **节点间并行**：使用MPI在计算节点之间进行通信
- **节点内并行**：使用Pthread在单个节点内的多核之间进行并行

### 1.2 基本混合程序结构

```c
#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int thread_id;
    int mpi_rank;
    int start_index;
    int end_index;
    double *data;
    double *result;
} thread_data_t;

void* worker_thread(void* arg) {
    thread_data_t* tdata = (thread_data_t*)arg;
    
    // 每个线程处理分配的数据段
    for (int i = tdata->start_index; i < tdata->end_index; i++) {
        tdata->result[i] = tdata->data[i] * tdata->data[i];
    }
    
    return NULL;
}

int main(int argc, char *argv[]) {
    int provided, required = MPI_THREAD_MULTIPLE;
    MPI_Init_thread(&argc, &argv, required, &provided);
    
    if (provided < required) {
        printf("MPI threading support insufficient\n");
        MPI_Finalize();
        return 1;
    }
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int DATA_SIZE = 10000;
    const int NUM_THREADS = 4;
    
    double *data = malloc(DATA_SIZE * sizeof(double));
    double *result = malloc(DATA_SIZE * sizeof(double));
    
    // 初始化数据
    for (int i = 0; i < DATA_SIZE; i++) {
        data[i] = rank * DATA_SIZE + i;
    }
    
    // 创建线程执行计算
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    
    int chunk_size = DATA_SIZE / NUM_THREADS;
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].mpi_rank = rank;
        thread_data[i].start_index = i * chunk_size;
        thread_data[i].end_index = (i + 1) * chunk_size;
        thread_data[i].data = data;
        thread_data[i].result = result;
        
        pthread_create(&threads[i], NULL, worker_thread, &thread_data[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // MPI通信
    double local_sum = 0.0;
    for (int i = 0; i < DATA_SIZE; i++) {
        local_sum += result[i];
    }
    
    double global_sum;
    MPI_Allreduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    
    if (rank == 0) {
        printf("Global sum: %.2f\n", global_sum);
    }
    
    free(data);
    free(result);
    MPI_Finalize();
    return 0;
}
```

## 2. MPI线程安全级别

### 2.1 线程安全级别详解

MPI提供四种线程安全级别：

```c
void check_thread_support() {
    int provided, required;
    
    // 测试不同的安全级别要求
    int levels[] = {MPI_THREAD_SINGLE, MPI_THREAD_FUNNELED, 
                   MPI_THREAD_SERIALIZED, MPI_THREAD_MULTIPLE};
    char *level_names[] = {"SINGLE", "FUNNELED", "SERIALIZED", "MULTIPLE"};
    
    for (int i = 0; i < 4; i++) {
        required = levels[i];
        MPI_Init_thread(NULL, NULL, required, &provided);
        
        printf("Requested: %s, Provided: %s\n", 
               level_names[i], level_names[provided]);
        
        if (provided >= required) {
            printf("  ✓ Requirement satisfied\n");
        } else {
            printf("  ✗ Requirement not met\n");
        }
        
        MPI_Finalize();
    }
}
```

### 2.2 不同安全级别的编程模式

#### 2.2.1 MPI_THREAD_FUNNELED模式

```c
// 只有主线程调用MPI
pthread_mutex_t mpi_access_mutex = PTHREAD_MUTEX_INITIALIZER;
int is_main_thread = 0;

void* funneled_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    // 只有主线程（thread_id == 0）可以调用MPI
    if (thread_id == 0) {
        is_main_thread = 1;
        
        // 准备数据
        double local_data[1000];
        for (int i = 0; i < 1000; i++) {
            local_data[i] = i * 3.14;
        }
        
        // MPI通信
        MPI_Bcast(local_data, 1000, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        
        printf("Main thread completed MPI broadcast\n");
    } else {
        // 其他线程执行计算
        printf("Worker thread %d performing computation\n", thread_id);
        
        double computation_result = 0.0;
        for (int i = 0; i < 100000; i++) {
            computation_result += sin(i * 0.001);
        }
    }
    
    return NULL;
}
```

#### 2.2.2 MPI_THREAD_SERIALIZED模式

```c
pthread_mutex_t mpi_mutex = PTHREAD_MUTEX_INITIALIZER;

void* serialized_worker(void* arg) {
    int thread_id = *(int*)arg;
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // 所有线程都可以调用MPI，但必须串行化
    pthread_mutex_lock(&mpi_mutex);
    
    double send_data = thread_id * rank * 2.5;
    double recv_data;
    
    // 一次只能有一个线程调用MPI
    MPI_Allreduce(&send_data, &recv_data, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    
    pthread_mutex_unlock(&mpi_mutex);
    
    printf("Thread %d on rank %d: sent %.2f, received %.2f\n", 
           thread_id, rank, send_data, recv_data);
    
    return NULL;
}
```

#### 2.2.3 MPI_THREAD_MULTIPLE模式

```c
void* multiple_worker(void* arg) {
    int thread_id = *(int*)arg;
    int rank, size;
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // 多个线程可以同时调用MPI
    double *send_data = malloc(1000 * sizeof(double));
    double *recv_data = malloc(1000 * sizeof(double));
    
    for (int i = 0; i < 1000; i++) {
        send_data[i] = thread_id * 1000 + i;
    }
    
    // 每个线程与不同的目标进程通信
    int target_rank = (rank + thread_id + 1) % size;
    int source_rank = (rank - thread_id - 1 + size) % size;
    
    MPI_Request send_req, recv_req;
    
    MPI_Isend(send_data, 1000, MPI_DOUBLE, target_rank, thread_id, 
              MPI_COMM_WORLD, &send_req);
    MPI_Irecv(recv_data, 1000, MPI_DOUBLE, source_rank, thread_id, 
              MPI_COMM_WORLD, &recv_req);
    
    // 在通信进行时执行计算
    double local_computation = 0.0;
    for (int i = 0; i < 50000; i++) {
        local_computation += sqrt(i) * log(i + 1);
    }
    
    MPI_Wait(&send_req, MPI_STATUS_IGNORE);
    MPI_Wait(&recv_req, MPI_STATUS_IGNORE);
    
    printf("Thread %d on rank %d: completed communication with rank %d\n",
           thread_id, rank, target_rank);
    
    free(send_data);
    free(recv_data);
    
    return NULL;
}
```

## 3. 混合编程设计模式

### 3.1 Master-Worker模式

```c
typedef struct {
    pthread_mutex_t task_mutex;
    pthread_cond_t task_available;
    pthread_cond_t task_completed;
    int *task_queue;
    int queue_front;
    int queue_rear;
    int queue_size;
    int active_tasks;
    bool shutdown;
} task_pool_t;

task_pool_t global_task_pool;

void* master_thread(void* arg) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (rank == 0) {
        // 主进程的主线程：任务分发
        for (int task_id = 0; task_id < 10000; task_id++) {
            
            // 将任务添加到本地队列
            pthread_mutex_lock(&global_task_pool.task_mutex);
            
            while ((global_task_pool.queue_rear + 1) % global_task_pool.queue_size == 
                   global_task_pool.queue_front) {
                pthread_cond_wait(&global_task_pool.task_completed, &global_task_pool.task_mutex);
            }
            
            global_task_pool.task_queue[global_task_pool.queue_rear] = task_id;
            global_task_pool.queue_rear = (global_task_pool.queue_rear + 1) % global_task_pool.queue_size;
            global_task_pool.active_tasks++;
            
            pthread_cond_signal(&global_task_pool.task_available);
            pthread_mutex_unlock(&global_task_pool.task_mutex);
            
            // 向其他进程分发任务
            if (task_id % 100 == 0) {
                for (int dest = 1; dest < size; dest++) {
                    int batch_tasks[10];
                    for (int i = 0; i < 10 && task_id + i < 10000; i++) {
                        batch_tasks[i] = task_id + i;
                    }
                    MPI_Send(batch_tasks, 10, MPI_INT, dest, 0, MPI_COMM_WORLD);
                }
            }
        }
        
        // 发送终止信号
        int terminate_signal = -1;
        for (int dest = 1; dest < size; dest++) {
            MPI_Send(&terminate_signal, 1, MPI_INT, dest, 0, MPI_COMM_WORLD);
        }
        
    } else {
        // 其他进程的主线程：接收任务
        while (1) {
            int received_tasks[10];
            MPI_Status status;
            
            MPI_Recv(received_tasks, 10, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
            
            if (received_tasks[0] == -1) break;
            
            // 将接收到的任务添加到本地队列
            pthread_mutex_lock(&global_task_pool.task_mutex);
            
            for (int i = 0; i < 10 && received_tasks[i] != -1; i++) {
                global_task_pool.task_queue[global_task_pool.queue_rear] = received_tasks[i];
                global_task_pool.queue_rear = (global_task_pool.queue_rear + 1) % global_task_pool.queue_size;
                global_task_pool.active_tasks++;
            }
            
            pthread_cond_broadcast(&global_task_pool.task_available);
            pthread_mutex_unlock(&global_task_pool.task_mutex);
        }
    }
    
    // 通知工作线程停止
    pthread_mutex_lock(&global_task_pool.task_mutex);
    global_task_pool.shutdown = true;
    pthread_cond_broadcast(&global_task_pool.task_available);
    pthread_mutex_unlock(&global_task_pool.task_mutex);
    
    return NULL;
}

void* worker_thread(void* arg) {
    int thread_id = *(int*)arg;
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    while (1) {
        pthread_mutex_lock(&global_task_pool.task_mutex);
        
        // 等待任务或终止信号
        while (global_task_pool.queue_front == global_task_pool.queue_rear && 
               !global_task_pool.shutdown) {
            pthread_cond_wait(&global_task_pool.task_available, &global_task_pool.task_mutex);
        }
        
        if (global_task_pool.shutdown) {
            pthread_mutex_unlock(&global_task_pool.task_mutex);
            break;
        }
        
        // 获取任务
        int task_id = global_task_pool.task_queue[global_task_pool.queue_front];
        global_task_pool.queue_front = (global_task_pool.queue_front + 1) % global_task_pool.queue_size;
        global_task_pool.active_tasks--;
        
        pthread_cond_signal(&global_task_pool.task_completed);
        pthread_mutex_unlock(&global_task_pool.task_mutex);
        
        // 执行任务
        printf("Thread %d on rank %d processing task %d\n", thread_id, rank, task_id);
        
        // 模拟任务处理
        double result = 0.0;
        for (int i = 0; i < task_id % 1000 + 1000; i++) {
            result += sin(i * 0.001) * cos(task_id * 0.001);
        }
        
        usleep((task_id % 100) * 1000);  // 模拟不同任务时间
    }
    
    return NULL;
}
```

### 3.2 数据并行模式

```c
typedef struct {
    int rank;
    int size;
    int thread_id;
    int num_threads;
    double **matrix_a;
    double **matrix_b;
    double **matrix_c;
    int matrix_size;
    int start_row;
    int end_row;
} matrix_mult_data_t;

void* matrix_multiply_thread(void* arg) {
    matrix_mult_data_t* data = (matrix_mult_data_t*)arg;
    
    // 计算分配给此线程的矩阵行范围
    int rows_per_thread = (data->end_row - data->start_row) / data->num_threads;
    int thread_start_row = data->start_row + data->thread_id * rows_per_thread;
    int thread_end_row = (data->thread_id == data->num_threads - 1) ? 
                         data->end_row : thread_start_row + rows_per_thread;
    
    printf("Thread %d on rank %d processing rows %d to %d\n",
           data->thread_id, data->rank, thread_start_row, thread_end_row - 1);
    
    // 执行矩阵乘法
    for (int i = thread_start_row; i < thread_end_row; i++) {
        for (int j = 0; j < data->matrix_size; j++) {
            data->matrix_c[i][j] = 0.0;
            for (int k = 0; k < data->matrix_size; k++) {
                data->matrix_c[i][j] += data->matrix_a[i][k] * data->matrix_b[k][j];
            }
        }
    }
    
    return NULL;
}

void hybrid_matrix_multiplication() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int MATRIX_SIZE = 1000;
    const int NUM_THREADS = 4;
    
    // 分配矩阵内存
    double **matrix_a = allocate_2d_array(MATRIX_SIZE, MATRIX_SIZE);
    double **matrix_b = allocate_2d_array(MATRIX_SIZE, MATRIX_SIZE);
    double **matrix_c = allocate_2d_array(MATRIX_SIZE, MATRIX_SIZE);
    
    // 计算每个进程负责的行范围
    int rows_per_process = MATRIX_SIZE / size;
    int start_row = rank * rows_per_process;
    int end_row = (rank == size - 1) ? MATRIX_SIZE : start_row + rows_per_process;
    
    // 初始化矩阵A的本地部分
    for (int i = start_row; i < end_row; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            matrix_a[i][j] = i * MATRIX_SIZE + j + rank * 0.1;
        }
    }
    
    // 所有进程需要完整的矩阵B
    if (rank == 0) {
        // 初始化矩阵B
        for (int i = 0; i < MATRIX_SIZE; i++) {
            for (int j = 0; j < MATRIX_SIZE; j++) {
                matrix_b[i][j] = (i + j) * 0.5;
            }
        }
    }
    
    // 广播矩阵B到所有进程
    for (int i = 0; i < MATRIX_SIZE; i++) {
        MPI_Bcast(matrix_b[i], MATRIX_SIZE, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    }
    
    // 创建线程进行并行计算
    pthread_t threads[NUM_THREADS];
    matrix_mult_data_t thread_data[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].rank = rank;
        thread_data[i].size = size;
        thread_data[i].thread_id = i;
        thread_data[i].num_threads = NUM_THREADS;
        thread_data[i].matrix_a = matrix_a;
        thread_data[i].matrix_b = matrix_b;
        thread_data[i].matrix_c = matrix_c;
        thread_data[i].matrix_size = MATRIX_SIZE;
        thread_data[i].start_row = start_row;
        thread_data[i].end_row = end_row;
        
        pthread_create(&threads[i], NULL, matrix_multiply_thread, &thread_data[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Rank %d completed local matrix multiplication\n", rank);
    
    // 收集结果到进程0
    if (rank == 0) {
        // 接收其他进程的结果
        for (int src = 1; src < size; src++) {
            int src_start = src * rows_per_process;
            int src_end = (src == size - 1) ? MATRIX_SIZE : src_start + rows_per_process;
            
            for (int i = src_start; i < src_end; i++) {
                MPI_Recv(matrix_c[i], MATRIX_SIZE, MPI_DOUBLE, src, i, 
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        }
        
        printf("Process 0: Collected complete result matrix\n");
        
    } else {
        // 发送本地结果到进程0
        for (int i = start_row; i < end_row; i++) {
            MPI_Send(matrix_c[i], MATRIX_SIZE, MPI_DOUBLE, 0, i, MPI_COMM_WORLD);
        }
    }
    
    // 清理内存
    free_2d_array(matrix_a, MATRIX_SIZE);
    free_2d_array(matrix_b, MATRIX_SIZE);
    free_2d_array(matrix_c, MATRIX_SIZE);
}
```

## 4. 通信与计算重叠

### 4.1 通信线程分离

```c
typedef struct {
    pthread_mutex_t comm_mutex;
    pthread_cond_t send_ready;
    pthread_cond_t recv_ready;
    double *send_buffer;
    double *recv_buffer;
    bool send_pending;
    bool recv_pending;
    bool send_completed;
    bool recv_completed;
    int buffer_size;
    bool shutdown;
} communication_context_t;

communication_context_t comm_ctx;

void* communication_thread(void* arg) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    MPI_Request send_request = MPI_REQUEST_NULL;
    MPI_Request recv_request = MPI_REQUEST_NULL;
    
    while (!comm_ctx.shutdown) {
        pthread_mutex_lock(&comm_ctx.comm_mutex);
        
        // 检查是否有发送请求
        if (comm_ctx.send_pending && send_request == MPI_REQUEST_NULL) {
            int dest = (rank + 1) % size;
            MPI_Isend(comm_ctx.send_buffer, comm_ctx.buffer_size, MPI_DOUBLE, 
                     dest, 0, MPI_COMM_WORLD, &send_request);
            comm_ctx.send_pending = false;
        }
        
        // 检查是否有接收请求
        if (comm_ctx.recv_pending && recv_request == MPI_REQUEST_NULL) {
            int source = (rank - 1 + size) % size;
            MPI_Irecv(comm_ctx.recv_buffer, comm_ctx.buffer_size, MPI_DOUBLE,
                     source, 0, MPI_COMM_WORLD, &recv_request);
            comm_ctx.recv_pending = false;
        }
        
        pthread_mutex_unlock(&comm_ctx.comm_mutex);
        
        // 测试通信完成状态
        if (send_request != MPI_REQUEST_NULL) {
            int flag;
            MPI_Test(&send_request, &flag, MPI_STATUS_IGNORE);
            if (flag) {
                pthread_mutex_lock(&comm_ctx.comm_mutex);
                comm_ctx.send_completed = true;
                pthread_cond_signal(&comm_ctx.send_ready);
                pthread_mutex_unlock(&comm_ctx.comm_mutex);
                send_request = MPI_REQUEST_NULL;
            }
        }
        
        if (recv_request != MPI_REQUEST_NULL) {
            int flag;
            MPI_Test(&recv_request, &flag, MPI_STATUS_IGNORE);
            if (flag) {
                pthread_mutex_lock(&comm_ctx.comm_mutex);
                comm_ctx.recv_completed = true;
                pthread_cond_signal(&comm_ctx.recv_ready);
                pthread_mutex_unlock(&comm_ctx.comm_mutex);
                recv_request = MPI_REQUEST_NULL;
            }
        }
        
        usleep(1000);  // 短暂休眠避免忙等
    }
    
    // 清理未完成的请求
    if (send_request != MPI_REQUEST_NULL) {
        MPI_Cancel(&send_request);
        MPI_Request_free(&send_request);
    }
    if (recv_request != MPI_REQUEST_NULL) {
        MPI_Cancel(&recv_request);
        MPI_Request_free(&recv_request);
    }
    
    return NULL;
}

void* computation_thread(void* arg) {
    int thread_id = *(int*)arg;
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    const int ITERATIONS = 100;
    
    for (int iter = 0; iter < ITERATIONS; iter++) {
        // 准备发送数据
        pthread_mutex_lock(&comm_ctx.comm_mutex);
        
        for (int i = 0; i < comm_ctx.buffer_size; i++) {
            comm_ctx.send_buffer[i] = iter * comm_ctx.buffer_size + i + thread_id * 0.1;
        }
        
        comm_ctx.send_pending = true;
        comm_ctx.send_completed = false;
        pthread_mutex_unlock(&comm_ctx.comm_mutex);
        
        // 启动接收
        pthread_mutex_lock(&comm_ctx.comm_mutex);
        comm_ctx.recv_pending = true;
        comm_ctx.recv_completed = false;
        pthread_mutex_unlock(&comm_ctx.comm_mutex);
        
        // 在通信进行时执行计算
        double computation_result = 0.0;
        for (int i = 0; i < 100000; i++) {
            computation_result += sin(i * 0.001 + thread_id) * cos(iter * 0.001);
        }
        
        printf("Thread %d on rank %d: iteration %d, computation result: %.6f\n",
               thread_id, rank, iter, computation_result);
        
        // 等待通信完成
        pthread_mutex_lock(&comm_ctx.comm_mutex);
        while (!comm_ctx.send_completed) {
            pthread_cond_wait(&comm_ctx.send_ready, &comm_ctx.comm_mutex);
        }
        pthread_mutex_unlock(&comm_ctx.comm_mutex);
        
        pthread_mutex_lock(&comm_ctx.comm_mutex);
        while (!comm_ctx.recv_completed) {
            pthread_cond_wait(&comm_ctx.recv_ready, &comm_ctx.comm_mutex);
        }
        pthread_mutex_unlock(&comm_ctx.comm_mutex);
        
        // 处理接收到的数据
        double received_sum = 0.0;
        for (int i = 0; i < comm_ctx.buffer_size; i++) {
            received_sum += comm_ctx.recv_buffer[i];
        }
        
        printf("Thread %d on rank %d: received data sum: %.2f\n", 
               thread_id, rank, received_sum);
    }
    
    return NULL;
}
```

### 4.2 计算通信管道化

```c
typedef struct {
    double **computation_stages;
    double **communication_buffers;
    pthread_mutex_t *stage_mutexes;
    pthread_cond_t *stage_conditions;
    bool *stage_ready;
    int num_stages;
    int stage_size;
    bool pipeline_active;
} pipeline_context_t;

pipeline_context_t pipeline_ctx;

void* pipeline_stage_thread(void* arg) {
    int stage_id = *(int*)arg;
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int ITERATIONS = 50;
    
    for (int iter = 0; iter < ITERATIONS; iter++) {
        // 等待上一阶段数据准备就绪
        if (stage_id > 0) {
            pthread_mutex_lock(&pipeline_ctx.stage_mutexes[stage_id - 1]);
            while (!pipeline_ctx.stage_ready[stage_id - 1]) {
                pthread_cond_wait(&pipeline_ctx.stage_conditions[stage_id - 1], 
                                &pipeline_ctx.stage_mutexes[stage_id - 1]);
            }
            
            // 复制输入数据
            memcpy(pipeline_ctx.computation_stages[stage_id], 
                   pipeline_ctx.computation_stages[stage_id - 1],
                   pipeline_ctx.stage_size * sizeof(double));
            
            pipeline_ctx.stage_ready[stage_id - 1] = false;
            pthread_mutex_unlock(&pipeline_ctx.stage_mutexes[stage_id - 1]);
        } else {
            // 第一阶段：初始化数据
            for (int i = 0; i < pipeline_ctx.stage_size; i++) {
                pipeline_ctx.computation_stages[stage_id][i] = 
                    iter * pipeline_ctx.stage_size + i + rank * 0.1;
            }
        }
        
        // 执行本阶段计算
        for (int i = 0; i < pipeline_ctx.stage_size; i++) {
            double input = pipeline_ctx.computation_stages[stage_id][i];
            pipeline_ctx.computation_stages[stage_id][i] = 
                sin(input + stage_id) * cos(input * stage_id);
        }
        
        printf("Stage %d on rank %d completed iteration %d\n", stage_id, rank, iter);
        
        // 如果是最后阶段，启动MPI通信
        if (stage_id == pipeline_ctx.num_stages - 1) {
            // 准备通信数据
            memcpy(pipeline_ctx.communication_buffers[0], 
                   pipeline_ctx.computation_stages[stage_id],
                   pipeline_ctx.stage_size * sizeof(double));
            
            // 与相邻进程交换数据
            int left_neighbor = (rank - 1 + size) % size;
            int right_neighbor = (rank + 1) % size;
            
            MPI_Request send_req, recv_req;
            MPI_Isend(pipeline_ctx.communication_buffers[0], pipeline_ctx.stage_size, 
                     MPI_DOUBLE, right_neighbor, iter, MPI_COMM_WORLD, &send_req);
            MPI_Irecv(pipeline_ctx.communication_buffers[1], pipeline_ctx.stage_size,
                     MPI_DOUBLE, left_neighbor, iter, MPI_COMM_WORLD, &recv_req);
            
            MPI_Wait(&send_req, MPI_STATUS_IGNORE);
            MPI_Wait(&recv_req, MPI_STATUS_IGNORE);
            
            printf("Stage %d on rank %d completed communication for iteration %d\n", 
                   stage_id, rank, iter);
        }
        
        // 通知下一阶段数据已准备好
        pthread_mutex_lock(&pipeline_ctx.stage_mutexes[stage_id]);
        pipeline_ctx.stage_ready[stage_id] = true;
        pthread_cond_signal(&pipeline_ctx.stage_conditions[stage_id]);
        pthread_mutex_unlock(&pipeline_ctx.stage_mutexes[stage_id]);
    }
    
    return NULL;
}
```

## 5. 动态负载均衡

### 5.1 工作窃取算法

```c
typedef struct {
    pthread_mutex_t queue_mutex;
    int *work_queue;
    int queue_front;
    int queue_rear;
    int queue_capacity;
    int work_count;
    bool stealing_enabled;
} work_stealing_queue_t;

work_stealing_queue_t local_queues[16];  // 每个线程一个队列
int num_local_threads;

void* work_stealing_thread(void* arg) {
    int thread_id = *(int*)arg;
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    work_stealing_queue_t* my_queue = &local_queues[thread_id];
    
    while (1) {
        int work_item = -1;
        
        // 尝试从自己的队列获取工作
        pthread_mutex_lock(&my_queue->queue_mutex);
        if (my_queue->work_count > 0) {
            work_item = my_queue->work_queue[my_queue->queue_front];
            my_queue->queue_front = (my_queue->queue_front + 1) % my_queue->queue_capacity;
            my_queue->work_count--;
        }
        pthread_mutex_unlock(&my_queue->queue_mutex);
        
        // 如果没有工作，尝试从其他线程窃取
        if (work_item == -1) {
            for (int victim = 0; victim < num_local_threads; victim++) {
                if (victim == thread_id) continue;
                
                work_stealing_queue_t* victim_queue = &local_queues[victim];
                
                pthread_mutex_lock(&victim_queue->queue_mutex);
                if (victim_queue->work_count > 1 && victim_queue->stealing_enabled) {
                    // 从队列尾部窃取工作（分治算法的特性）
                    victim_queue->queue_rear = (victim_queue->queue_rear - 1 + 
                                              victim_queue->queue_capacity) % victim_queue->queue_capacity;
                    work_item = victim_queue->work_queue[victim_queue->queue_rear];
                    victim_queue->work_count--;
                    
                    printf("Thread %d stole work %d from thread %d\n", 
                           thread_id, work_item, victim);
                }
                pthread_mutex_unlock(&victim_queue->queue_mutex);
                
                if (work_item != -1) break;
            }
        }
        
        // 如果本地没有工作，尝试从其他进程请求
        if (work_item == -1) {
            for (int target_rank = 0; target_rank < size; target_rank++) {
                if (target_rank == rank) continue;
                
                // 发送工作请求
                int request = 1;
                MPI_Send(&request, 1, MPI_INT, target_rank, 100, MPI_COMM_WORLD);
                
                // 接收响应
                int response;
                MPI_Status status;
                MPI_Recv(&response, 1, MPI_INT, target_rank, 101, MPI_COMM_WORLD, &status);
                
                if (response != -1) {
                    work_item = response;
                    printf("Thread %d on rank %d received work %d from rank %d\n",
                           thread_id, rank, work_item, target_rank);
                    break;
                }
            }
        }
        
        if (work_item == -1) {
            // 没有更多工作，休眠一段时间
            usleep(10000);
            continue;
        }
        
        // 执行工作
        printf("Thread %d on rank %d processing work item %d\n", thread_id, rank, work_item);
        
        // 模拟工作处理
        double result = 0.0;
        for (int i = 0; i < work_item % 10000 + 5000; i++) {
            result += sin(i * 0.001 + work_item) * cos(i * 0.002);
        }
        
        // 模拟可能产生新的子任务
        if (work_item > 100 && (work_item % 10 == 0)) {
            int subtask1 = work_item / 2;
            int subtask2 = work_item / 2 + 1;
            
            // 添加子任务到自己的队列
            pthread_mutex_lock(&my_queue->queue_mutex);
            if (my_queue->work_count < my_queue->queue_capacity - 2) {
                my_queue->work_queue[my_queue->queue_rear] = subtask1;
                my_queue->queue_rear = (my_queue->queue_rear + 1) % my_queue->queue_capacity;
                my_queue->work_count++;
                
                my_queue->work_queue[my_queue->queue_rear] = subtask2;
                my_queue->queue_rear = (my_queue->queue_rear + 1) % my_queue->queue_capacity;
                my_queue->work_count++;
            }
            pthread_mutex_unlock(&my_queue->queue_mutex);
        }
    }
    
    return NULL;
}

void* work_request_handler(void* arg) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    while (1) {
        int request;
        MPI_Status status;
        
        // 非阻塞检查工作请求
        int flag;
        MPI_Iprobe(MPI_ANY_SOURCE, 100, MPI_COMM_WORLD, &flag, &status);
        
        if (flag) {
            MPI_Recv(&request, 1, MPI_INT, status.MPI_SOURCE, 100, 
                    MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            int response = -1;
            
            // 查找可以共享的工作
            for (int i = 0; i < num_local_threads; i++) {
                work_stealing_queue_t* queue = &local_queues[i];
                
                pthread_mutex_lock(&queue->queue_mutex);
                if (queue->work_count > 2) {
                    // 给出一半的工作
                    queue->queue_rear = (queue->queue_rear - 1 + queue->queue_capacity) % 
                                       queue->queue_capacity;
                    response = queue->work_queue[queue->queue_rear];
                    queue->work_count--;
                }
                pthread_mutex_unlock(&queue->queue_mutex);
                
                if (response != -1) break;
            }
            
            MPI_Send(&response, 1, MPI_INT, status.MPI_SOURCE, 101, MPI_COMM_WORLD);
            
            if (response != -1) {
                printf("Rank %d shared work %d with rank %d\n", 
                       rank, response, status.MPI_SOURCE);
            }
        }
        
        usleep(5000);
    }
    
    return NULL;
}
```

## 6. NUMA优化和亲和性设置

### 6.1 NUMA感知的内存分配

```c
#include <numa.h>
#include <numaif.h>

typedef struct {
    int thread_id;
    int numa_node;
    double *local_data;
    size_t data_size;
    int cpu_core;
} numa_thread_data_t;

void* numa_aware_worker(void* arg) {
    numa_thread_data_t* data = (numa_thread_data_t*)arg;
    
    // 设置线程运行在指定的CPU核心
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(data->cpu_core, &cpuset);
    
    int result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        printf("Failed to set CPU affinity for thread %d\n", data->thread_id);
    }
    
    // 设置内存分配策略到指定NUMA节点
    unsigned long nodemask = 1UL << data->numa_node;
    set_mempolicy(MPOL_BIND, &nodemask, sizeof(nodemask) * 8);
    
    // 分配NUMA本地内存
    data->local_data = numa_alloc_onnode(data->data_size, data->numa_node);
    if (data->local_data == NULL) {
        printf("Failed to allocate NUMA-local memory for thread %d\n", data->thread_id);
        return NULL;
    }
    
    printf("Thread %d allocated %.2f MB on NUMA node %d, CPU core %d\n",
           data->thread_id, data->data_size / (1024.0 * 1024.0), 
           data->numa_node, data->cpu_core);
    
    // 初始化数据以确保在本地NUMA节点上分配物理页面
    for (size_t i = 0; i < data->data_size / sizeof(double); i++) {
        data->local_data[i] = data->thread_id * 1000000 + i;
    }
    
    // 执行内存密集型计算
    double sum = 0.0;
    for (int iter = 0; iter < 100; iter++) {
        for (size_t i = 0; i < data->data_size / sizeof(double); i++) {
            data->local_data[i] = sqrt(data->local_data[i]) + sin(iter * 0.01);
            sum += data->local_data[i];
        }
    }
    
    printf("Thread %d completed computation, sum: %.2e\n", data->thread_id, sum);
    
    // 清理
    numa_free(data->local_data, data->data_size);
    
    return NULL;
}

void setup_numa_topology() {
    if (numa_available() < 0) {
        printf("NUMA not available\n");
        return;
    }
    
    int num_nodes = numa_max_node() + 1;
    int num_cpus = numa_num_configured_cpus();
    
    printf("NUMA topology: %d nodes, %d CPUs\n", num_nodes, num_cpus);
    
    // 显示每个NUMA节点的CPU核心
    for (int node = 0; node < num_nodes; node++) {
        struct bitmask *cpus = numa_allocate_cpumask();
        numa_node_to_cpus(node, cpus);
        
        printf("NUMA node %d CPUs:", node);
        for (int cpu = 0; cpu < num_cpus; cpu++) {
            if (numa_bitmask_isbitset(cpus, cpu)) {
                printf(" %d", cpu);
            }
        }
        printf("\n");
        
        numa_free_cpumask(cpus);
    }
}

void hybrid_numa_aware_computation() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    setup_numa_topology();
    
    int num_nodes = numa_max_node() + 1;
    int num_cpus = numa_num_configured_cpus();
    int threads_per_node = 4;  // 每个NUMA节点4个线程
    int total_threads = num_nodes * threads_per_node;
    
    pthread_t *threads = malloc(total_threads * sizeof(pthread_t));
    numa_thread_data_t *thread_data = malloc(total_threads * sizeof(numa_thread_data_t));
    
    // 为每个NUMA节点创建线程
    for (int node = 0; node < num_nodes; node++) {
        // 获取该节点的CPU列表
        struct bitmask *cpus = numa_allocate_cpumask();
        numa_node_to_cpus(node, cpus);
        
        int cpu_list[64];
        int cpu_count = 0;
        for (int cpu = 0; cpu < num_cpus; cpu++) {
            if (numa_bitmask_isbitset(cpus, cpu)) {
                cpu_list[cpu_count++] = cpu;
            }
        }
        
        // 在该节点创建线程
        for (int t = 0; t < threads_per_node && t < cpu_count; t++) {
            int thread_id = node * threads_per_node + t;
            
            thread_data[thread_id].thread_id = thread_id;
            thread_data[thread_id].numa_node = node;
            thread_data[thread_id].cpu_core = cpu_list[t];
            thread_data[thread_id].data_size = 64 * 1024 * 1024;  // 64MB per thread
            
            pthread_create(&threads[thread_id], NULL, numa_aware_worker, 
                          &thread_data[thread_id]);
        }
        
        numa_free_cpumask(cpus);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < total_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Rank %d completed NUMA-aware computation\n", rank);
    
    free(threads);
    free(thread_data);
}
```

## 7. 性能监控和调优

### 7.1 详细性能测量

```c
typedef struct {
    double computation_time;
    double communication_time;
    double synchronization_time;
    double idle_time;
    long cache_misses;
    long memory_accesses;
    double cpu_utilization;
} thread_performance_t;

typedef struct {
    thread_performance_t *thread_stats;
    double total_execution_time;
    double load_imbalance;
    double communication_efficiency;
    int num_threads;
} process_performance_t;

process_performance_t perf_stats;

void* performance_monitoring_thread(void* arg) {
    int thread_id = *(int*)arg;
    thread_performance_t *stats = &perf_stats.thread_stats[thread_id];
    
    struct timespec start_time, end_time, comp_start, comp_end;
    struct timespec comm_start, comm_end, sync_start, sync_end;
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    const int ITERATIONS = 1000;
    
    for (int iter = 0; iter < ITERATIONS; iter++) {
        // 计算阶段
        clock_gettime(CLOCK_MONOTONIC, &comp_start);
        
        double computation_result = 0.0;
        for (int i = 0; i < 100000; i++) {
            computation_result += sin(i * 0.001 + thread_id) * cos(iter * 0.001);
        }
        
        clock_gettime(CLOCK_MONOTONIC, &comp_end);
        stats->computation_time += (comp_end.tv_sec - comp_start.tv_sec) + 
                                  (comp_end.tv_nsec - comp_start.tv_nsec) / 1e9;
        
        // 同步阶段
        clock_gettime(CLOCK_MONOTONIC, &sync_start);
        pthread_barrier_wait(&computation_barrier);
        clock_gettime(CLOCK_MONOTONIC, &sync_end);
        
        stats->synchronization_time += (sync_end.tv_sec - sync_start.tv_sec) + 
                                      (sync_end.tv_nsec - sync_start.tv_nsec) / 1e9;
        
        // 主线程负责MPI通信
        if (thread_id == 0) {
            clock_gettime(CLOCK_MONOTONIC, &comm_start);
            
            double send_data = computation_result;
            double recv_data;
            MPI_Allreduce(&send_data, &recv_data, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            
            clock_gettime(CLOCK_MONOTONIC, &comm_end);
            stats->communication_time += (comm_end.tv_sec - comm_start.tv_sec) + 
                                        (comm_end.tv_nsec - comm_start.tv_nsec) / 1e9;
        }
        
        pthread_barrier_wait(&communication_barrier);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    
    double total_thread_time = (end_time.tv_sec - start_time.tv_sec) + 
                              (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    
    stats->idle_time = total_thread_time - stats->computation_time - 
                      stats->communication_time - stats->synchronization_time;
    
    stats->cpu_utilization = (stats->computation_time / total_thread_time) * 100.0;
    
    printf("Thread %d performance summary:\n", thread_id);
    printf("  Computation time: %.6f seconds (%.1f%%)\n", 
           stats->computation_time, (stats->computation_time / total_thread_time) * 100);
    printf("  Communication time: %.6f seconds (%.1f%%)\n", 
           stats->communication_time, (stats->communication_time / total_thread_time) * 100);
    printf("  Synchronization time: %.6f seconds (%.1f%%)\n", 
           stats->synchronization_time, (stats->synchronization_time / total_thread_time) * 100);
    printf("  Idle time: %.6f seconds (%.1f%%)\n", 
           stats->idle_time, (stats->idle_time / total_thread_time) * 100);
    
    return NULL;
}

void analyze_performance() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // 计算本进程的性能统计
    double max_comp_time = 0.0, min_comp_time = DBL_MAX, total_comp_time = 0.0;
    double max_comm_time = 0.0, min_comm_time = DBL_MAX, total_comm_time = 0.0;
    
    for (int i = 0; i < perf_stats.num_threads; i++) {
        thread_performance_t *stats = &perf_stats.thread_stats[i];
        
        max_comp_time = fmax(max_comp_time, stats->computation_time);
        min_comp_time = fmin(min_comp_time, stats->computation_time);
        total_comp_time += stats->computation_time;
        
        max_comm_time = fmax(max_comm_time, stats->communication_time);
        min_comm_time = fmin(min_comm_time, stats->communication_time);
        total_comm_time += stats->communication_time;
    }
    
    double avg_comp_time = total_comp_time / perf_stats.num_threads;
    perf_stats.load_imbalance = ((max_comp_time - min_comp_time) / avg_comp_time) * 100.0;
    
    // 收集全局统计信息
    double global_max_comp, global_min_comp, global_avg_comp;
    double global_max_comm, global_min_comm, global_avg_comm;
    
    MPI_Allreduce(&max_comp_time, &global_max_comp, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
    MPI_Allreduce(&min_comp_time, &global_min_comp, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&avg_comp_time, &global_avg_comp, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    global_avg_comp /= size;
    
    MPI_Allreduce(&max_comm_time, &global_max_comm, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
    MPI_Allreduce(&min_comm_time, &global_min_comm, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&total_comm_time, &global_avg_comm, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    global_avg_comm /= (size * perf_stats.num_threads);
    
    if (rank == 0) {
        printf("\n=== Global Performance Analysis ===\n");
        printf("Computation time statistics:\n");
        printf("  Maximum: %.6f seconds\n", global_max_comp);
        printf("  Minimum: %.6f seconds\n", global_min_comp);
        printf("  Average: %.6f seconds\n", global_avg_comp);
        printf("  Load imbalance: %.2f%%\n", 
               ((global_max_comp - global_min_comp) / global_avg_comp) * 100.0);
        
        printf("\nCommunication time statistics:\n");
        printf("  Maximum: %.6f seconds\n", global_max_comm);
        printf("  Minimum: %.6f seconds\n", global_min_comm);
        printf("  Average: %.6f seconds\n", global_avg_comm);
        
        double comm_efficiency = (global_avg_comp / (global_avg_comp + global_avg_comm)) * 100.0;
        printf("  Communication efficiency: %.2f%%\n", comm_efficiency);
        
        double parallel_efficiency = (global_min_comp / global_max_comp) * 100.0;
        printf("  Parallel efficiency: %.2f%%\n", parallel_efficiency);
    }
}
```

### 7.2 自适应负载均衡

```c
typedef struct {
    double workload_history[100];
    int history_index;
    double average_workload;
    double workload_variance;
    bool needs_rebalancing;
} workload_monitor_t;

workload_monitor_t workload_monitor;

void update_workload_statistics(double current_workload) {
    workload_monitor.workload_history[workload_monitor.history_index] = current_workload;
    workload_monitor.history_index = (workload_monitor.history_index + 1) % 100;
    
    // 计算平均值
    double sum = 0.0;
    for (int i = 0; i < 100; i++) {
        sum += workload_monitor.workload_history[i];
    }
    workload_monitor.average_workload = sum / 100.0;
    
    // 计算方差
    double variance_sum = 0.0;
    for (int i = 0; i < 100; i++) {
        double diff = workload_monitor.workload_history[i] - workload_monitor.average_workload;
        variance_sum += diff * diff;
    }
    workload_monitor.workload_variance = variance_sum / 100.0;
    
    // 判断是否需要重新平衡
    double coefficient_of_variation = sqrt(workload_monitor.workload_variance) / 
                                     workload_monitor.average_workload;
    workload_monitor.needs_rebalancing = (coefficient_of_variation > 0.2);
}

void* adaptive_load_balancer(void* arg) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int REBALANCE_INTERVAL = 50;
    int iteration_count = 0;
    
    while (1) {
        iteration_count++;
        
        // 收集本地负载信息
        double local_computation_time = 0.0;
        for (int i = 0; i < perf_stats.num_threads; i++) {
            local_computation_time += perf_stats.thread_stats[i].computation_time;
        }
        
        update_workload_statistics(local_computation_time);
        
        if (iteration_count % REBALANCE_INTERVAL == 0) {
            // 收集全局负载信息
            double all_workloads[size];
            MPI_Allgather(&workload_monitor.average_workload, 1, MPI_DOUBLE,
                         all_workloads, 1, MPI_DOUBLE, MPI_COMM_WORLD);
            
            // 计算全局负载统计
            double global_avg = 0.0, global_max = 0.0, global_min = DBL_MAX;
            for (int i = 0; i < size; i++) {
                global_avg += all_workloads[i];
                global_max = fmax(global_max, all_workloads[i]);
                global_min = fmin(global_min, all_workloads[i]);
            }
            global_avg /= size;
            
            double load_imbalance = ((global_max - global_min) / global_avg) * 100.0;
            
            if (rank == 0) {
                printf("Load balancing analysis (iteration %d):\n", iteration_count);
                printf("  Global average workload: %.6f\n", global_avg);
                printf("  Load imbalance: %.2f%%\n", load_imbalance);
            }
            
            // 如果负载不平衡超过阈值，进行重新分配
            if (load_imbalance > 15.0) {
                rebalance_workload(all_workloads, size, rank);
            }
        }
        
        sleep(1);
    }
    
    return NULL;
}

void rebalance_workload(double *workloads, int size, int rank) {
    // 找到最忙和最闲的进程
    int busiest_rank = 0, idlest_rank = 0;
    double max_load = workloads[0], min_load = workloads[0];
    
    for (int i = 1; i < size; i++) {
        if (workloads[i] > max_load) {
            max_load = workloads[i];
            busiest_rank = i;
        }
        if (workloads[i] < min_load) {
            min_load = workloads[i];
            idlest_rank = i;
        }
    }
    
    // 计算需要迁移的工作量
    double workload_to_migrate = (max_load - min_load) * 0.3;  // 迁移30%的差值
    
    if (rank == 0) {
        printf("Rebalancing: migrating %.2f%% workload from rank %d to rank %d\n",
               (workload_to_migrate / max_load) * 100.0, busiest_rank, idlest_rank);
    }
    
    // 实现工作负载迁移逻辑
    if (rank == busiest_rank) {
        // 最忙的进程减少工作
        migrate_work_to_process(idlest_rank, workload_to_migrate);
    } else if (rank == idlest_rank) {
        // 最闲的进程接收工作
        receive_work_from_process(busiest_rank);
    }
}
```

## 8. 调试和错误处理

### 8.1 混合程序调试技术

```c
typedef struct {
    int rank;
    int thread_id;
    pthread_t pthread_handle;
    char thread_name[64];
    bool is_mpi_thread;
    double start_time;
    char current_operation[128];
} thread_debug_info_t;

thread_debug_info_t debug_info[64];
int num_debug_threads = 0;
pthread_mutex_t debug_mutex = PTHREAD_MUTEX_INITIALIZER;

void register_thread_for_debugging(int thread_id, const char* thread_name, bool is_mpi) {
    pthread_mutex_lock(&debug_mutex);
    
    debug_info[num_debug_threads].rank = 0;  // Will be set by MPI calls
    debug_info[num_debug_threads].thread_id = thread_id;
    debug_info[num_debug_threads].pthread_handle = pthread_self();
    strncpy(debug_info[num_debug_threads].thread_name, thread_name, 63);
    debug_info[num_debug_threads].is_mpi_thread = is_mpi;
    debug_info[num_debug_threads].start_time = MPI_Wtime();
    strcpy(debug_info[num_debug_threads].current_operation, "CREATED");
    
    MPI_Comm_rank(MPI_COMM_WORLD, &debug_info[num_debug_threads].rank);
    
    num_debug_threads++;
    
    pthread_mutex_unlock(&debug_mutex);
    
    printf("DEBUG: Registered thread %d (%s) on rank %d, MPI-enabled: %s\n",
           thread_id, thread_name, debug_info[num_debug_threads-1].rank, 
           is_mpi ? "YES" : "NO");
}

void update_thread_operation(int thread_id, const char* operation) {
    pthread_mutex_lock(&debug_mutex);
    
    for (int i = 0; i < num_debug_threads; i++) {
        if (debug_info[i].thread_id == thread_id) {
            strncpy(debug_info[i].current_operation, operation, 127);
            break;
        }
    }
    
    pthread_mutex_unlock(&debug_mutex);
}

void* debugging_monitor_thread(void* arg) {
    while (1) {
        sleep(5);  // 每5秒检查一次
        
        pthread_mutex_lock(&debug_mutex);
        
        printf("\n=== Thread Status Report ===\n");
        for (int i = 0; i < num_debug_threads; i++) {
            double runtime = MPI_Wtime() - debug_info[i].start_time;
            printf("Thread %d (%s) on rank %d: %s (running %.2fs)\n",
                   debug_info[i].thread_id, 
                   debug_info[i].thread_name,
                   debug_info[i].rank,
                   debug_info[i].current_operation,
                   runtime);
        }
        printf("============================\n\n");
        
        pthread_mutex_unlock(&debug_mutex);
    }
    
    return NULL;
}

void* deadlock_detection_thread(void* arg) {
    const double DEADLOCK_TIMEOUT = 30.0;  // 30秒超时
    
    typedef struct {
        int thread_id;
        double last_activity_time;
        char last_operation[128];
    } thread_activity_t;
    
    thread_activity_t activities[64];
    int num_activities = 0;
    
    while (1) {
        sleep(10);
        
        double current_time = MPI_Wtime();
        
        pthread_mutex_lock(&debug_mutex);
        
        for (int i = 0; i < num_debug_threads; i++) {
            bool found = false;
            for (int j = 0; j < num_activities; j++) {
                if (activities[j].thread_id == debug_info[i].thread_id) {
                    // 检查操作是否改变
                    if (strcmp(activities[j].last_operation, debug_info[i].current_operation) != 0) {
                        activities[j].last_activity_time = current_time;
                        strcpy(activities[j].last_operation, debug_info[i].current_operation);
                    } else {
                        // 检查是否超时
                        if (current_time - activities[j].last_activity_time > DEADLOCK_TIMEOUT) {
                            printf("WARNING: Possible deadlock detected in thread %d (%s) on rank %d\n",
                                   debug_info[i].thread_id, debug_info[i].thread_name, debug_info[i].rank);
                            printf("  Stuck in operation: %s for %.1f seconds\n",
                                   debug_info[i].current_operation, 
                                   current_time - activities[j].last_activity_time);
                        }
                    }
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                activities[num_activities].thread_id = debug_info[i].thread_id;
                activities[num_activities].last_activity_time = current_time;
                strcpy(activities[num_activities].last_operation, debug_info[i].current_operation);
                num_activities++;
            }
        }
        
        pthread_mutex_unlock(&debug_mutex);
    }
    
    return NULL;
}
```

### 8.2 异常处理和恢复

```c
typedef struct {
    jmp_buf jump_buffer;
    int error_code;
    char error_message[256];
    bool recovery_possible;
} error_context_t;

__thread error_context_t thread_error_context;

void hybrid_error_handler(int signal) {
    switch (signal) {
        case SIGSEGV:
            strcpy(thread_error_context.error_message, "Segmentation fault");
            thread_error_context.error_code = -1;
            break;
        case SIGFPE:
            strcpy(thread_error_context.error_message, "Floating point exception");
            thread_error_context.error_code = -2;
            break;
        case SIGTERM:
            strcpy(thread_error_context.error_message, "Termination signal");
            thread_error_context.error_code = -3;
            break;
        default:
            snprintf(thread_error_context.error_message, 255, "Unknown signal %d", signal);
            thread_error_context.error_code = signal;
            break;
    }
    
    thread_error_context.recovery_possible = false;
    
    int rank, thread_id = *(int*)pthread_getspecific(thread_id_key);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    printf("ERROR: Thread %d on rank %d caught signal %d: %s\n",
           thread_id, rank, signal, thread_error_context.error_message);
    
    longjmp(thread_error_context.jump_buffer, signal);
}

void* fault_tolerant_worker(void* arg) {
    int thread_id = *(int*)arg;
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // 设置信号处理器
    signal(SIGSEGV, hybrid_error_handler);
    signal(SIGFPE, hybrid_error_handler);
    signal(SIGTERM, hybrid_error_handler);
    
    // 设置错误恢复点
    int error_code = setjmp(thread_error_context.jump_buffer);
    
    if (error_code != 0) {
        // 错误恢复逻辑
        printf("Thread %d on rank %d recovering from error %d: %s\n",
               thread_id, rank, error_code, thread_error_context.error_message);
        
        // 尝试恢复或优雅退出
        if (thread_error_context.recovery_possible) {
            printf("Thread %d attempting recovery\n", thread_id);
            // 重置状态，重试操作
        } else {
            printf("Thread %d cannot recover, exiting\n", thread_id);
            pthread_exit(NULL);
        }
    }
    
    // 正常工作循环
    for (int iter = 0; iter < 1000; iter++) {
        update_thread_operation(thread_id, "COMPUTING");
        
        // 可能导致错误的计算
        double *data = malloc(10000 * sizeof(double));
        if (data == NULL) {
            strcpy(thread_error_context.error_message, "Memory allocation failed");
            thread_error_context.error_code = -100;
            thread_error_context.recovery_possible = true;
            longjmp(thread_error_context.jump_buffer, -100);
        }
        
        for (int i = 0; i < 10000; i++) {
            data[i] = sin(i * 0.001 + thread_id + iter);
        }
        
        update_thread_operation(thread_id, "MPI_COMMUNICATION");
        
        // MPI通信（仅主线程）
        if (thread_id == 0) {
            double local_sum = 0.0;
            for (int i = 0; i < 10000; i++) {
                local_sum += data[i];
            }
            
            double global_sum;
            int mpi_result = MPI_Allreduce(&local_sum, &global_sum, 1, MPI_DOUBLE, 
                                          MPI_SUM, MPI_COMM_WORLD);
            
            if (mpi_result != MPI_SUCCESS) {
                char mpi_error[MPI_MAX_ERROR_STRING];
                int length;
                MPI_Error_string(mpi_result, mpi_error, &length);
                
                snprintf(thread_error_context.error_message, 255, 
                        "MPI Error: %s", mpi_error);
                thread_error_context.error_code = mpi_result;
                thread_error_context.recovery_possible = false;
                
                free(data);
                longjmp(thread_error_context.jump_buffer, mpi_result);
            }
        }
        
        free(data);
        update_thread_operation(thread_id, "IDLE");
        usleep(10000);
    }
    
    return NULL;
}
```

## 9. 最佳实践总结

### 9.1 设计原则

1. **选择合适的MPI线程安全级别**
   - `MPI_THREAD_SINGLE`：单线程，最简单但限制最大
   - `MPI_THREAD_FUNNELED`：只有主线程调用MPI，适合计算密集型应用
   - `MPI_THREAD_SERIALIZED`：串行化MPI调用，中等复杂度
   - `MPI_THREAD_MULTIPLE`：完全线程安全，最复杂但最灵活

2. **内存层次优化**
   - 理解NUMA拓扑结构
   - 使用NUMA感知的内存分配
   - 设置合适的线程CPU亲和性
   - 优化数据访问模式

3. **通信优化**
   - 重叠计算和通信
   - 使用非阻塞MPI操作
   - 分离通信线程
   - 批量处理小消息

4. **负载均衡**
   - 实现动态工作分配
   - 使用工作窃取算法
   - 监控负载不平衡
   - 自适应调整策略

### 9.2 常见陷阱

1. **死锁问题**
   - MPI和Pthread锁的交互
   - 线程间的资源竞争
   - 不正确的同步顺序

2. **性能问题**
   - 过度同步开销
   - NUMA不友好的内存访问
   - 线程竞争共享资源
   - 不平衡的工作分布

3. **可移植性问题**
   - MPI实现的差异
   - 不同系统的NUMA拓扑
   - 编译器特定功能

### 9.3 调试策略

1. **使用专门的调试工具**
   - Intel Inspector（线程错误检测）
   - Intel VTune（性能分析）
   - Valgrind/Helgrind（内存和线程错误）
   - TAU（性能监控）

2. **实现运行时监控**
   - 线程状态跟踪
   - 死锁检测
   - 性能计数器
   - 负载监控

3. **错误处理机制**
   - 优雅降级
   - 错误恢复
   - 状态检查点
   - 异常日志

## 10. 总结

Pthread+MPI混合编程是现代高性能计算中的重要编程模式，它充分利用了现代计算系统的多级并行特性。成功的混合编程需要深入理解：

1. **两级并行模型**：节点间MPI通信和节点内Pthread并行
2. **内存层次结构**：从NUMA节点到缓存的多级内存优化
3. **同步和通信**：最小化开销的高效协调机制
4. **负载均衡**：动态适应计算负载变化
5. **性能优化**：通信计算重叠、亲和性设置、内存优化

通过合理的设计和实现，混合编程可以在现代HPC系统上实现优异的性能和可扩展性，是构建大规模并行应用的重要技术基础。