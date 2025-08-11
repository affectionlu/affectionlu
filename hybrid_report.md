# Pthread+MPI混合并行编程完整技术报告

## 1. 混合并行编程模型概述

### 1.1 混合编程模型简介

混合并行编程结合了分布式内存并行（MPI）和共享内存并行（Pthread）的优势，在现代多核集群系统中实现两级并行：
- **节点间并行**：使用MPI进行进程间通信
- **节点内并行**：使用Pthread进行线程级并行

### 1.2 基本架构

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

```c
void check_thread_support() {
    int provided, required;
    
    // 查询当前MPI实现的线程支持
    MPI_Query_thread(&provided);
    printf("Current thread support level: ");
    
    switch (provided) {
        case MPI_THREAD_SINGLE:
            printf("MPI_THREAD_SINGLE - No threading support\n");
            break;
        case MPI_THREAD_FUNNELED:
            printf("MPI_THREAD_FUNNELED - Only main thread can call MPI\n");
            break;
        case MPI_THREAD_SERIALIZED:
            printf("MPI_THREAD_SERIALIZED - Only one thread at a time\n");
            break;
        case MPI_THREAD_MULTIPLE:
            printf("MPI_THREAD_MULTIPLE - Full thread support\n");
            break;
    }
}

// MPI_THREAD_FUNNELED 模式示例
void funneled_mode_example() {
    int rank, size, provided;
    
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    
    if (provided != MPI_THREAD_FUNNELED) {
        printf("Warning: Requested thread level not provided\n");
    }
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // 只有主线程可以调用MPI函数
    // 工作线程不能直接调用MPI
    
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    
    // 线程函数不包含MPI调用
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, computation_only_thread, &i);
    }
    
    // 主线程处理所有MPI通信
    double data[1000];
    if (rank == 0) {
        for (int i = 1; i < size; i++) {
            MPI_Send(data, 1000, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
        }
    } else {
        MPI_Recv(data, 1000, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    
    // 等待计算线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    MPI_Finalize();
}
```

### 2.2 串行化模式（MPI_THREAD_SERIALIZED）

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

