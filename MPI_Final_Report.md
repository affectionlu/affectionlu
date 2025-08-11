# MPI并行编程完整技术报告

## 1. MPI概述与基础

### 1.1 MPI简介

消息传递接口（Message Passing Interface, MPI）是用于并行计算的标准化编程接口，支持分布式内存系统上的进程间通信。MPI是高性能计算领域最重要的并行编程模型之一。

### 1.2 基本程序结构

```c
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int rank, size;
    
    // 初始化MPI环境
    MPI_Init(&argc, &argv);
    
    // 获取进程号和总进程数
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    printf("Hello from process %d of %d\n", rank, size);
    
    // 清理MPI环境
    MPI_Finalize();
    return 0;
}
```

### 1.3 编译和运行

```bash
# 编译
mpicc -o hello_mpi hello_mpi.c

# 运行
mpirun -np 4 ./hello_mpi
```

## 2. MPI环境管理

### 2.1 初始化和终止

```c
// 基本初始化
int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    
    // 获取MPI实现信息
    int version, subversion;
    MPI_Get_version(&version, &subversion);
    printf("MPI Version: %d.%d\n", version, subversion);
    
    // 获取处理器名称
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);
    printf("Running on: %s\n", processor_name);
    
    MPI_Finalize();
    return 0;
}

// 线程安全初始化
void thread_safe_init() {
    int provided, required = MPI_THREAD_MULTIPLE;
    
    MPI_Init_thread(NULL, NULL, required, &provided);
    
    switch (provided) {
        case MPI_THREAD_SINGLE:
            printf("Only single-threaded\n");
            break;
        case MPI_THREAD_FUNNELED:
            printf("Only main thread can call MPI\n");
            break;
        case MPI_THREAD_SERIALIZED:
            printf("One thread at a time can call MPI\n");
            break;
        case MPI_THREAD_MULTIPLE:
            printf("Multiple threads can call MPI\n");
            break;
    }
    
    MPI_Finalize();
}
```

### 2.2 错误处理

```c
void error_handling_example() {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // 设置错误处理模式
    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
    
    int data = 42;
    int result = MPI_Send(&data, 1, MPI_INT, 999, 0, MPI_COMM_WORLD);
    
    if (result != MPI_SUCCESS) {
        char error_string[MPI_MAX_ERROR_STRING];
        int length;
        MPI_Error_string(result, error_string, &length);
        printf("Process %d: MPI Error: %s\n", rank, error_string);
    }
}
```

## 3. 点对点通信

### 3.1 阻塞通信

```c
void blocking_send_recv() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (size < 2) {
        printf("This example requires at least 2 processes\n");
        return;
    }
    
    const int ARRAY_SIZE = 10000;
    double *data = malloc(ARRAY_SIZE * sizeof(double));
    
    if (rank == 0) {
        // 进程0发送数据
        for (int i = 0; i < ARRAY_SIZE; i++) {
            data[i] = i * 3.14159;
        }
        
        printf("Process 0: Sending %d elements to process 1\n", ARRAY_SIZE);
        MPI_Send(data, ARRAY_SIZE, MPI_DOUBLE, 1, 0, MPI_COMM_WORLD);
        
    } else if (rank == 1) {
        // 进程1接收数据
        MPI_Status status;
        MPI_Recv(data, ARRAY_SIZE, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, &status);
        
        int count;
        MPI_Get_count(&status, MPI_DOUBLE, &count);
        printf("Process 1: Received %d elements from process %d\n", 
               count, status.MPI_SOURCE);
        
        // 验证数据
        printf("First element: %.5f, Last element: %.5f\n", 
               data[0], data[ARRAY_SIZE-1]);
    }
    
    free(data);
}
```

### 3.2 发送模式

```c
void send_modes_example() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (size < 2) return;
    
    int data[1000];
    for (int i = 0; i < 1000; i++) data[i] = i + rank * 1000;
    
    if (rank == 0) {
        // 标准发送 - 系统决定缓冲
        MPI_Send(data, 1000, MPI_INT, 1, 0, MPI_COMM_WORLD);
        printf("Standard send completed\n");
        
        // 缓冲发送 - 立即返回
        int buffer_size = 1000 * sizeof(int) + MPI_BSEND_OVERHEAD;
        char *buffer = malloc(buffer_size);
        MPI_Buffer_attach(buffer, buffer_size);
        
        MPI_Bsend(data, 1000, MPI_INT, 1, 1, MPI_COMM_WORLD);
        printf("Buffered send completed\n");
        
        MPI_Buffer_detach(&buffer, &buffer_size);
        free(buffer);
        
        // 同步发送 - 等待接收方开始接收
        MPI_Ssend(data, 1000, MPI_INT, 1, 2, MPI_COMM_WORLD);
        printf("Synchronous send completed\n");
        
        // 就绪发送 - 要求接收方已准备好
        MPI_Rsend(data, 1000, MPI_INT, 1, 3, MPI_COMM_WORLD);
        printf("Ready send completed\n");
        
    } else if (rank == 1) {
        MPI_Status status;
        
        // 按顺序接收
        MPI_Recv(data, 1000, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
        printf("Received standard send\n");
        
        MPI_Recv(data, 1000, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
        printf("Received buffered send\n");
        
        MPI_Recv(data, 1000, MPI_INT, 0, 2, MPI_COMM_WORLD, &status);
        printf("Received synchronous send\n");
        
        MPI_Recv(data, 1000, MPI_INT, 0, 3, MPI_COMM_WORLD, &status);
        printf("Received ready send\n");
    }
}
```

### 3.3 非阻塞通信

```c
void non_blocking_communication() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (size < 2) return;
    
    const int COUNT = 50000;
    double *send_data = malloc(COUNT * sizeof(double));
    double *recv_data = malloc(COUNT * sizeof(double));
    
    MPI_Request send_request, recv_request;
    
    if (rank == 0) {
        // 初始化发送数据
        for (int i = 0; i < COUNT; i++) {
            send_data[i] = i * 2.71828;
        }
        
        // 非阻塞发送
        MPI_Isend(send_data, COUNT, MPI_DOUBLE, 1, 0, 
                  MPI_COMM_WORLD, &send_request);
        
        printf("Process 0: Started non-blocking send\n");
        
        // 可以在发送进行时执行其他工作
        double local_sum = 0.0;
        for (int i = 0; i < COUNT; i++) {
            local_sum += send_data[i] * send_data[i];
        }
        printf("Process 0: Local computation result: %.2f\n", local_sum);
        
        // 等待发送完成
        MPI_Status status;
        MPI_Wait(&send_request, &status);
        printf("Process 0: Send completed\n");
        
    } else if (rank == 1) {
        // 非阻塞接收
        MPI_Irecv(recv_data, COUNT, MPI_DOUBLE, 0, 0, 
                  MPI_COMM_WORLD, &recv_request);
        
        printf("Process 1: Started non-blocking receive\n");
        
        // 在接收进行时执行其他工作
        for (int i = 0; i < 100000; i++) {
            double temp = sin(i * 0.001) + cos(i * 0.001);
        }
        printf("Process 1: Computation during receive completed\n");
        
        // 等待接收完成
        MPI_Status status;
        MPI_Wait(&recv_request, &status);
        printf("Process 1: Receive completed\n");
        
        // 验证接收的数据
        printf("Process 1: First received value: %.5f\n", recv_data[0]);
    }
    
    free(send_data);
    free(recv_data);
}
```

### 3.4 多请求管理

```c
void multiple_requests_example() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int NUM_PARTNERS = size - 1;
    const int COUNT = 1000;
    
    double **send_buffers = malloc(NUM_PARTNERS * sizeof(double*));
    double **recv_buffers = malloc(NUM_PARTNERS * sizeof(double*));
    MPI_Request *requests = malloc(2 * NUM_PARTNERS * sizeof(MPI_Request));
    
    // 分配缓冲区
    for (int i = 0; i < NUM_PARTNERS; i++) {
        send_buffers[i] = malloc(COUNT * sizeof(double));
        recv_buffers[i] = malloc(COUNT * sizeof(double));
    }
    
    // 与所有其他进程启动通信
    int req_index = 0;
    for (int partner = 0; partner < size; partner++) {
        if (partner != rank) {
            int buffer_index = (partner > rank) ? partner - 1 : partner;
            
            // 初始化发送数据
            for (int j = 0; j < COUNT; j++) {
                send_buffers[buffer_index][j] = rank * 1000 + j;
            }
            
            // 启动非阻塞发送和接收
            MPI_Isend(send_buffers[buffer_index], COUNT, MPI_DOUBLE, 
                     partner, 0, MPI_COMM_WORLD, &requests[req_index++]);
            MPI_Irecv(recv_buffers[buffer_index], COUNT, MPI_DOUBLE, 
                     partner, 0, MPI_COMM_WORLD, &requests[req_index++]);
        }
    }
    
    printf("Process %d: Started %d communications\n", rank, req_index);
    
    // 等待所有通信完成
    MPI_Status *statuses = malloc(2 * NUM_PARTNERS * sizeof(MPI_Status));
    MPI_Waitall(2 * NUM_PARTNERS, requests, statuses);
    
    printf("Process %d: All communications completed\n", rank);
    
    // 验证接收的数据
    for (int i = 0; i < NUM_PARTNERS; i++) {
        printf("Process %d: Received from buffer %d, first value: %.1f\n", 
               rank, i, recv_buffers[i][0]);
    }
    
    // 清理内存
    for (int i = 0; i < NUM_PARTNERS; i++) {
        free(send_buffers[i]);
        free(recv_buffers[i]);
    }
    free(send_buffers);
    free(recv_buffers);
    free(requests);
    free(statuses);
}
```

## 4. 集合通信

### 4.1 广播操作

```c
void broadcast_examples() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // 标量广播
    int scalar_data;
    if (rank == 0) {
        scalar_data = 42;
        printf("Process 0: Broadcasting scalar value %d\n", scalar_data);
    }
    
    MPI_Bcast(&scalar_data, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    if (rank != 0) {
        printf("Process %d: Received scalar value %d\n", rank, scalar_data);
    }
    
    // 数组广播
    const int ARRAY_SIZE = 10000;
    double *array_data = malloc(ARRAY_SIZE * sizeof(double));
    
    if (rank == 0) {
        for (int i = 0; i < ARRAY_SIZE; i++) {
            array_data[i] = i * 3.14159;
        }
        printf("Process 0: Broadcasting array of %d elements\n", ARRAY_SIZE);
    }
    
    MPI_Bcast(array_data, ARRAY_SIZE, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    if (rank != 0) {
        printf("Process %d: Received array, sum of first 10 elements: %.2f\n", 
               rank, array_data[0] + array_data[9]);
    }
    
    free(array_data);
}
```

### 4.2 散射和收集操作

```c
void scatter_gather_examples() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int ELEMENTS_PER_PROC = 1000;
    const int TOTAL_ELEMENTS = ELEMENTS_PER_PROC * size;
    
    double *send_data = NULL;
    double recv_data[ELEMENTS_PER_PROC];
    double *gathered_data = NULL;
    
    if (rank == 0) {
        // 根进程准备和分配数据
        send_data = malloc(TOTAL_ELEMENTS * sizeof(double));
        gathered_data = malloc(TOTAL_ELEMENTS * sizeof(double));
        
        for (int i = 0; i < TOTAL_ELEMENTS; i++) {
            send_data[i] = i * 1.5;
        }
        printf("Process 0: Prepared %d elements for scattering\n", TOTAL_ELEMENTS);
    }
    
    // 散射数据到所有进程
    MPI_Scatter(send_data, ELEMENTS_PER_PROC, MPI_DOUBLE,
                recv_data, ELEMENTS_PER_PROC, MPI_DOUBLE,
                0, MPI_COMM_WORLD);
    
    printf("Process %d: Received %d elements via scatter\n", rank, ELEMENTS_PER_PROC);
    
    // 每个进程处理自己的数据
    for (int i = 0; i < ELEMENTS_PER_PROC; i++) {
        recv_data[i] *= (rank + 1);  // 乘以进程号+1
    }
    
    printf("Process %d: Processed local data\n", rank);
    
    // 收集处理后的数据
    MPI_Gather(recv_data, ELEMENTS_PER_PROC, MPI_DOUBLE,
               gathered_data, ELEMENTS_PER_PROC, MPI_DOUBLE,
               0, MPI_COMM_WORLD);
    
    if (rank == 0) {
        printf("Process 0: Gathered all processed data\n");
        
        // 验证收集的数据
        double total_sum = 0.0;
        for (int i = 0; i < TOTAL_ELEMENTS; i++) {
            total_sum += gathered_data[i];
        }
        printf("Process 0: Total sum of gathered data: %.2f\n", total_sum);
        
        free(send_data);
        free(gathered_data);
    }
}
```

### 4.3 全收集操作

```c
void allgather_example() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int LOCAL_SIZE = 100;
    const int TOTAL_SIZE = LOCAL_SIZE * size;
    
    double *local_data = malloc(LOCAL_SIZE * sizeof(double));
    double *all_data = malloc(TOTAL_SIZE * sizeof(double));
    
    // 每个进程准备本地数据
    for (int i = 0; i < LOCAL_SIZE; i++) {
        local_data[i] = rank * LOCAL_SIZE + i;
    }
    
    printf("Process %d: Prepared local data\n", rank);
    
    // 全收集操作 - 所有进程都获得所有数据
    MPI_Allgather(local_data, LOCAL_SIZE, MPI_DOUBLE,
                  all_data, LOCAL_SIZE, MPI_DOUBLE,
                  MPI_COMM_WORLD);
    
    printf("Process %d: Completed allgather\n", rank);
    
    // 验证收到的数据
    for (int proc = 0; proc < size; proc++) {
        double expected_first = proc * LOCAL_SIZE;
        double actual_first = all_data[proc * LOCAL_SIZE];
        
        if (actual_first != expected_first) {
            printf("Process %d: Data verification failed for proc %d\n", rank, proc);
        }
    }
    
    printf("Process %d: Data verification passed\n", rank);
    
    free(local_data);
    free(all_data);
}
```

### 4.4 变长数据操作