void serialized_mode_example() {
    int provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_SERIALIZED, &provided);
    
    if (provided < MPI_THREAD_SERIALIZED) {
        printf("Insufficient threading support\n");
        MPI_Finalize();
        return;
    }
    
    const int NUM_THREADS = 3;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, serialized_worker, &thread_ids[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    MPI_Finalize();
}
```

### 2.3 完全多线程模式（MPI_THREAD_MULTIPLE）

```c
void* multiple_worker(void* arg) {
    int thread_id = *(int*)arg;
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // 线程可以并发调用MPI函数
    double local_data[100];
    for (int i = 0; i < 100; i++) {
        local_data[i] = thread_id * 100 + i;
    }
    
    double global_sum[100];
    
    // 多个线程可以同时调用MPI
    MPI_Allreduce(local_data, global_sum, 100, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    
    printf("Thread %d on rank %d: first global sum = %.2f\n", 
           thread_id, rank, global_sum[0]);
    
    return NULL;
}

void multiple_mode_example() {
    int provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);
    
    if (provided < MPI_THREAD_MULTIPLE) {
        printf("Full threading not available, using level %d\n", provided);
    }
    
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, multiple_worker, &thread_ids[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    MPI_Finalize();
}
```

## 3. 混合编程设计模式

### 3.1 主从模式（Master-Worker）

```c
typedef struct {
    int *work_queue;
    int queue_size;
    int current_index;
    pthread_mutex_t queue_mutex;
    pthread_cond_t work_available;
    bool shutdown;
} work_queue_t;

work_queue_t global_queue;

int get_next_work_item() {
    pthread_mutex_lock(&global_queue.queue_mutex);
    
    while (global_queue.current_index >= global_queue.queue_size && !global_queue.shutdown) {
        pthread_cond_wait(&global_queue.work_available, &global_queue.queue_mutex);
    }
    
    if (global_queue.shutdown) {
        pthread_mutex_unlock(&global_queue.queue_mutex);
        return -1;
    }
    
    int work_item = global_queue.work_queue[global_queue.current_index++];
    pthread_mutex_unlock(&global_queue.queue_mutex);
    
    return work_item;
}

void* master_worker_thread(void* arg) {
    int thread_id = *(int*)arg;
    int work_item;
    
    while ((work_item = get_next_work_item()) != -1) {
        // 处理工作项
        double result = expensive_computation(work_item);
        printf("Thread %d processed work item %d, result: %.6f\n", 
               thread_id, work_item, result);
    }
    
    return NULL;
}

void master_worker_pattern() {
    int rank, size, provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // 初始化工作队列
    global_queue.queue_size = 1000;
    global_queue.work_queue = malloc(global_queue.queue_size * sizeof(int));
    global_queue.current_index = 0;
    global_queue.shutdown = false;
    pthread_mutex_init(&global_queue.queue_mutex, NULL);
    pthread_cond_init(&global_queue.work_available, NULL);
    
    if (rank == 0) {
        // 主进程分发工作
        for (int i = 0; i < global_queue.queue_size; i++) {
            global_queue.work_queue[i] = i;
        }
        
        // 向其他进程发送工作
        for (int proc = 1; proc < size; proc++) {
            int work_batch[100];
            for (int j = 0; j < 100; j++) {
                work_batch[j] = proc * 100 + j;
            }
            MPI_Send(work_batch, 100, MPI_INT, proc, 0, MPI_COMM_WORLD);
        }
    } else {
        // 从主进程接收工作
        int work_batch[100];
        MPI_Recv(work_batch, 100, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        // 更新本地工作队列
        pthread_mutex_lock(&global_queue.queue_mutex);
        for (int i = 0; i < 100; i++) {
            global_queue.work_queue[i] = work_batch[i];
        }
        global_queue.queue_size = 100;
        pthread_cond_broadcast(&global_queue.work_available);
        pthread_mutex_unlock(&global_queue.queue_mutex);
    }
    
    // 创建工作线程
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, master_worker_thread, &thread_ids[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 清理
    free(global_queue.work_queue);
    pthread_mutex_destroy(&global_queue.queue_mutex);
    pthread_cond_destroy(&global_queue.work_available);
    
    MPI_Finalize();
}
```

### 3.2 数据并行模式

```c
typedef struct {
    double *local_data;
    double *global_data;
    int data_size;
    int thread_id;
    int num_threads;
    int mpi_rank;
    int mpi_size;
} data_parallel_context_t;

void* data_parallel_worker(void* arg) {
    data_parallel_context_t* ctx = (data_parallel_context_t*)arg;
    
    // 计算线程处理的数据范围
    int elements_per_thread = ctx->data_size / ctx->num_threads;
    int start_idx = ctx->thread_id * elements_per_thread;
    int end_idx = (ctx->thread_id + 1) * elements_per_thread;
    
    // 执行本地计算
    for (int i = start_idx; i < end_idx; i++) {
        ctx->local_data[i] = sin(ctx->local_data[i]) + cos(ctx->local_data[i]);
    }
    
    printf("Thread %d on rank %d: processed elements %d to %d\n",
           ctx->thread_id, ctx->mpi_rank, start_idx, end_idx - 1);
    
    return NULL;
}

void data_parallel_pattern() {
    int rank, size, provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int TOTAL_DATA_SIZE = 100000;
    const int LOCAL_DATA_SIZE = TOTAL_DATA_SIZE / size;
    const int NUM_THREADS = 4;
    
    double *local_data = malloc(LOCAL_DATA_SIZE * sizeof(double));
    double *global_data = NULL;
    
    if (rank == 0) {
        global_data = malloc(TOTAL_DATA_SIZE * sizeof(double));
        // 初始化全局数据
        for (int i = 0; i < TOTAL_DATA_SIZE; i++) {
            global_data[i] = i * 0.001;
        }
    }
    
    // 分散数据到各进程
    MPI_Scatter(global_data, LOCAL_DATA_SIZE, MPI_DOUBLE,
                local_data, LOCAL_DATA_SIZE, MPI_DOUBLE,
                0, MPI_COMM_WORLD);
    
    // 创建数据并行工作线程
    pthread_t threads[NUM_THREADS];
    data_parallel_context_t contexts[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        contexts[i].local_data = local_data;
        contexts[i].global_data = global_data;
        contexts[i].data_size = LOCAL_DATA_SIZE;
        contexts[i].thread_id = i;
        contexts[i].num_threads = NUM_THREADS;
        contexts[i].mpi_rank = rank;
        contexts[i].mpi_size = size;
        
        pthread_create(&threads[i], NULL, data_parallel_worker, &contexts[i]);
    }
    
    // 等待线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 收集处理后的数据
    MPI_Gather(local_data, LOCAL_DATA_SIZE, MPI_DOUBLE,
               global_data, LOCAL_DATA_SIZE, MPI_DOUBLE,
               0, MPI_COMM_WORLD);
    
    if (rank == 0) {
        printf("Data parallel processing completed\n");
        free(global_data);
    }
    
    free(local_data);
    MPI_Finalize();
}
```

## 4. 通信计算重叠

### 4.1 通信线程分离

```c
typedef struct {
    MPI_Comm comm;
    double *send_buffer;
    double *recv_buffer;
    int buffer_size;
    int partner_rank;
    bool *communication_done;
    pthread_mutex_t *comm_mutex;
} communication_context_t;

void* communication_thread(void* arg) {
    communication_context_t* ctx = (communication_context_t*)arg;
    
    MPI_Request send_req, recv_req;
    
    // 启动非阻塞通信
    MPI_Isend(ctx->send_buffer, ctx->buffer_size, MPI_DOUBLE,
              ctx->partner_rank, 0, ctx->comm, &send_req);
    MPI_Irecv(ctx->recv_buffer, ctx->buffer_size, MPI_DOUBLE,
              ctx->partner_rank, 0, ctx->comm, &recv_req);
    
    // 等待通信完成
    MPI_Wait(&send_req, MPI_STATUS_IGNORE);
    MPI_Wait(&recv_req, MPI_STATUS_IGNORE);
    
    // 通知主线程通信完成
    pthread_mutex_lock(ctx->comm_mutex);
    *(ctx->communication_done) = true;
    pthread_mutex_unlock(ctx->comm_mutex);
    
    return NULL;
}

void* computation_thread(void* arg) {
    double *work_data = (double*)arg;
    
    // 执行计算密集型工作
    for (int i = 0; i < 50000; i++) {
        for (int j = 0; j < 1000; j++) {
            work_data[j] = sin(work_data[j]) * cos(i * 0.001);
        }
    }
    
    return NULL;
}

void communication_computation_overlap() {
    int rank, size, provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);
    
    if (provided < MPI_THREAD_MULTIPLE) {
        printf("Insufficient thread support for overlap\n");
        MPI_Finalize();
        return;
    }
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (size < 2) {
        MPI_Finalize();
        return;
    }
    
    const int BUFFER_SIZE = 10000;
    double *send_buffer = malloc(BUFFER_SIZE * sizeof(double));
    double *recv_buffer = malloc(BUFFER_SIZE * sizeof(double));
    double *work_data = malloc(1000 * sizeof(double));
    
    // 初始化数据
    for (int i = 0; i < BUFFER_SIZE; i++) {
        send_buffer[i] = rank * BUFFER_SIZE + i;
    }
    for (int i = 0; i < 1000; i++) {
        work_data[i] = i * 0.1;
    }
    
    bool communication_done = false;
    pthread_mutex_t comm_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    // 设置通信上下文
    communication_context_t comm_ctx = {
        .comm = MPI_COMM_WORLD,
        .send_buffer = send_buffer,
        .recv_buffer = recv_buffer,
        .buffer_size = BUFFER_SIZE,
        .partner_rank = (rank + 1) % size,
        .communication_done = &communication_done,
        .comm_mutex = &comm_mutex
    };
    
    pthread_t comm_thread, comp_thread;
    
    double start_time = MPI_Wtime();
    
    // 同时启动通信和计算线程
    pthread_create(&comm_thread, NULL, communication_thread, &comm_ctx);
    pthread_create(&comp_thread, NULL, computation_thread, work_data);
    
    // 等待两个线程完成
    pthread_join(comm_thread, NULL);
    pthread_join(comp_thread, NULL);
    
    double end_time = MPI_Wtime();
    
    printf("Rank %d: Overlap completed in %.4f seconds\n", 
           rank, end_time - start_time);
    
    free(send_buffer);
    free(recv_buffer);
    free(work_data);
    pthread_mutex_destroy(&comm_mutex);
    
    MPI_Finalize();
}
```

### 4.2 流水线并行

```c
typedef struct {
    int stage_id;
    int num_stages;
    double *input_buffer;
    double *output_buffer;
    int buffer_size;
    pthread_barrier_t *stage_barrier;
    MPI_Comm comm;
    int rank;
    int size;
} pipeline_stage_t;

void* pipeline_stage_worker(void* arg) {
    pipeline_stage_t* stage = (pipeline_stage_t*)arg;
    
    for (int iteration = 0; iteration < 10; iteration++) {
        // 阶段特定的处理
        switch (stage->stage_id) {
            case 0:  // 数据预处理
                for (int i = 0; i < stage->buffer_size; i++) {
                    stage->output_buffer[i] = stage->input_buffer[i] * 2.0;
                }
                break;
            case 1:  // 主要计算
                for (int i = 0; i < stage->buffer_size; i++) {
                    stage->output_buffer[i] = sin(stage->input_buffer[i]);
                }
                break;
            case 2:  // 后处理
                for (int i = 0; i < stage->buffer_size; i++) {
                    stage->output_buffer[i] = stage->input_buffer[i] + 1.0;
                }
                break;
        }
        
        // 等待所有阶段完成当前迭代
        pthread_barrier_wait(stage->stage_barrier);
        
        // 传递数据到下一个阶段
        if (stage->stage_id < stage->num_stages - 1) {
            // 将输出传递给下一阶段的输入
            memcpy(stage->input_buffer + stage->buffer_size,
                   stage->output_buffer,
                   stage->buffer_size * sizeof(double));
        }
        
        // MPI通信传递到下一个进程
        if (stage->rank < stage->size - 1) {
            MPI_Send(stage->output_buffer, stage->buffer_size, MPI_DOUBLE,
                     stage->rank + 1, iteration, stage->comm);
        }
        
        if (stage->rank > 0) {
            MPI_Recv(stage->input_buffer, stage->buffer_size, MPI_DOUBLE,
                     stage->rank - 1, iteration, stage->comm, MPI_STATUS_IGNORE);
        }
        
        pthread_barrier_wait(stage->stage_barrier);
    }
    
    return NULL;
}

void pipeline_parallel_pattern() {
    int rank, size, provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int NUM_STAGES = 3;
    const int BUFFER_SIZE = 1000;
    
    pthread_barrier_t stage_barrier;
    pthread_barrier_init(&stage_barrier, NULL, NUM_STAGES);
    
    pthread_t stage_threads[NUM_STAGES];
    pipeline_stage_t stages[NUM_STAGES];
    
    double *buffers = malloc(NUM_STAGES * 2 * BUFFER_SIZE * sizeof(double));
    
    for (int i = 0; i < NUM_STAGES; i++) {
        stages[i].stage_id = i;
        stages[i].num_stages = NUM_STAGES;
        stages[i].input_buffer = buffers + i * 2 * BUFFER_SIZE;
        stages[i].output_buffer = buffers + (i * 2 + 1) * BUFFER_SIZE;
        stages[i].buffer_size = BUFFER_SIZE;
        stages[i].stage_barrier = &stage_barrier;
        stages[i].comm = MPI_COMM_WORLD;
        stages[i].rank = rank;
        stages[i].size = size;
        
        // 初始化输入数据
        if (rank == 0 && i == 0) {
            for (int j = 0; j < BUFFER_SIZE; j++) {
                stages[i].input_buffer[j] = j * 0.01;
            }
        }
        
        pthread_create(&stage_threads[i], NULL, pipeline_stage_worker, &stages[i]);
    }
    
    for (int i = 0; i < NUM_STAGES; i++) {
        pthread_join(stage_threads[i], NULL);
    }
    
    printf("Rank %d: Pipeline processing completed\n", rank);
    
    pthread_barrier_destroy(&stage_barrier);
    free(buffers);
    MPI_Finalize();
}
```

## 5. 动态负载均衡

### 5.1 工作窃取算法

```c
typedef struct work_item {
    int task_id;
    double workload;
    struct work_item *next;
} work_item_t;

typedef struct {
    work_item_t *head;
    work_item_t *tail;
    int count;
    pthread_mutex_t mutex;
    int owner_rank;
} work_queue_t;

work_queue_t local_work_queues[MAX_THREADS];
int num_local_threads = 4;

work_item_t* steal_work(int victim_thread) {
    work_item_t *stolen_work = NULL;
    
    pthread_mutex_lock(&local_work_queues[victim_thread].mutex);
    
    if (local_work_queues[victim_thread].count > 1) {
        // 从队列尾部窃取工作
        stolen_work = local_work_queues[victim_thread].tail;
        
        if (stolen_work->next == NULL) {
            // 只有一个元素
            local_work_queues[victim_thread].head = NULL;
            local_work_queues[victim_thread].tail = NULL;
        } else {
            // 找到倒数第二个元素
            work_item_t *prev = local_work_queues[victim_thread].head;
            while (prev->next != stolen_work) {
                prev = prev->next;
            }
            prev->next = NULL;
            local_work_queues[victim_thread].tail = prev;
        }
        
        local_work_queues[victim_thread].count--;
    }
    
    pthread_mutex_unlock(&local_work_queues[victim_thread].mutex);
    
    return stolen_work;
}

void add_work(int thread_id, work_item_t *work) {
    pthread_mutex_lock(&local_work_queues[thread_id].mutex);
    
    work->next = NULL;
    
    if (local_work_queues[thread_id].head == NULL) {
        local_work_queues[thread_id].head = work;
        local_work_queues[thread_id].tail = work;
    } else {
        local_work_queues[thread_id].tail->next = work;
        local_work_queues[thread_id].tail = work;
    }
    
    local_work_queues[thread_id].count++;
    
    pthread_mutex_unlock(&local_work_queues[thread_id].mutex);
}

work_item_t* get_local_work(int thread_id) {
    work_item_t *work = NULL;
    
    pthread_mutex_lock(&local_work_queues[thread_id].mutex);
    
    if (local_work_queues[thread_id].head != NULL) {
        work = local_work_queues[thread_id].head;
        local_work_queues[thread_id].head = work->next;
        
        if (local_work_queues[thread_id].head == NULL) {
            local_work_queues[thread_id].tail = NULL;
        }
        
        local_work_queues[thread_id].count--;
    }
    
    pthread_mutex_unlock(&local_work_queues[thread_id].mutex);
    
    return work;
}

void* work_stealing_thread(void* arg) {
    int thread_id = *(int*)arg;
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    int idle_count = 0;
    const int MAX_IDLE = 100;
    
    while (true) {
        // 尝试获取本地工作
        work_item_t *work = get_local_work(thread_id);
        
        if (work != NULL) {
            // 执行工作
            printf("Thread %d on rank %d: executing task %d (workload %.2f)\n",
                   thread_id, rank, work->task_id, work->workload);
            
            // 模拟工作执行时间
            usleep((int)(work->workload * 1000));
            
            free(work);
            idle_count = 0;
        } else {
            // 本地无工作，尝试窃取
            for (int victim = 0; victim < num_local_threads; victim++) {
                if (victim != thread_id) {
                    work = steal_work(victim);
                    if (work != NULL) {
                        printf("Thread %d stole task %d from thread %d\n",
                               thread_id, work->task_id, victim);
                        
                        // 执行窃取的工作
                        usleep((int)(work->workload * 1000));
                        free(work);
                        idle_count = 0;
                        break;
                    }
                }
            }
            
            if (work == NULL) {
                idle_count++;
                if (idle_count > MAX_IDLE) {
                    // 请求更多工作或退出
                    break;
                }
                usleep(1000);  // 短暂等待
            }
        }
    }
    
    return NULL;
}

void work_stealing_example() {
    int rank, size, provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // 初始化工作队列
    for (int i = 0; i < num_local_threads; i++) {
        local_work_queues[i].head = NULL;
        local_work_queues[i].tail = NULL;
        local_work_queues[i].count = 0;
        local_work_queues[i].owner_rank = rank;
        pthread_mutex_init(&local_work_queues[i].mutex, NULL);
    }
    
    // 创建初始工作
    int num_tasks = 50;
    for (int i = 0; i < num_tasks; i++) {
        work_item_t *work = malloc(sizeof(work_item_t));
        work->task_id = rank * num_tasks + i;
        work->workload = 10.0 + (i % 20);  // 10-30ms的工作
        work->next = NULL;
        
        add_work(i % num_local_threads, work);
    }
    
    // 创建工作线程
    pthread_t threads[num_local_threads];
    int thread_ids[num_local_threads];
    
    for (int i = 0; i < num_local_threads; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, work_stealing_thread, &thread_ids[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < num_local_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 清理
    for (int i = 0; i < num_local_threads; i++) {
        pthread_mutex_destroy(&local_work_queues[i].mutex);
    }
    
    MPI_Finalize();
}
```

### 5.2 自适应负载均衡

```c
typedef struct {
    double computation_time;
    double communication_time;
    int tasks_completed;
    double efficiency;
} performance_metrics_t;

typedef struct {
    int thread_id;
    performance_metrics_t *metrics;
    double *load_factor;
    pthread_mutex_t *metrics_mutex;
    bool *rebalance_needed;
} adaptive_thread_context_t;

void update_performance_metrics(performance_metrics_t *metrics, 
                               double comp_time, double comm_time) {
    metrics->computation_time += comp_time;
    metrics->communication_time += comm_time;
    metrics->tasks_completed++;
    
    // 计算效率：计算时间 / 总时间
    double total_time = metrics->computation_time + metrics->communication_time;
    metrics->efficiency = total_time > 0 ? metrics->computation_time / total_time : 0;
}

double calculate_optimal_load(performance_metrics_t *all_metrics, int num_threads) {
    double total_efficiency = 0.0;
    double max_efficiency = 0.0;
    
    for (int i = 0; i < num_threads; i++) {
        total_efficiency += all_metrics[i].efficiency;
        if (all_metrics[i].efficiency > max_efficiency) {
            max_efficiency = all_metrics[i].efficiency;
        }
    }
    
    double avg_efficiency = total_efficiency / num_threads;
    
    // 如果效率差异超过阈值，需要重新平衡
    return max_efficiency - avg_efficiency;
}

void* adaptive_worker_thread(void* arg) {
    adaptive_thread_context_t *ctx = (adaptive_thread_context_t*)arg;
    
    double start_time, end_time;
    int local_tasks = 100 / 4;  // 初始负载分配
    
    for (int iteration = 0; iteration < 10; iteration++) {
        start_time = MPI_Wtime();
        
        // 执行计算任务
        for (int i = 0; i < local_tasks; i++) {
            double work_intensity = 1.0 + ctx->thread_id * 0.2;
            
            // 模拟不同强度的计算
            for (int j = 0; j < (int)(1000 * work_intensity); j++) {
                volatile double dummy = sin(j * 0.001) * cos(j * 0.001);
            }
        }
        
        end_time = MPI_Wtime();
        double comp_time = end_time - start_time;
        
        // 模拟通信时间
        double comm_time = 0.001 * local_tasks;  // 1ms per task
        
        // 更新性能指标
        pthread_mutex_lock(ctx->metrics_mutex);
        update_performance_metrics(&ctx->metrics[ctx->thread_id], comp_time, comm_time);
        
        // 检查是否需要重新平衡
        if (iteration > 0 && iteration % 3 == 0) {
            double load_imbalance = calculate_optimal_load(ctx->metrics, 4);
            if (load_imbalance > 0.1) {  // 10% 效率差异阈值
                *ctx->rebalance_needed = true;
            }
        }
        
        pthread_mutex_unlock(ctx->metrics_mutex);
        
        // 如果需要重新平衡，调整本地任务数
        if (*ctx->rebalance_needed) {
            pthread_mutex_lock(ctx->metrics_mutex);
            
            double my_efficiency = ctx->metrics[ctx->thread_id].efficiency;
            double avg_efficiency = 0.0;
            for (int i = 0; i < 4; i++) {
                avg_efficiency += ctx->metrics[i].efficiency;
            }
            avg_efficiency /= 4;
            
            if (my_efficiency > avg_efficiency) {
                local_tasks += 5;  // 增加高效线程的负载
            } else if (my_efficiency < avg_efficiency) {
                local_tasks = local_tasks > 5 ? local_tasks - 5 : local_tasks;
            }
            
            *ctx->rebalance_needed = false;
            pthread_mutex_unlock(ctx->metrics_mutex);
            
            printf("Thread %d: Rebalanced to %d tasks (efficiency: %.3f)\n",
                   ctx->thread_id, local_tasks, my_efficiency);
        }
        
        printf("Thread %d: Iteration %d, tasks=%d, comp_time=%.4f, efficiency=%.3f\n",
               ctx->thread_id, iteration, local_tasks, comp_time,
               ctx->metrics[ctx->thread_id].efficiency);
    }
    
    return NULL;
}

void adaptive_load_balancing() {
    int rank, size, provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int NUM_THREADS = 4;
    performance_metrics_t metrics[NUM_THREADS];
    double load_factors[NUM_THREADS];
    pthread_mutex_t metrics_mutex = PTHREAD_MUTEX_INITIALIZER;
    bool rebalance_needed = false;
    
    // 初始化性能指标
    for (int i = 0; i < NUM_THREADS; i++) {
        metrics[i].computation_time = 0.0;
        metrics[i].communication_time = 0.0;
        metrics[i].tasks_completed = 0;
        metrics[i].efficiency = 1.0;
        load_factors[i] = 1.0;
    }
    
    pthread_t threads[NUM_THREADS];
    adaptive_thread_context_t contexts[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        contexts[i].thread_id = i;
        contexts[i].metrics = metrics;
        contexts[i].load_factor = &load_factors[i];
        contexts[i].metrics_mutex = &metrics_mutex;
        contexts[i].rebalance_needed = &rebalance_needed;
        
        pthread_create(&threads[i], NULL, adaptive_worker_thread, &contexts[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 打印最终性能统计
    if (rank == 0) {
        printf("\nFinal Performance Statistics:\n");
        for (int i = 0; i < NUM_THREADS; i++) {
            printf("Thread %d: Tasks=%d, CompTime=%.4f, CommTime=%.4f, Efficiency=%.3f\n",
                   i, metrics[i].tasks_completed, 
                   metrics[i].computation_time, metrics[i].communication_time,
                   metrics[i].efficiency);
        }
    }
    
    pthread_mutex_destroy(&metrics_mutex);
    MPI_Finalize();
}
```

## 6. NUMA优化

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

void numa_aware_hybrid_programming() {
    int rank, size, provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // 检查NUMA支持
    if (numa_available() == -1) {
        printf("NUMA not available on this system\n");
        MPI_Finalize();
        return;
    }
    
    int num_numa_nodes = numa_num_configured_nodes();
    int num_cpus = numa_num_configured_cpus();
    
    printf("Rank %d: NUMA nodes: %d, CPUs: %d\n", rank, num_numa_nodes, num_cpus);
    
    const int NUM_THREADS = 4;
    const size_t DATA_SIZE_PER_THREAD = 100 * 1024 * 1024;  // 100MB per thread
    
    pthread_t threads[NUM_THREADS];
    numa_thread_data_t thread_data[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].numa_node = i % num_numa_nodes;
        thread_data[i].data_size = DATA_SIZE_PER_THREAD;
        thread_data[i].cpu_core = (rank * NUM_THREADS + i) % num_cpus;
        
        pthread_create(&threads[i], NULL, numa_aware_worker, &thread_data[i]);
    }
    
    // 主线程执行MPI通信
    double communication_data[1000];
    for (int i = 0; i < 1000; i++) {
        communication_data[i] = rank * 1000 + i;
    }
    
    // MPI通信示例
    if (rank == 0) {
        for (int target = 1; target < size; target++) {
            MPI_Send(communication_data, 1000, MPI_DOUBLE, target, 0, MPI_COMM_WORLD);
        }
    } else {
        MPI_Recv(communication_data, 1000, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Rank %d received MPI data from rank 0\n", rank);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Rank %d: NUMA-aware hybrid computation completed\n", rank);
    
    MPI_Finalize();
}
```

### 6.2 CPU亲和性设置

```c
void set_thread_affinity(int thread_id, int num_threads_per_node) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    // 获取系统CPU信息
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    int cores_per_thread = num_cpus / num_threads_per_node;
    
    // 为每个线程分配CPU核心
    int start_cpu = thread_id * cores_per_thread;
    int end_cpu = start_cpu + cores_per_thread;
    
    for (int cpu = start_cpu; cpu < end_cpu && cpu < num_cpus; cpu++) {
        CPU_SET(cpu, &cpuset);
    }
    
    int result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (result == 0) {
        printf("Thread %d bound to CPUs %d-%d\n", thread_id, start_cpu, end_cpu - 1);
    } else {
        printf("Failed to set CPU affinity for thread %d\n", thread_id);
    }
}

void* affinity_aware_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    // 设置CPU亲和性
    set_thread_affinity(thread_id, 4);
    
    // 分配大量内存进行计算
    const size_t WORK_SIZE = 50 * 1024 * 1024;  // 50MB
    double *work_array = malloc(WORK_SIZE * sizeof(double));
    
    if (work_array == NULL) {
        printf("Thread %d: Memory allocation failed\n", thread_id);
        return NULL;
    }
    
    // 初始化工作数据
    for (size_t i = 0; i < WORK_SIZE / sizeof(double); i++) {
        work_array[i] = thread_id * i * 0.001;
    }
    
    // 执行计算密集型工作
    double result = 0.0;
    for (int iter = 0; iter < 10; iter++) {
        for (size_t i = 0; i < WORK_SIZE / sizeof(double); i++) {
            work_array[i] = sin(work_array[i]) + cos(work_array[i]);
            result += work_array[i];
        }
    }
    
    printf("Thread %d: Computation result = %.6e\n", thread_id, result);
    
    free(work_array);
    return NULL;
}

void cpu_affinity_example() {
    int rank, size, provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    printf("Rank %d: Starting CPU affinity example\n", rank);
    
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    double start_time = MPI_Wtime();
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, affinity_aware_worker, &thread_ids[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    double end_time = MPI_Wtime();
    
    printf("Rank %d: Affinity-aware computation completed in %.4f seconds\n",
           rank, end_time - start_time);
    
    MPI_Finalize();
}
```

## 7. 性能监控和调优

### 7.1 详细性能测量

```c
typedef struct {
    double total_computation_time;
    double total_communication_time;
    double total_synchronization_time;
    double total_idle_time;
    int num_samples;
    double max_computation_time;
    double min_computation_time;
} detailed_performance_t;

detailed_performance_t thread_performance[MAX_THREADS];
pthread_mutex_t perf_mutex = PTHREAD_MUTEX_INITIALIZER;

void record_performance(int thread_id, double comp_time, double comm_time, 
                        double sync_time, double idle_time) {
    pthread_mutex_lock(&perf_mutex);
    
    detailed_performance_t *perf = &thread_performance[thread_id];
    
    perf->total_computation_time += comp_time;
    perf->total_communication_time += comm_time;
    perf->total_synchronization_time += sync_time;
    perf->total_idle_time += idle_time;
    perf->num_samples++;
    
    if (comp_time > perf->max_computation_time) {
        perf->max_computation_time = comp_time;
    }
    
    if (perf->num_samples == 1 || comp_time < perf->min_computation_time) {
        perf->min_computation_time = comp_time;
    }
    
    pthread_mutex_unlock(&perf_mutex);
}

void* performance_monitoring_worker(void* arg) {
    int thread_id = *(int*)arg;
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // 初始化性能数据
    thread_performance[thread_id].total_computation_time = 0.0;
    thread_performance[thread_id].total_communication_time = 0.0;
    thread_performance[thread_id].total_synchronization_time = 0.0;
    thread_performance[thread_id].total_idle_time = 0.0;
    thread_performance[thread_id].num_samples = 0;
    thread_performance[thread_id].max_computation_time = 0.0;
    thread_performance[thread_id].min_computation_time = 0.0;
    
    for (int iteration = 0; iteration < 20; iteration++) {
        double comp_start = MPI_Wtime();
        
        // 模拟计算工作
        volatile double result = 0.0;
        int work_intensity = 1000 + (thread_id * 200);
        for (int i = 0; i < work_intensity; i++) {
            for (int j = 0; j < 1000; j++) {
                result += sin(i * j * 0.001) * cos(i * j * 0.001);
            }
        }
        
        double comp_end = MPI_Wtime();
        double comp_time = comp_end - comp_start;
        
        // 模拟同步开销
        double sync_start = MPI_Wtime();
        pthread_barrier_wait(&computation_barrier);
        double sync_end = MPI_Wtime();
        double sync_time = sync_end - sync_start;
        
        // 模拟通信开销（只有线程0参与）
        double comm_time = 0.0;
        if (thread_id == 0) {
            double comm_start = MPI_Wtime();
            
            double data[100];
            for (int i = 0; i < 100; i++) {
                data[i] = rank * 100 + i;
            }
            
            double global_sum[100];
            MPI_Allreduce(data, global_sum, 100, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            
            double comm_end = MPI_Wtime();
            comm_time = comm_end - comm_start;
        }
        
        // 模拟空闲时间
        double idle_start = MPI_Wtime();
        usleep(1000 + (thread_id * 500));  // 1-3ms idle time
        double idle_end = MPI_Wtime();
        double idle_time = idle_end - idle_start;
        
        // 记录性能数据
        record_performance(thread_id, comp_time, comm_time, sync_time, idle_time);
        
        if (iteration % 5 == 0) {
            printf("Thread %d, Iteration %d: Comp=%.4f, Comm=%.4f, Sync=%.4f, Idle=%.4f\n",
                   thread_id, iteration, comp_time, comm_time, sync_time, idle_time);
        }
    }
    
    return NULL;
}

void performance_analysis() {
    int rank, size, provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int NUM_THREADS = 4;
    pthread_barrier_init(&computation_barrier, NULL, NUM_THREADS);
    
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    double program_start = MPI_Wtime();
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, performance_monitoring_worker, &thread_ids[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    double program_end = MPI_Wtime();
    double total_program_time = program_end - program_start;
    
    // 分析和报告性能数据
    printf("\n=== Performance Analysis for Rank %d ===\n", rank);
    printf("Total program time: %.4f seconds\n", total_program_time);
    printf("Thread Performance Breakdown:\n");
    printf("TID\tComp(avg)\tComm(avg)\tSync(avg)\tIdle(avg)\tComp(min/max)\tEfficiency\n");
    
    for (int i = 0; i < NUM_THREADS; i++) {
        detailed_performance_t *perf = &thread_performance[i];
        
        double avg_comp = perf->total_computation_time / perf->num_samples;
        double avg_comm = perf->total_communication_time / perf->num_samples;
        double avg_sync = perf->total_synchronization_time / perf->num_samples;
        double avg_idle = perf->total_idle_time / perf->num_samples;
        
        double total_productive = perf->total_computation_time + perf->total_communication_time;
        double total_time = total_productive + perf->total_synchronization_time + perf->total_idle_time;
        double efficiency = total_time > 0 ? total_productive / total_time : 0;
        
        printf("%d\t%.4f\t\t%.4f\t\t%.4f\t\t%.4f\t\t%.4f/%.4f\t%.3f\n",
               i, avg_comp, avg_comm, avg_sync, avg_idle,
               perf->min_computation_time, perf->max_computation_time, efficiency);
    }
    
    // 计算负载均衡指标
    double max_total_comp = 0.0, min_total_comp = DBL_MAX;
    for (int i = 0; i < NUM_THREADS; i++) {
        if (thread_performance[i].total_computation_time > max_total_comp) {
            max_total_comp = thread_performance[i].total_computation_time;
        }
        if (thread_performance[i].total_computation_time < min_total_comp) {
            min_total_comp = thread_performance[i].total_computation_time;
        }
    }
    
    double load_imbalance = max_total_comp > 0 ? (max_total_comp - min_total_comp) / max_total_comp : 0;
    printf("Load imbalance: %.2f%%\n", load_imbalance * 100);
    
    pthread_barrier_destroy(&computation_barrier);
    MPI_Finalize();
}
```

### 7.2 自适应负载均衡

```c
typedef struct {
    double workload_history[10];
    int history_index;
    double predicted_workload;
    bool needs_rebalancing;
} adaptive_scheduler_t;

adaptive_scheduler_t schedulers[MAX_THREADS];

double predict_workload(adaptive_scheduler_t *scheduler) {
    double sum = 0.0;
    int count = 0;
    
    for (int i = 0; i < 10; i++) {
        if (scheduler->workload_history[i] > 0) {
            sum += scheduler->workload_history[i];
            count++;
        }
    }
    
    return count > 0 ? sum / count : 1.0;
}

void update_workload_history(adaptive_scheduler_t *scheduler, double workload) {
    scheduler->workload_history[scheduler->history_index] = workload;
    scheduler->history_index = (scheduler->history_index + 1) % 10;
    scheduler->predicted_workload = predict_workload(scheduler);
}

int calculate_optimal_thread_count(int current_threads, double *predicted_workloads) {
    double total_predicted_work = 0.0;
    for (int i = 0; i < current_threads; i++) {
        total_predicted_work += predicted_workloads[i];
    }
    
    double avg_work_per_thread = total_predicted_work / current_threads;
    double max_work = 0.0;
    
    for (int i = 0; i < current_threads; i++) {
        if (predicted_workloads[i] > max_work) {
            max_work = predicted_workloads[i];
        }
    }
    
    // 如果最大工作负载超过平均值的150%，考虑增加线程
    if (max_work > avg_work_per_thread * 1.5) {
        return current_threads + 1;
    }
    
    // 如果最大工作负载低于平均值的75%，考虑减少线程
    if (max_work < avg_work_per_thread * 0.75 && current_threads > 2) {
        return current_threads - 1;
    }
    
    return current_threads;
}

void* adaptive_load_balancing_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    // 初始化调度器
    schedulers[thread_id].history_index = 0;
    schedulers[thread_id].predicted_workload = 1.0;
    schedulers[thread_id].needs_rebalancing = false;
    for (int i = 0; i < 10; i++) {
        schedulers[thread_id].workload_history[i] = 0.0;
    }
    
    for (int iteration = 0; iteration < 15; iteration++) {
        double iteration_start = MPI_Wtime();
        
        // 基于预测的工作负载调整工作量
        int work_units = (int)(100 * schedulers[thread_id].predicted_workload);
        
        // 执行可变工作负载
        volatile double result = 0.0;
        for (int i = 0; i < work_units; i++) {
            for (int j = 0; j < 1000; j++) {
                result += sin(i * j * 0.001);
            }
        }
        
        double iteration_end = MPI_Wtime();
        double iteration_time = iteration_end - iteration_start;
        
        // 更新工作负载历史
        update_workload_history(&schedulers[thread_id], iteration_time);
        
        printf("Thread %d, Iteration %d: WorkUnits=%d, Time=%.4f, Predicted=%.4f\n",
               thread_id, iteration, work_units, iteration_time, 
               schedulers[thread_id].predicted_workload);
        
        // 每5次迭代检查是否需要重新平衡
        if (iteration > 0 && iteration % 5 == 0) {
            double predicted_workloads[4];
            for (int i = 0; i < 4; i++) {
                predicted_workloads[i] = schedulers[i].predicted_workload;
            }
            
            int optimal_threads = calculate_optimal_thread_count(4, predicted_workloads);
            
            if (optimal_threads != 4) {
                printf("Thread %d suggests optimal thread count: %d\n", 
                       thread_id, optimal_threads);
                
                // 在实际应用中，这里会触发线程池调整
                for (int i = 0; i < 4; i++) {
                    schedulers[i].needs_rebalancing = true;
                }
            }
        }
        
        // 如果需要重新平衡，调整预测工作负载
        if (schedulers[thread_id].needs_rebalancing) {
            double adjustment_factor = 0.9 + (thread_id * 0.05);  // 0.9 to 1.05
            schedulers[thread_id].predicted_workload *= adjustment_factor;
            schedulers[thread_id].needs_rebalancing = false;
            
            printf("Thread %d: Adjusted predicted workload to %.4f\n",
                   thread_id, schedulers[thread_id].predicted_workload);
        }
        
        usleep(50000);  // 50ms between iterations
    }
    
    return NULL;
}

void adaptive_load_balancing_example() {
    int rank, size, provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    printf("Rank %d: Starting adaptive load balancing example\n", rank);
    
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, adaptive_load_balancing_worker, &thread_ids[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 打印最终调度统计
    printf("\nRank %d: Final Adaptive Scheduling Statistics:\n", rank);
    for (int i = 0; i < NUM_THREADS; i++) {
        printf("Thread %d: Final predicted workload = %.4f\n", 
               i, schedulers[i].predicted_workload);
    }
    
    MPI_Finalize();
}
```

## 8. 调试和错误处理

### 8.1 混合程序调试技术

```c
#include <signal.h>
#include <execinfo.h>

void signal_handler(int sig) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    printf("Process %d received signal %d\n", rank, sig);
    
    // 打印栈跟踪
    void *array[10];
    size_t size = backtrace(array, 10);
    char **strings = backtrace_symbols(array, size);
    
    printf("Stack trace for process %d:\n", rank);
    for (size_t i = 0; i < size; i++) {
        printf("  %s\n", strings[i]);
    }
    
    free(strings);
    
    // 清理MPI并终止
    MPI_Abort(MPI_COMM_WORLD, sig);
}

typedef struct {
    int thread_id;
    int error_count;
    char last_error[256];
    pthread_mutex_t error_mutex;
} thread_debug_info_t;

thread_debug_info_t debug_info[MAX_THREADS];

void log_thread_error(int thread_id, const char *error_msg) {
    pthread_mutex_lock(&debug_info[thread_id].error_mutex);
    
    debug_info[thread_id].error_count++;
    strncpy(debug_info[thread_id].last_error, error_msg, 255);
    debug_info[thread_id].last_error[255] = '\0';
    
    printf("Thread %d ERROR #%d: %s\n", 
           thread_id, debug_info[thread_id].error_count, error_msg);
    
    pthread_mutex_unlock(&debug_info[thread_id].error_mutex);
}

void* debuggable_worker(void* arg) {
    int thread_id = *(int*)arg;
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // 初始化调试信息
    debug_info[thread_id].thread_id = thread_id;
    debug_info[thread_id].error_count = 0;
    pthread_mutex_init(&debug_info[thread_id].error_mutex, NULL);
    
    printf("Thread %d on rank %d starting with PID %d, TID %ld\n",
           thread_id, rank, getpid(), pthread_self());
    
    for (int iteration = 0; iteration < 10; iteration++) {
        try_computation: {
            // 模拟可能出错的计算
            if (iteration == 5 && thread_id == 2) {
                log_thread_error(thread_id, "Simulated computation error");
                // 在实际应用中，这里可能会重试或恢复
                goto skip_iteration;
            }
            
            // 正常计算
            volatile double result = 0.0;
            for (int i = 0; i < 1000; i++) {
                result += sin(i * thread_id * 0.001);
            }
            
            printf("Thread %d, iteration %d: result = %.6f\n", 
                   thread_id, iteration, result);
        }
        
        skip_iteration:
        
        // 模拟同步点检查
        if (iteration % 3 == 0) {
            printf("Thread %d: Synchronization checkpoint at iteration %d\n",
                   thread_id, iteration);
        }
        
        usleep(100000);  // 100ms
    }
    
    printf("Thread %d completed successfully with %d errors\n",
           thread_id, debug_info[thread_id].error_count);
    
    pthread_mutex_destroy(&debug_info[thread_id].error_mutex);
    return NULL;
}

void debugging_example() {
    int rank, size, provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // 设置信号处理器
    signal(SIGSEGV, signal_handler);
    signal(SIGFPE, signal_handler);
    signal(SIGABRT, signal_handler);
    
    printf("Rank %d: Debugging example started, PID = %d\n", rank, getpid());
    
    // 如果是rank 0，等待调试器附加
    if (rank == 0 && getenv("WAIT_FOR_DEBUGGER")) {
        printf("Rank 0: Waiting for debugger attachment...\n");
        printf("Attach debugger to PID %d and set variable 'wait=0' to continue\n", getpid());
        
        volatile int wait = 1;
        while (wait) {
            sleep(1);
        }
        printf("Rank 0: Debugger attached, continuing...\n");
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, debuggable_worker, &thread_ids[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 收集和报告调试统计信息
    int total_errors = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        total_errors += debug_info[i].error_count;
    }
    
    printf("Rank %d: Debugging summary - total errors: %d\n", rank, total_errors);
    
    MPI_Finalize();
}
```

### 8.2 异常处理和恢复

```c
typedef enum {
    ERROR_NONE = 0,
    ERROR_COMPUTATION,
    ERROR_COMMUNICATION,
    ERROR_MEMORY,
    ERROR_TIMEOUT
} error_type_t;

typedef struct {
    error_type_t type;
    int severity;  // 1-10
    char description[256];
    double timestamp;
    int recovery_attempts;
} error_record_t;

typedef struct {
    error_record_t errors[100];
    int error_count;
    pthread_mutex_t error_log_mutex;
    bool shutdown_requested;
} error_manager_t;

error_manager_t global_error_manager = {
    .error_count = 0,
    .error_log_mutex = PTHREAD_MUTEX_INITIALIZER,
    .shutdown_requested = false
};

void log_error(error_type_t type, int severity, const char *description) {
    pthread_mutex_lock(&global_error_manager.error_log_mutex);
    
    if (global_error_manager.error_count < 100) {
        error_record_t *error = &global_error_manager.errors[global_error_manager.error_count];
        error->type = type;
        error->severity = severity;
        strncpy(error->description, description, 255);
        error->description[255] = '\0';
        error->timestamp = MPI_Wtime();
        error->recovery_attempts = 0;
        
        global_error_manager.error_count++;
        
        printf("ERROR [%.3f]: Type=%d, Severity=%d, Desc=%s\n",
               error->timestamp, type, severity, description);
        
        // 如果是严重错误，请求关闭
        if (severity >= 8) {
            global_error_manager.shutdown_requested = true;
            printf("CRITICAL ERROR: Shutdown requested\n");
        }
    }
    
    pthread_mutex_unlock(&global_error_manager.error_log_mutex);
}

bool attempt_recovery(error_type_t error_type) {
    switch (error_type) {
        case ERROR_COMPUTATION:
            // 重新初始化计算状态
            printf("Attempting computation recovery...\n");
            usleep(100000);  // 模拟恢复时间
            return true;
            
        case ERROR_COMMUNICATION:
            // 重试通信
            printf("Attempting communication recovery...\n");
            usleep(200000);
            return (rand() % 100) > 30;  // 70% 成功率
            
        case ERROR_MEMORY:
            // 清理和重新分配内存
            printf("Attempting memory recovery...\n");
            usleep(50000);
            return (rand() % 100) > 50;  // 50% 成功率
            
        case ERROR_TIMEOUT:
            // 调整超时设置
            printf("Attempting timeout recovery...\n");
            return true;
            
        default:
            return false;
    }
}

void* fault_tolerant_worker(void* arg) {
    int thread_id = *(int*)arg;
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    printf("Fault-tolerant worker %d starting on rank %d\n", thread_id, rank);
    
    for (int iteration = 0; iteration < 20; iteration++) {
        if (global_error_manager.shutdown_requested) {
            printf("Thread %d: Shutdown requested, exiting\n", thread_id);
            break;
        }
        
        bool operation_successful = false;
        int retry_count = 0;
        const int MAX_RETRIES = 3;
        
        while (!operation_successful && retry_count < MAX_RETRIES) {
            try {
                // 模拟可能失败的操作
                int failure_probability = 15;  // 15% 失败率
                
                if ((rand() % 100) < failure_probability) {
                    // 模拟不同类型的错误
                    error_type_t error_type = (error_type_t)(1 + (rand() % 4));
                    int severity = 1 + (rand() % 10);
                    
                    char error_desc[256];
                    snprintf(error_desc, sizeof(error_desc),
                            "Thread %d operation failed in iteration %d (attempt %d)",
                            thread_id, iteration, retry_count + 1);
                    
                    log_error(error_type, severity, error_desc);
                    
                    // 尝试恢复
                    if (attempt_recovery(error_type)) {
                        printf("Thread %d: Recovery successful\n", thread_id);
                        retry_count++;
                        continue;
                    } else {
                        printf("Thread %d: Recovery failed\n", thread_id);
                        retry_count++;
                        continue;
                    }
                }
                
                // 正常操作
                volatile double result = 0.0;
                for (int i = 0; i < 1000; i++) {
                    result += sin(i * thread_id * 0.001) * cos(iteration * 0.01);
                }
                
                operation_successful = true;
                
                if (iteration % 5 == 0) {
                    printf("Thread %d: Iteration %d completed successfully, result=%.6f\n",
                           thread_id, iteration, result);
                }
                
            } catch (...) {
                log_error(ERROR_COMPUTATION, 7, "Unexpected exception in worker thread");
                retry_count++;
            }
        }
        
        if (!operation_successful) {
            char critical_error[256];
            snprintf(critical_error, sizeof(critical_error),
                    "Thread %d: Failed after %d retry attempts in iteration %d",
                    thread_id, MAX_RETRIES, iteration);
            log_error(ERROR_COMPUTATION, 9, critical_error);
            break;
        }
        
        usleep(50000);  // 50ms between iterations
    }
    
    printf("Thread %d: Exiting after %d iterations\n", thread_id, 20);
    return NULL;
}

void fault_tolerance_example() {
    int rank, size, provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    srand(time(NULL) + rank);  // 不同的随机种子
    
    printf("Rank %d: Starting fault tolerance example\n", rank);
    
    const int NUM_THREADS = 3;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    double start_time = MPI_Wtime();
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, fault_tolerant_worker, &thread_ids[i]);
    }
    
    // 监控线程状态
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    double end_time = MPI_Wtime();
    
    // 生成错误报告
    pthread_mutex_lock(&global_error_manager.error_log_mutex);
    
    printf("\n=== Error Report for Rank %d ===\n", rank);
    printf("Execution time: %.3f seconds\n", end_time - start_time);
    printf("Total errors logged: %d\n", global_error_manager.error_count);
    
    // 按错误类型统计
    int error_type_counts[5] = {0};
    for (int i = 0; i < global_error_manager.error_count; i++) {
        error_type_counts[global_error_manager.errors[i].type]++;
    }
    
    printf("Error breakdown:\n");
    printf("  Computation errors: %d\n", error_type_counts[ERROR_COMPUTATION]);
    printf("  Communication errors: %d\n", error_type_counts[ERROR_COMMUNICATION]);
    printf("  Memory errors: %d\n", error_type_counts[ERROR_MEMORY]);
    printf("  Timeout errors: %d\n", error_type_counts[ERROR_TIMEOUT]);
    
    pthread_mutex_unlock(&global_error_manager.error_log_mutex);
    
    MPI_Finalize();
}
```

## 9. 最佳实践和常见陷阱

### 9.1 最佳实践总结

1. **选择合适的线程安全级别**
   - 根据应用需求选择MPI线程安全级别
   - 避免不必要的同步开销

2. **内存管理优化**
   - 使用NUMA感知的内存分配
   - 避免跨NUMA节点的频繁内存访问

3. **通信计算重叠**
   - 使用非阻塞MPI操作与计算重叠
   - 分离通信和计算线程

4. **负载均衡**
   - 实现动态负载均衡机制
   - 监控和调整工作分配

### 9.2 常见陷阱和解决方案

```c
// 陷阱1：线程安全问题
void thread_safety_pitfall_example() {
    // 错误：在MPI_THREAD_FUNNELED模式下让非主线程调用MPI
    // 正确：确保只有主线程调用MPI函数
    
    int provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided);
    
    if (provided == MPI_THREAD_FUNNELED) {
        // 只有主线程可以调用MPI
        // 工作线程不能直接调用MPI函数
    }
}

// 陷阱2：死锁问题
void deadlock_avoidance_example() {
    // 错误：线程间同步与MPI通信的不当组合可能导致死锁
    // 正确：仔细设计同步模式，避免循环等待
    
    // 使用超时机制避免无限等待
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 5;  // 5秒超时
    
    int result = pthread_mutex_timedlock(&some_mutex, &timeout);
    if (result == ETIMEDOUT) {
        printf("Mutex lock timed out, avoiding potential deadlock\n");
    }
}

// 陷阱3：性能陷阱
void performance_pitfall_example() {
    // 错误：过度同步导致性能下降
    // 正确：最小化同步点，使用无锁数据结构
    
    // 避免频繁的细粒度同步
    // 使用批量操作减少同步开销
}
```

## 10. 总结

混合MPI+Pthread编程提供了强大的两级并行能力，但需要仔细设计以实现最佳性能：

### 10.1 关键要点
- **线程安全**：选择合适的MPI线程安全级别
- **负载均衡**：实现动态负载均衡和工作窃取
- **内存优化**：使用NUMA感知的内存分配
- **通信重叠**：实现计算与通信的重叠
- **错误处理**：建立健壮的错误检测和恢复机制

### 10.2 性能优化策略
- 最小化同步开销
- 优化数据局部性
- 实现自适应调度
- 监控和调优性能瓶颈

### 10.3 调试和维护
- 使用专门的调试工具
- 实现详细的性能监控
- 建立错误日志和恢复机制

通过遵循这些最佳实践，可以开发出高效、可扩展的混合并行应用程序，充分利用现代多核集群系统的计算能力。