```c
void variable_length_operations() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // 每个进程有不同数量的数据
    int local_count = (rank + 1) * 100;
    double *local_data = malloc(local_count * sizeof(double));
    
    // 初始化本地数据
    for (int i = 0; i < local_count; i++) {
        local_data[i] = rank * 1000 + i;
    }
    
    // 收集所有进程的数据计数
    int *all_counts = malloc(size * sizeof(int));
    MPI_Allgather(&local_count, 1, MPI_INT, all_counts, 1, MPI_INT, MPI_COMM_WORLD);
    
    // 计算总数据量和位移
    int total_count = 0;
    int *displacements = malloc(size * sizeof(int));
    
    for (int i = 0; i < size; i++) {
        displacements[i] = total_count;
        total_count += all_counts[i];
    }
    
    printf("Process %d: Local count %d, Total count %d\n", 
           rank, local_count, total_count);
    
    // 变长全收集
    double *all_data = malloc(total_count * sizeof(double));
    MPI_Allgatherv(local_data, local_count, MPI_DOUBLE,
                   all_data, all_counts, displacements, MPI_DOUBLE,
                   MPI_COMM_WORLD);
    
    printf("Process %d: Completed variable-length allgather\n", rank);
    
    // 变长散射（只有根进程有完整数据）
    double *scattered_data = malloc(local_count * sizeof(double));
    
    if (rank == 0) {
        // 根进程已有all_data
        MPI_Scatterv(all_data, all_counts, displacements, MPI_DOUBLE,
                     scattered_data, local_count, MPI_DOUBLE,
                     0, MPI_COMM_WORLD);
    } else {
        MPI_Scatterv(NULL, NULL, NULL, MPI_DOUBLE,
                     scattered_data, local_count, MPI_DOUBLE,
                     0, MPI_COMM_WORLD);
    }
    
    printf("Process %d: Completed variable-length scatter\n", rank);
    
    free(local_data);
    free(all_counts);
    free(displacements);
    free(all_data);
    free(scattered_data);
}
```

### 4.5 归约操作

```c
void reduction_operations() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // 标量归约
    double local_value = (rank + 1) * 3.14159;
    double global_sum, global_max, global_min;
    
    printf("Process %d: Local value = %.5f\n", rank, local_value);
    
    // 求和归约
    MPI_Reduce(&local_value, &global_sum, 1, MPI_DOUBLE, MPI_SUM, 
               0, MPI_COMM_WORLD);
    
    // 最大值归约
    MPI_Reduce(&local_value, &global_max, 1, MPI_DOUBLE, MPI_MAX, 
               0, MPI_COMM_WORLD);
    
    // 最小值归约
    MPI_Reduce(&local_value, &global_min, 1, MPI_DOUBLE, MPI_MIN, 
               0, MPI_COMM_WORLD);
    
    if (rank == 0) {
        printf("Global sum: %.5f\n", global_sum);
        printf("Global max: %.5f\n", global_max);
        printf("Global min: %.5f\n", global_min);
    }
    
    // 全归约 - 所有进程都获得结果
    double all_sum, all_product;
    
    MPI_Allreduce(&local_value, &all_sum, 1, MPI_DOUBLE, MPI_SUM, 
                  MPI_COMM_WORLD);
    MPI_Allreduce(&local_value, &all_product, 1, MPI_DOUBLE, MPI_PROD, 
                  MPI_COMM_WORLD);
    
    printf("Process %d: All-reduce sum = %.5f, product = %.5e\n", 
           rank, all_sum, all_product);
    
    // 数组归约
    const int ARRAY_SIZE = 1000;
    double *local_array = malloc(ARRAY_SIZE * sizeof(double));
    double *global_array = malloc(ARRAY_SIZE * sizeof(double));
    
    for (int i = 0; i < ARRAY_SIZE; i++) {
        local_array[i] = rank * i + 1.0;
    }
    
    MPI_Reduce(local_array, global_array, ARRAY_SIZE, MPI_DOUBLE, MPI_SUM, 
               0, MPI_COMM_WORLD);
    
    if (rank == 0) {
        double array_sum = 0.0;
        for (int i = 0; i < ARRAY_SIZE; i++) {
            array_sum += global_array[i];
        }
        printf("Sum of all array elements: %.2f\n", array_sum);
    }
    
    free(local_array);
    free(global_array);
}
```

### 4.6 自定义归约操作

```c
// 自定义归约函数：找到最大值及其位置
void max_location_func(void *in, void *inout, int *len, MPI_Datatype *datatype) {
    typedef struct {
        double value;
        int rank;
    } MaxLocation;
    
    MaxLocation *in_data = (MaxLocation *)in;
    MaxLocation *inout_data = (MaxLocation *)inout;
    
    for (int i = 0; i < *len; i++) {
        if (in_data[i].value > inout_data[i].value) {
            inout_data[i] = in_data[i];
        }
    }
}

void custom_reduction_example() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    typedef struct {
        double value;
        int rank;
    } MaxLocation;
    
    // 创建自定义数据类型
    MPI_Datatype maxloc_type;
    int block_lengths[2] = {1, 1};
    MPI_Aint displacements[2];
    MPI_Datatype types[2] = {MPI_DOUBLE, MPI_INT};
    
    MaxLocation dummy;
    MPI_Aint base_addr;
    MPI_Get_address(&dummy, &base_addr);
    MPI_Get_address(&dummy.value, &displacements[0]);
    MPI_Get_address(&dummy.rank, &displacements[1]);
    displacements[0] -= base_addr;
    displacements[1] -= base_addr;
    
    MPI_Type_create_struct(2, block_lengths, displacements, types, &maxloc_type);
    MPI_Type_commit(&maxloc_type);
    
    // 创建自定义归约操作
    MPI_Op max_loc_op;
    MPI_Op_create(max_location_func, 1, &max_loc_op);
    
    // 本地数据
    MaxLocation local_data;
    local_data.value = (rank + 1) * (rank + 1) * 2.5;
    local_data.rank = rank;
    
    printf("Process %d: Local value = %.2f\n", rank, local_data.value);
    
    MaxLocation global_max;
    
    // 执行自定义归约
    MPI_Reduce(&local_data, &global_max, 1, maxloc_type, max_loc_op, 
               0, MPI_COMM_WORLD);
    
    if (rank == 0) {
        printf("Global maximum: %.2f found at process %d\n", 
               global_max.value, global_max.rank);
    }
    
    // 清理
    MPI_Op_free(&max_loc_op);
    MPI_Type_free(&maxloc_type);
}
```

## 5. 数据类型

### 5.1 基本数据类型

```c
void basic_datatypes_example() {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    if (rank == 0) {
        // 显示MPI基本数据类型大小
        printf("MPI Data Type Sizes:\n");
        printf("MPI_INT: %lu bytes\n", sizeof(int));
        printf("MPI_FLOAT: %lu bytes\n", sizeof(float));
        printf("MPI_DOUBLE: %lu bytes\n", sizeof(double));
        printf("MPI_CHAR: %lu bytes\n", sizeof(char));
        printf("MPI_LONG: %lu bytes\n", sizeof(long));
        printf("MPI_LONG_LONG: %lu bytes\n", sizeof(long long));
        
        // 发送不同类型的数据
        int int_data = 42;
        float float_data = 3.14f;
        double double_data = 2.71828;
        char char_data = 'A';
        
        MPI_Send(&int_data, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
        MPI_Send(&float_data, 1, MPI_FLOAT, 1, 1, MPI_COMM_WORLD);
        MPI_Send(&double_data, 1, MPI_DOUBLE, 1, 2, MPI_COMM_WORLD);
        MPI_Send(&char_data, 1, MPI_CHAR, 1, 3, MPI_COMM_WORLD);
        
    } else if (rank == 1) {
        int int_data;
        float float_data;
        double double_data;
        char char_data;
        
        MPI_Recv(&int_data, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&float_data, 1, MPI_FLOAT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&double_data, 1, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&char_data, 1, MPI_CHAR, 0, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        printf("Process 1 received:\n");
        printf("  int: %d\n", int_data);
        printf("  float: %.2f\n", float_data);
        printf("  double: %.5f\n", double_data);
        printf("  char: %c\n", char_data);
    }
}
```

### 5.2 派生数据类型

```c
void derived_datatypes_example() {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // 连续数据类型
    MPI_Datatype three_ints;
    MPI_Type_contiguous(3, MPI_INT, &three_ints);
    MPI_Type_commit(&three_ints);
    
    // 向量数据类型 - 处理数组的跨步访问
    MPI_Datatype vector_type;
    MPI_Type_vector(5,      // 块数量
                   2,      // 每块元素数
                   4,      // 块间跨步
                   MPI_INT, &vector_type);
    MPI_Type_commit(&vector_type);
    
    // 索引数据类型 - 不规则位移
    int block_lengths[] = {2, 3, 1};
    int displacements[] = {0, 3, 8};
    MPI_Datatype indexed_type;
    MPI_Type_indexed(3, block_lengths, displacements, MPI_INT, &indexed_type);
    MPI_Type_commit(&indexed_type);
    
    if (rank == 0) {
        int data[20];
        for (int i = 0; i < 20; i++) data[i] = i;
        
        // 发送连续数据
        MPI_Send(data, 1, three_ints, 1, 0, MPI_COMM_WORLD);
        
        // 发送向量数据
        MPI_Send(data, 1, vector_type, 1, 1, MPI_COMM_WORLD);
        
        // 发送索引数据
        MPI_Send(data, 1, indexed_type, 1, 2, MPI_COMM_WORLD);
        
    } else if (rank == 1) {
        int recv_data[20];
        
        MPI_Recv(recv_data, 1, three_ints, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Contiguous data: %d %d %d\n", recv_data[0], recv_data[1], recv_data[2]);
        
        memset(recv_data, 0, sizeof(recv_data));
        MPI_Recv(recv_data, 1, vector_type, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Vector data: ");
        for (int i = 0; i < 20; i++) {
            if (recv_data[i] != 0) printf("%d ", recv_data[i]);
        }
        printf("\n");
        
        memset(recv_data, 0, sizeof(recv_data));
        MPI_Recv(recv_data, 1, indexed_type, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Indexed data: ");
        for (int i = 0; i < 20; i++) {
            if (recv_data[i] != 0) printf("%d ", recv_data[i]);
        }
        printf("\n");
    }
    
    // 清理数据类型
    MPI_Type_free(&three_ints);
    MPI_Type_free(&vector_type);
    MPI_Type_free(&indexed_type);
}
```

### 5.3 结构体数据类型

```c
void struct_datatype_example() {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    typedef struct {
        int id;
        double coordinates[3];
        char name[16];
        float mass;
    } Particle;
    
    // 创建结构体数据类型
    MPI_Datatype particle_type;
    int block_lengths[4] = {1, 3, 16, 1};
    MPI_Aint displacements[4];
    MPI_Datatype types[4] = {MPI_INT, MPI_DOUBLE, MPI_CHAR, MPI_FLOAT};
    
    // 计算结构体成员的位移
    Particle dummy_particle;
    MPI_Aint base_address;
    MPI_Get_address(&dummy_particle, &base_address);
    MPI_Get_address(&dummy_particle.id, &displacements[0]);
    MPI_Get_address(&dummy_particle.coordinates, &displacements[1]);
    MPI_Get_address(&dummy_particle.name, &displacements[2]);
    MPI_Get_address(&dummy_particle.mass, &displacements[3]);
    
    // 转换为相对位移
    for (int i = 0; i < 4; i++) {
        displacements[i] -= base_address;
    }
    
    MPI_Type_create_struct(4, block_lengths, displacements, types, &particle_type);
    MPI_Type_commit(&particle_type);
    
    if (rank == 0) {
        Particle particles[5];
        
        // 初始化粒子数据
        for (int i = 0; i < 5; i++) {
            particles[i].id = i;
            particles[i].coordinates[0] = i * 1.0;
            particles[i].coordinates[1] = i * 2.0;
            particles[i].coordinates[2] = i * 3.0;
            snprintf(particles[i].name, 16, "Particle_%d", i);
            particles[i].mass = (i + 1) * 10.5f;
        }
        
        printf("Process 0: Sending particle array\n");
        MPI_Send(particles, 5, particle_type, 1, 0, MPI_COMM_WORLD);
        
    } else if (rank == 1) {
        Particle particles[5];
        
        MPI_Recv(particles, 5, particle_type, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        printf("Process 1: Received particles:\n");
        for (int i = 0; i < 5; i++) {
            printf("  ID: %d, Name: %s, Mass: %.1f, Pos: (%.1f, %.1f, %.1f)\n",
                   particles[i].id, particles[i].name, particles[i].mass,
                   particles[i].coordinates[0], particles[i].coordinates[1], 
                   particles[i].coordinates[2]);
        }
    }
    
    MPI_Type_free(&particle_type);
}
```

## 6. 通信器和组管理

### 6.1 通信器操作

```c
void communicator_operations() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // 复制通信器
    MPI_Comm dup_comm;
    MPI_Comm_dup(MPI_COMM_WORLD, &dup_comm);
    
    int dup_rank, dup_size;
    MPI_Comm_rank(dup_comm, &dup_rank);
    MPI_Comm_size(dup_comm, &dup_size);
    
    printf("Process %d: Duplicated communicator rank=%d, size=%d\n", 
           rank, dup_rank, dup_size);
    
    // 分割通信器
    int color = rank % 2;  // 按奇偶分组
    int key = rank;        // 新通信器内的排序键
    
    MPI_Comm split_comm;
    MPI_Comm_split(MPI_COMM_WORLD, color, key, &split_comm);
    
    int split_rank, split_size;
    MPI_Comm_rank(split_comm, &split_rank);
    MPI_Comm_size(split_comm, &split_size);
    
    printf("Process %d: Split communicator (color=%d) rank=%d, size=%d\n",
           rank, color, split_rank, split_size);
    
    // 在子通信器中进行集合操作
    int local_data = rank;
    int sum_result;
    
    MPI_Allreduce(&local_data, &sum_result, 1, MPI_INT, MPI_SUM, split_comm);
    printf("Process %d: Sum in split communicator = %d\n", rank, sum_result);
    
    // 通信器比较
    int compare_result;
    MPI_Comm_compare(MPI_COMM_WORLD, dup_comm, &compare_result);
    
    if (rank == 0) {
        switch (compare_result) {
            case MPI_IDENT:
                printf("Communicators are identical\n");
                break;
            case MPI_CONGRUENT:
                printf("Communicators are congruent\n");
                break;
            case MPI_SIMILAR:
                printf("Communicators are similar\n");
                break;
            case MPI_UNEQUAL:
                printf("Communicators are unequal\n");
                break;
        }
    }
    
    // 清理通信器
    MPI_Comm_free(&dup_comm);
    MPI_Comm_free(&split_comm);
}
```

### 6.2 组操作

```c
void group_operations() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    MPI_Group world_group, subset_group, union_group;
    MPI_Comm subset_comm;
    
    // 获取MPI_COMM_WORLD的组
    MPI_Comm_group(MPI_COMM_WORLD, &world_group);
    
    // 创建子集组（选择偶数rank的进程）
    int num_even = (size + 1) / 2;
    int *even_ranks = malloc(num_even * sizeof(int));
    
    int even_count = 0;
    for (int i = 0; i < size; i += 2) {
        even_ranks[even_count++] = i;
    }
    
    MPI_Group_incl(world_group, even_count, even_ranks, &subset_group);
    
    // 从组创建通信器
    MPI_Comm_create(MPI_COMM_WORLD, subset_group, &subset_comm);
    
    // 检查当前进程是否在新通信器中
    if (subset_comm != MPI_COMM_NULL) {
        int subset_rank, subset_size;
        MPI_Comm_rank(subset_comm, &subset_rank);
        MPI_Comm_size(subset_comm, &subset_size);
        
        printf("Process %d: In subset communicator with rank=%d, size=%d\n",
               rank, subset_rank, subset_size);
        
        // 在子集中进行通信
        if (subset_rank == 0) {
            int message = 12345;
            for (int i = 1; i < subset_size; i++) {
                MPI_Send(&message, 1, MPI_INT, i, 0, subset_comm);
            }
        } else {
            int received_message;
            MPI_Recv(&received_message, 1, MPI_INT, 0, 0, subset_comm, MPI_STATUS_IGNORE);
            printf("Process %d: Received message %d in subset\n", rank, received_message);
        }
        
        MPI_Comm_free(&subset_comm);
    } else {
        printf("Process %d: Not in subset communicator\n", rank);
    }
    
    // 组的并集操作
    int odd_count = size / 2;
    int *odd_ranks = malloc(odd_count * sizeof(int));
    
    int odd_index = 0;
    for (int i = 1; i < size; i += 2) {
        odd_ranks[odd_index++] = i;
    }
    
    MPI_Group odd_group;
    if (odd_count > 0) {
        MPI_Group_incl(world_group, odd_count, odd_ranks, &odd_group);
        MPI_Group_union(subset_group, odd_group, &union_group);
        
        int union_size;
        MPI_Group_size(union_group, &union_size);
        
        if (rank == 0) {
            printf("Union group size: %d\n", union_size);
        }
        
        MPI_Group_free(&odd_group);
        MPI_Group_free(&union_group);
    }
    
    // 清理
    free(even_ranks);
    free(odd_ranks);
    MPI_Group_free(&world_group);
    MPI_Group_free(&subset_group);
}
```

## 7. 并行I/O

### 7.1 基本文件操作

```c
void basic_parallel_io() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int ELEMENTS_PER_PROC = 1000;
    double *data = malloc(ELEMENTS_PER_PROC * sizeof(double));
    
    // 每个进程准备不同的数据
    for (int i = 0; i < ELEMENTS_PER_PROC; i++) {
        data[i] = rank * ELEMENTS_PER_PROC + i;
    }
    
    // 并行写文件
    MPI_File write_file;
    MPI_File_open(MPI_COMM_WORLD, "parallel_output.dat",
                  MPI_MODE_CREATE | MPI_MODE_WRONLY,
                  MPI_INFO_NULL, &write_file);
    
    // 计算每个进程的文件偏移
    MPI_Offset offset = rank * ELEMENTS_PER_PROC * sizeof(double);
    
    // 写入数据
    MPI_Status write_status;
    MPI_File_write_at(write_file, offset, data, ELEMENTS_PER_PROC, 
                      MPI_DOUBLE, &write_status);
    
    int write_count;
    MPI_Get_count(&write_status, MPI_DOUBLE, &write_count);
    printf("Process %d: Wrote %d elements at offset %lld\n", 
           rank, write_count, (long long)offset);
    
    MPI_File_close(&write_file);
    
    // 同步所有进程
    MPI_Barrier(MPI_COMM_WORLD);
    
    // 并行读文件验证
    double *read_data = malloc(ELEMENTS_PER_PROC * sizeof(double));
    
    MPI_File read_file;
    MPI_File_open(MPI_COMM_WORLD, "parallel_output.dat",
                  MPI_MODE_RDONLY, MPI_INFO_NULL, &read_file);
    
    MPI_Status read_status;
    MPI_File_read_at(read_file, offset, read_data, ELEMENTS_PER_PROC,
                     MPI_DOUBLE, &read_status);
    
    int read_count;
    MPI_Get_count(&read_status, MPI_DOUBLE, &read_count);
    printf("Process %d: Read %d elements, first=%.1f, last=%.1f\n",
           rank, read_count, read_data[0], read_data[read_count-1]);
    
    MPI_File_close(&read_file);
    
    free(data);
    free(read_data);
}
```

### 7.2 集合I/O操作

```c
void collective_io_operations() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int LOCAL_SIZE = 500;
    int *local_data = malloc(LOCAL_SIZE * sizeof(int));
    
    // 准备本地数据
    for (int i = 0; i < LOCAL_SIZE; i++) {
        local_data[i] = rank * 1000 + i;
    }
    
    // 集合写操作
    MPI_File file;
    MPI_File_open(MPI_COMM_WORLD, "collective_output.dat",
                  MPI_MODE_CREATE | MPI_MODE_WRONLY,
                  MPI_INFO_NULL, &file);
    
    // 有序写入 - 进程按rank顺序写入
    MPI_Status status;
    MPI_File_write_ordered(file, local_data, LOCAL_SIZE, MPI_INT, &status);
    
    int write_count;
    MPI_Get_count(&status, MPI_INT, &write_count);
    printf("Process %d: Collective write completed, wrote %d elements\n",
           rank, write_count);
    
    MPI_File_close(&file);
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // 集合读操作
    int *read_data = malloc(LOCAL_SIZE * sizeof(int));
    
    MPI_File_open(MPI_COMM_WORLD, "collective_output.dat",
                  MPI_MODE_RDONLY, MPI_INFO_NULL, &file);
    
    MPI_File_read_ordered(file, read_data, LOCAL_SIZE, MPI_INT, &status);
    
    int read_count;
    MPI_Get_count(&status, MPI_INT, &read_count);
    
    // 验证数据
    bool data_correct = true;
    for (int i = 0; i < LOCAL_SIZE; i++) {
        if (read_data[i] != rank * 1000 + i) {
            data_correct = false;
            break;
        }
    }
    
    printf("Process %d: Collective read completed, data %s\n",
           rank, data_correct ? "correct" : "incorrect");
    
    MPI_File_close(&file);
    
    free(local_data);
    free(read_data);
}
```

### 7.3 文件视图操作

```c
void file_view_operations() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int ARRAY_SIZE = 20;
    int *global_array = malloc(ARRAY_SIZE * sizeof(int));
    
    if (rank == 0) {
        // 进程0初始化全局数组
        for (int i = 0; i < ARRAY_SIZE; i++) {
            global_array[i] = i;
        }
        
        // 写入完整数组
        MPI_File file;
        MPI_File_open(MPI_COMM_SELF, "array_data.dat",
                      MPI_MODE_CREATE | MPI_MODE_WRONLY,
                      MPI_INFO_NULL, &file);
        
        MPI_File_write(file, global_array, ARRAY_SIZE, MPI_INT, MPI_STATUS_IGNORE);
        MPI_File_close(&file);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // 所有进程使用文件视图读取特定部分
    int elements_per_proc = ARRAY_SIZE / size;
    int *local_data = malloc(elements_per_proc * sizeof(int));
    
    MPI_File file;
    MPI_File_open(MPI_COMM_WORLD, "array_data.dat",
                  MPI_MODE_RDONLY, MPI_INFO_NULL, &file);
    
    // 创建文件视图 - 每个进程看到文件的不同部分
    MPI_Datatype filetype;
    MPI_Type_contiguous(elements_per_proc, MPI_INT, &filetype);
    MPI_Type_commit(&filetype);
    
    MPI_Offset disp = rank * elements_per_proc * sizeof(int);
    MPI_File_set_view(file, disp, MPI_INT, filetype, "native", MPI_INFO_NULL);
    
    // 使用文件视图读取数据
    MPI_File_read(file, local_data, elements_per_proc, MPI_INT, MPI_STATUS_IGNORE);
    
    printf("Process %d: Read elements ", rank);
    for (int i = 0; i < elements_per_proc; i++) {
        printf("%d ", local_data[i]);
    }
    printf("\n");
    
    MPI_File_close(&file);
    MPI_Type_free(&filetype);
    
    free(global_array);
    free(local_data);
}
```

## 8. 单边通信（RMA）

### 8.1 基本远程内存访问

```c
void basic_rma_operations() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (size < 2) {
        printf("RMA example requires at least 2 processes\n");
        return;
    }
    
    const int WIN_SIZE = 1000;
    int *win_buffer;
    MPI_Win window;
    
    // 分配并创建窗口
    MPI_Win_allocate(WIN_SIZE * sizeof(int), sizeof(int), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &win_buffer, &window);
    
    // 初始化窗口数据
    for (int i = 0; i < WIN_SIZE; i++) {
        win_buffer[i] = rank * 1000 + i;
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    if (rank == 0) {
        int *get_data = malloc(100 * sizeof(int));
        int *put_data = malloc(100 * sizeof(int));
        
        // 初始化要写入的数据
        for (int i = 0; i < 100; i++) {
            put_data[i] = 9000 + i;
        }
        
        // 开始RMA访问纪元
        MPI_Win_fence(0, window);
        
        // 从进程1读取数据
        MPI_Get(get_data, 100, MPI_INT, 1, 0, 100, MPI_INT, window);
        
        // 向进程1写入数据
        MPI_Put(put_data, 100, MPI_INT, 1, 100, 100, MPI_INT, window);
        
        // 结束RMA访问纪元
        MPI_Win_fence(0, window);
        
        printf("Process 0: RMA operations completed\n");
        printf("Process 0: Read from process 1: first=%d, last=%d\n",
               get_data[0], get_data[99]);
        
        free(get_data);
        free(put_data);
        
    } else if (rank == 1) {
        // 进程1参与fence同步
        MPI_Win_fence(0, window);
        MPI_Win_fence(0, window);
        
        printf("Process 1: Window after RMA operations:\n");
        printf("Process 1: Elements 0-9: ");
        for (int i = 0; i < 10; i++) {
            printf("%d ", win_buffer[i]);
        }
        printf("\n");
        
        printf("Process 1: Elements 100-109: ");
        for (int i = 100; i < 110; i++) {
            printf("%d ", win_buffer[i]);
        }
        printf("\n");
        
    } else {
        // 其他进程参与fence同步
        MPI_Win_fence(0, window);
        MPI_Win_fence(0, window);
    }
    
    MPI_Win_free(&window);
}
```

### 8.2 累积操作

```c
void accumulate_operations() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int WIN_SIZE = 10;
    int *win_buffer;
    MPI_Win window;
    
    // 创建窗口
    MPI_Win_allocate(WIN_SIZE * sizeof(int), sizeof(int), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &win_buffer, &window);
    
    // 初始化为0
    for (int i = 0; i < WIN_SIZE; i++) {
        win_buffer[i] = 0;
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // 所有进程向进程0的窗口累积值
    int contribution = rank + 1;
    
    MPI_Win_fence(0, window);
    
    // 累积到进程0的不同位置
    MPI_Accumulate(&contribution, 1, MPI_INT, 0, rank % WIN_SIZE, 1, MPI_INT, 
                   MPI_SUM, window);
    
    MPI_Win_fence(0, window);
    
    if (rank == 0) {
        printf("Process 0: Window after accumulation:\n");
        for (int i = 0; i < WIN_SIZE; i++) {
            printf("  Position %d: %d\n", i, win_buffer[i]);
        }
        
        // 验证结果
        int total_contributions = 0;
        for (int i = 0; i < size; i++) {
            total_contributions += (i + 1);
        }
        
        int window_sum = 0;
        for (int i = 0; i < WIN_SIZE; i++) {
            window_sum += win_buffer[i];
        }
        
        printf("Process 0: Expected total: %d, Actual total: %d\n",
               total_contributions, window_sum);
    }
    
    MPI_Win_free(&window);
}
```

### 8.3 主动目标同步

```c
void active_target_synchronization() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (size < 2) return;
    
    const int WIN_SIZE = 100;
    double *win_buffer;
    MPI_Win window;
    
    MPI_Win_allocate(WIN_SIZE * sizeof(double), sizeof(double), MPI_INFO_NULL,
                     MPI_COMM_WORLD, &win_buffer, &window);
    
    for (int i = 0; i < WIN_SIZE; i++) {
        win_buffer[i] = rank * 100.0 + i;
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    if (rank == 0) {
        double *fetch_data = malloc(50 * sizeof(double));
        double *update_data = malloc(50 * sizeof(double));
        
        for (int i = 0; i < 50; i++) {
            update_data[i] = 1000.0 + i;
        }
        
        // 主动目标同步 - 访问进程1
        MPI_Win_start(MPI_GROUP_EMPTY, 0, window);
        
        // 获取-更新操作
        MPI_Get_accumulate(update_data, 50, MPI_DOUBLE,
                          fetch_data, 50, MPI_DOUBLE,
                          1, 0, 50, MPI_DOUBLE, MPI_REPLACE, window);
        
        MPI_Win_complete(window);
        
        printf("Process 0: Fetched data from process 1: first=%.1f, last=%.1f\n",
               fetch_data[0], fetch_data[49]);
        
        free(fetch_data);
        free(update_data);
        
    } else if (rank == 1) {
        MPI_Group world_group, origin_group;
        
        MPI_Comm_group(MPI_COMM_WORLD, &world_group);
        
        int origin_rank = 0;
        MPI_Group_incl(world_group, 1, &origin_rank, &origin_group);
        
        // 暴露窗口给进程0
        MPI_Win_post(origin_group, 0, window);
        MPI_Win_wait(window);
        
        printf("Process 1: Window updated by process 0\n");
        printf("Process 1: New values: first=%.1f, last=%.1f\n",
               win_buffer[0], win_buffer[49]);
        
        MPI_Group_free(&world_group);
        MPI_Group_free(&origin_group);
    }
    
    MPI_Win_free(&window);
}
```

## 9. 非阻塞集合通信

### 9.1 非阻塞集合操作

```c
void non_blocking_collective_operations() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int COUNT = 10000;
    double *local_data = malloc(COUNT * sizeof(double));
    double *result_data = malloc(COUNT * sizeof(double));
    
    // 初始化本地数据
    for (int i = 0; i < COUNT; i++) {
        local_data[i] = rank * COUNT + i;
    }
    
    // 非阻塞全归约
    MPI_Request allreduce_request;
    MPI_Iallreduce(local_data, result_data, COUNT, MPI_DOUBLE, MPI_SUM,
                   MPI_COMM_WORLD, &allreduce_request);
    
    printf("Process %d: Started non-blocking allreduce\n", rank);
    
    // 在全归约进行时执行其他计算
    double local_computation_result = 0.0;
    for (int i = 0; i < 100000; i++) {
        local_computation_result += sin(i * 0.001) * cos(i * 0.001);
    }
    
    printf("Process %d: Local computation during allreduce: %.6f\n", 
           rank, local_computation_result);
    
    // 等待全归约完成
    MPI_Status status;
    MPI_Wait(&allreduce_request, &status);
    
    printf("Process %d: Non-blocking allreduce completed\n", rank);
    printf("Process %d: Global sum first element: %.1f\n", rank, result_data[0]);
    
    // 非阻塞广播
    double *broadcast_data = malloc(COUNT * sizeof(double));
    
    if (rank == 0) {
        for (int i = 0; i < COUNT; i++) {
            broadcast_data[i] = i * 3.14159;
        }
    }
    
    MPI_Request bcast_request;
    MPI_Ibcast(broadcast_data, COUNT, MPI_DOUBLE, 0, MPI_COMM_WORLD, &bcast_request);
    
    printf("Process %d: Started non-blocking broadcast\n", rank);
    
    // 执行其他工作
    usleep(100000);  // 模拟其他工作
    
    MPI_Wait(&bcast_request, &status);
    
    if (rank != 0) {
        printf("Process %d: Received broadcast, first value: %.5f\n", 
               rank, broadcast_data[0]);
    }
    
    free(local_data);
    free(result_data);
    free(broadcast_data);
}
```

### 9.2 多个非阻塞操作

```c
void multiple_non_blocking_operations() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    const int COUNT = 1000;
    
    // 准备多个数据集
    double *data1 = malloc(COUNT * sizeof(double));
    double *data2 = malloc(COUNT * sizeof(double));
    double *result1 = malloc(COUNT * sizeof(double));
    double *result2 = malloc(COUNT * sizeof(double));
    double *broadcast_data = malloc(COUNT * sizeof(double));
    
    for (int i = 0; i < COUNT; i++) {
        data1[i] = rank * COUNT + i;
        data2[i] = (rank + 1) * COUNT + i;
    }
    
    if (rank == 0) {
        for (int i = 0; i < COUNT; i++) {
            broadcast_data[i] = i * 2.71828;
        }
    }
    
    // 启动多个非阻塞集合操作
    MPI_Request requests[3];
    
    MPI_Iallreduce(data1, result1, COUNT, MPI_DOUBLE, MPI_SUM, 
                   MPI_COMM_WORLD, &requests[0]);
    
    MPI_Iallreduce(data2, result2, COUNT, MPI_DOUBLE, MPI_MAX, 
                   MPI_COMM_WORLD, &requests[1]);
    
    MPI_Ibcast(broadcast_data, COUNT, MPI_DOUBLE, 0, 
               MPI_COMM_WORLD, &requests[2]);
    
    printf("Process %d: Started 3 non-blocking collective operations\n", rank);
    
    // 执行大量本地计算
    double computation_result = 0.0;
    for (int i = 0; i < 1000000; i++) {
        computation_result += sqrt(i) * log(i + 1);
    }
    
    printf("Process %d: Completed local computation: %.6e\n", 
           rank, computation_result);
    
    // 等待所有集合操作完成
    MPI_Status statuses[3];
    MPI_Waitall(3, requests, statuses);
    
    printf("Process %d: All non-blocking collectives completed\n", rank);
    
    // 验证结果
    printf("Process %d: Results - Sum[0]=%.1f, Max[0]=%.1f, Bcast[0]=%.5f\n",
           rank, result1[0], result2[0], broadcast_data[0]);
    
    free(data1);
    free(data2);
    free(result1);
    free(result2);
    free(broadcast_data);
}
```

## 10. 性能测量和优化

### 10.1 时间测量

```c
void timing_measurements() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    double start_time, end_time, elapsed_time;
    const int WORK_SIZE = 1000000;
    double *work_array = malloc(WORK_SIZE * sizeof(double));
    
    // 初始化工作数据
    for (int i = 0; i < WORK_SIZE; i++) {
        work_array[i] = i * 1.5;
    }
    
    // 同步所有进程
    MPI_Barrier(MPI_COMM_WORLD);
    
    // 测量计算时间
    start_time = MPI_Wtime();
    
    // 执行计算工作
    double result = 0.0;
    for (int i = 0; i < WORK_SIZE; i++) {
        result += sin(work_array[i]) * cos(work_array[i]);
    }
    
    end_time = MPI_Wtime();
    elapsed_time = end_time - start_time;
    
    printf("Process %d: Computation time: %.6f seconds, result: %.6e\n",
           rank, elapsed_time, result);
    
    // 收集所有进程的时间统计
    double max_time, min_time, avg_time;
    
    MPI_Reduce(&elapsed_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&elapsed_time, &min_time, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&elapsed_time, &avg_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    
    if (rank == 0) {
        avg_time /= size;
        printf("\nTiming Statistics:\n");
        printf("  Maximum time: %.6f seconds\n", max_time);
        printf("  Minimum time: %.6f seconds\n", min_time);
        printf("  Average time: %.6f seconds\n", avg_time);
        printf("  Load imbalance: %.2f%%\n", 
               ((max_time - min_time) / avg_time) * 100.0);
        printf("  Timer resolution: %.9f seconds\n", MPI_Wtick());
    }
    
    // 测量通信时间
    MPI_Barrier(MPI_COMM_WORLD);
    
    start_time = MPI_Wtime();
    
    // 执行全归约通信
    double local_sum = result;
    double global_sum;
    MPI_Allreduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    
    end_time = MPI_Wtime();
    double comm_time = end_time - start_time;
    
    printf("Process %d: Communication time: %.6f seconds\n", rank, comm_time);
    
    free(work_array);
}
```

### 10.2 带宽和延迟测量

```c
void bandwidth_latency_test() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (size < 2) {
        printf("Bandwidth test requires at least 2 processes\n");
        return;
    }
    
    const int MAX_SIZE = 1024 * 1024;  // 1MB
    const int NUM_ITERATIONS = 100;
    
    char *send_buffer = malloc(MAX_SIZE);
    char *recv_buffer = malloc(MAX_SIZE);
    
    // 初始化缓冲区
    memset(send_buffer, 'A' + rank, MAX_SIZE);
    
    if (rank == 0) {
        printf("Message Size (bytes)\tLatency (μs)\tBandwidth (MB/s)\n");
        printf("---------------------------------------------------\n");
    }
    
    // 测试不同消息大小
    for (int size_bytes = 1; size_bytes <= MAX_SIZE; size_bytes *= 2) {
        double total_time = 0.0;
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        if (rank == 0) {
            double start_time = MPI_Wtime();
            
            for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
                MPI_Send(send_buffer, size_bytes, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
                MPI_Recv(recv_buffer, size_bytes, MPI_BYTE, 1, 0, MPI_COMM_WORLD, 
                         MPI_STATUS_IGNORE);
            }
            
            double end_time = MPI_Wtime();
            total_time = end_time - start_time;
            
            // 计算往返时间和带宽
            double latency_us = (total_time / NUM_ITERATIONS / 2.0) * 1e6;
            double bandwidth_mbps = (size_bytes * NUM_ITERATIONS * 2.0) / 
                                   (total_time * 1024 * 1024);
            
            printf("%15d\t%11.2f\t%13.2f\n", size_bytes, latency_us, bandwidth_mbps);
            
        } else if (rank == 1) {
            for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
                MPI_Recv(recv_buffer, size_bytes, MPI_BYTE, 0, 0, MPI_COMM_WORLD,
                         MPI_STATUS_IGNORE);
                MPI_Send(recv_buffer, size_bytes, MPI_BYTE, 0, 0, MPI_COMM_WORLD);
            }
        }
    }
    
    free(send_buffer);
    free(recv_buffer);
}
```

## 11. 调试和错误处理

### 11.1 高级错误处理

```c
void error_handler(MPI_Comm *comm, int *error_code, ...) {
    char error_string[MPI_MAX_ERROR_STRING];
    int length, rank;
    
    MPI_Error_string(*error_code, error_string, &length);
    MPI_Comm_rank(*comm, &rank);
    
    printf("Process %d: MPI Error: %s\n", rank, error_string);
    
    // 记录错误并继续执行或终止
    MPI_Abort(*comm, *error_code);
}

void advanced_error_handling() {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // 创建自定义错误处理器
    MPI_Errhandler custom_errhandler;
    MPI_Comm_create_errhandler(error_handler, &custom_errhandler);
    
    // 设置错误处理器
    MPI_Comm_set_errhandler(MPI_COMM_WORLD, custom_errhandler);
    
    // 故意触发错误（发送到不存在的进程）
    int data = 42;
    int result = MPI_Send(&data, 1, MPI_INT, 999, 0, MPI_COMM_WORLD);
    
    if (result != MPI_SUCCESS) {
        printf("Process %d: Send operation failed as expected\n", rank);
    }
    
    // 清理
    MPI_Errhandler_free(&custom_errhandler);
}
```

### 11.2 调试支持

```c
void debugging_support() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // 获取处理器名称用于调试
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);
    
    printf("Process %d running on %s\n", rank, processor_name);
    
    // 调试钩子 - 在这里可以附加调试器
    if (rank == 0) {
        printf("Attach debugger to process %d on %s, PID: %d\n", 
               rank, processor_name, getpid());
        printf("Press any key to continue...\n");
        getchar();
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // 检查MPI实现信息
    int version, subversion;
    MPI_Get_version(&version, &subversion);
    
    char library_version[MPI_MAX_LIBRARY_VERSION_STRING];
    int version_len;
    MPI_Get_library_version(library_version, &version_len);
    
    if (rank == 0) {
        printf("MPI Version: %d.%d\n", version, subversion);
        printf("Library: %s\n", library_version);
    }
}
```

## 12. 最佳实践和常见陷阱

### 12.1 性能最佳实践

```c
void performance_best_practices() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // 1. 使用非阻塞通信重叠计算和通信
    const int COUNT = 100000;
    double *data = malloc(COUNT * sizeof(double));
    double *recv_data = malloc(COUNT * sizeof(double));
    
    // 初始化数据
    for (int i = 0; i < COUNT; i++) {
        data[i] = rank * COUNT + i;
    }
    
    if (rank == 0) {
        MPI_Request send_request;
        
        // 启动非阻塞发送
        MPI_Isend(data, COUNT, MPI_DOUBLE, 1, 0, MPI_COMM_WORLD, &send_request);
        
        // 在发送进行时执行计算
        double local_sum = 0.0;
        for (int i = 0; i < COUNT; i++) {
            local_sum += data[i] * data[i];
        }
        
        // 等待发送完成
        MPI_Wait(&send_request, MPI_STATUS_IGNORE);
        
        printf("Process 0: Overlapped computation result: %.2e\n", local_sum);
        
    } else if (rank == 1) {
        MPI_Request recv_request;
        
        // 启动非阻塞接收
        MPI_Irecv(recv_data, COUNT, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, &recv_request);
        
        // 在接收进行时执行其他工作
        double work_result = 0.0;
        for (int i = 0; i < 50000; i++) {
            work_result += sin(i * 0.001);
        }
        
        // 等待接收完成
        MPI_Wait(&recv_request, MPI_STATUS_IGNORE);
        
        printf("Process 1: Work during receive: %.6f\n", work_result);
    }
    
    // 2. 选择合适的集合通信算法
    double *array_data = malloc(COUNT * sizeof(double));
    double *result_data = malloc(COUNT * sizeof(double));
    
    for (int i = 0; i < COUNT; i++) {
        array_data[i] = rank + i * 0.1;
    }
    
    // 对于大数组，全归约比归约+广播更高效
    MPI_Allreduce(array_data, result_data, COUNT, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    
    // 3. 避免不必要的数据拷贝
    // 直接在原地操作
    MPI_Allreduce(MPI_IN_PLACE, array_data, COUNT, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
    
    free(data);
    free(recv_data);
    free(array_data);
    free(result_data);
}
```

### 12.2 常见陷阱和解决方案

```c
void common_pitfalls_and_solutions() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (size < 2) return;
    
    // 陷阱1：死锁 - 错误的发送/接收顺序
    printf("Demonstrating deadlock avoidance...\n");
    
    int send_data = rank;
    int recv_data;
    
    // 错误的做法（会死锁）：
    /*
    if (rank == 0) {
        MPI_Send(&send_data, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
        MPI_Recv(&recv_data, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    } else if (rank == 1) {
        MPI_Send(&send_data, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        MPI_Recv(&recv_data, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    */
    
    // 正确的做法1：使用非阻塞通信
    if (rank == 0) {
        MPI_Request send_req, recv_req;
        MPI_Isend(&send_data, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, &send_req);
        MPI_Irecv(&recv_data, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, &recv_req);
        
        MPI_Wait(&send_req, MPI_STATUS_IGNORE);
        MPI_Wait(&recv_req, MPI_STATUS_IGNORE);
        
        printf("Process 0: Sent %d, received %d\n", send_data, recv_data);
        
    } else if (rank == 1) {
        MPI_Request send_req, recv_req;
        MPI_Isend(&send_data, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &send_req);
        MPI_Irecv(&recv_data, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &recv_req);
        
        MPI_Wait(&send_req, MPI_STATUS_IGNORE);
        MPI_Wait(&recv_req, MPI_STATUS_IGNORE);
        
        printf("Process 1: Sent %d, received %d\n", send_data, recv_data);
    }
    
    // 正确的做法2：使用Sendrecv
    if (rank < 2) {
        int partner = 1 - rank;
        MPI_Sendrecv(&send_data, 1, MPI_INT, partner, 0,
                     &recv_data, 1, MPI_INT, partner, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        printf("Process %d: Sendrecv - sent %d, received %d\n", 
               rank, send_data, recv_data);
    }
    
    // 陷阱2：缓冲区溢出
    printf("Demonstrating buffer overflow protection...\n");
    
    // 检查接收消息的实际大小
    if (rank == 0) {
        int large_data[1000];
        for (int i = 0; i < 1000; i++) large_data[i] = i;
        
        MPI_Send(large_data, 500, MPI_INT, 1, 0, MPI_COMM_WORLD);  // 只发送500个元素
        
    } else if (rank == 1) {
        int recv_buffer[1000];
        MPI_Status status;
        
        MPI_Recv(recv_buffer, 1000, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
        
        int actual_count;
        MPI_Get_count(&status, MPI_INT, &actual_count);
        
        printf("Process 1: Expected up to 1000, actually received %d elements\n", 
               actual_count);
    }
}
```

## 13. 总结

### 13.1 MPI编程要点

1. **初始化和清理**：总是调用`MPI_Init`和`MPI_Finalize`
2. **进程标识**：使用`MPI_Comm_rank`和`MPI_Comm_size`获取进程信息
3. **通信选择**：根据需要选择点对点或集合通信
4. **数据类型**：正确使用MPI数据类型，必要时创建派生类型
5. **错误处理**：设置适当的错误处理策略
6. **性能优化**：使用非阻塞通信、重叠计算通信、选择合适的算法

### 13.2 高性能计算最佳实践

1. **负载均衡**：确保工作在进程间均匀分布
2. **通信最小化**：减少不必要的数据传输
3. **内存管理**：有效管理内存分配和释放
4. **可扩展性**：设计能在不同规模下高效运行的算法
5. **调试和测试**：使用适当的工具进行性能分析和调试

MPI是高性能并行计算的基础，掌握其核心概念和最佳实践对开发高效的并行应用至关重要。通过合理使用MPI的各种功能，可以充分发挥现代并行计算系统的性能潜力。