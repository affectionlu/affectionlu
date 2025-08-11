# Pthread多线程编程完整技术报告

## 1. Pthread概述

### 1.1 POSIX线程简介

POSIX线程（Pthread）是IEEE POSIX 1003.1c标准定义的线程API，是Unix和类Unix系统上多线程编程的标准接口。Pthread提供了创建、管理和同步线程的完整功能集。

### 1.2 基本程序结构

```c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void* thread_function(void* arg) {
    int thread_id = *(int*)arg;
    printf("Hello from thread %d\n", thread_id);
    return NULL;
}

int main() {
    pthread_t thread;
    int thread_id = 1;
    
    // 创建线程
    pthread_create(&thread, NULL, thread_function, &thread_id);
    
    // 等待线程完成
    pthread_join(thread, NULL);
    
    printf("Main thread finished\n");
    return 0;
}
```

### 1.3 编译和运行

```bash
# 编译
gcc -pthread -o pthread_example pthread_example.c

# 运行
./pthread_example
```

## 2. 线程管理

### 2.1 线程创建和生命周期

```c
typedef struct {
    int id;
    double *data;
    int size;
    double result;
} thread_data_t;

void* worker_thread(void* arg) {
    thread_data_t* tdata = (thread_data_t*)arg;
    
    printf("Thread %d processing %d elements\n", tdata->id, tdata->size);
    
    // 执行计算
    double sum = 0.0;
    for (int i = 0; i < tdata->size; i++) {
        sum += tdata->data[i] * tdata->data[i];
    }
    tdata->result = sum;
    
    printf("Thread %d completed, result: %.2f\n", tdata->id, tdata->result);
    return (void*)(long)tdata->id;
}

int main() {
    const int NUM_THREADS = 4;
    const int DATA_SIZE = 1000;
    
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    
    // 准备数据
    double *shared_data = malloc(DATA_SIZE * sizeof(double));
    for (int i = 0; i < DATA_SIZE; i++) {
        shared_data[i] = i * 1.5;
    }
    
    // 创建线程
    int chunk_size = DATA_SIZE / NUM_THREADS;
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].id = i;
        thread_data[i].data = shared_data + i * chunk_size;
        thread_data[i].size = chunk_size;
        
        int result = pthread_create(&threads[i], NULL, worker_thread, &thread_data[i]);
        if (result != 0) {
            printf("Error creating thread %d\n", i);
            exit(1);
        }
    }
    
    // 等待所有线程完成并收集结果
    double total_result = 0.0;
    for (int i = 0; i < NUM_THREADS; i++) {
        void* return_value;
        pthread_join(threads[i], &return_value);
        
        printf("Thread %d returned: %ld\n", i, (long)return_value);
        total_result += thread_data[i].result;
    }
    
    printf("Total result: %.2f\n", total_result);
    
    free(shared_data);
    return 0;
}
```

### 2.2 线程属性管理

```c
void thread_attributes_example() {
    pthread_t thread;
    pthread_attr_t attr;
    
    // 初始化属性对象
    pthread_attr_init(&attr);
    
    // 设置分离状态
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    // 设置栈大小（2MB）
    size_t stack_size = 2 * 1024 * 1024;
    pthread_attr_setstacksize(&attr, stack_size);
    
    // 设置调度策略
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    
    // 设置调度参数
    struct sched_param param;
    param.sched_priority = 50;
    pthread_attr_setschedparam(&attr, &param);
    
    // 设置继承调度
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    
    // 设置竞争范围
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    
    // 自定义栈地址和大小
    void *stack_addr = malloc(stack_size);
    pthread_attr_setstack(&attr, stack_addr, stack_size);
    
    // 设置保护区大小
    pthread_attr_setguardsize(&attr, 4096);
    
    // 创建线程
    pthread_create(&thread, &attr, worker_function, NULL);
    
    // 清理属性对象
    pthread_attr_destroy(&attr);
    
    // 注意：对于分离线程，不能调用pthread_join
    // pthread_join(thread, NULL); // 错误！
    
    free(stack_addr);
}
```

### 2.3 分离线程和可连接线程

```c
void* detached_worker(void* arg) {
    int work_id = *(int*)arg;
    
    printf("Detached thread %d starting work\n", work_id);
    
    // 执行一些工作
    for (int i = 0; i < 1000000; i++) {
        // 模拟工作
    }
    
    printf("Detached thread %d completed\n", work_id);
    return NULL;
}

void detached_threads_example() {
    const int NUM_DETACHED = 3;
    pthread_t threads[NUM_DETACHED];
    int work_ids[NUM_DETACHED];
    
    for (int i = 0; i < NUM_DETACHED; i++) {
        work_ids[i] = i;
        
        // 创建可连接线程
        pthread_create(&threads[i], NULL, detached_worker, &work_ids[i]);
        
        // 将线程设置为分离状态
        pthread_detach(threads[i]);
    }
    
    printf("Main thread created %d detached threads\n", NUM_DETACHED);
    
    // 等待一段时间让分离线程完成工作
    sleep(2);
    
    // 分离线程会自动清理资源，无需pthread_join
    printf("Main thread exiting\n");
}
```

## 3. 同步机制

### 3.1 互斥锁（Mutex）

```c
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int shared_counter = 0;

void* increment_thread(void* arg) {
    int iterations = *(int*)arg;
    
    for (int i = 0; i < iterations; i++) {
        pthread_mutex_lock(&mutex);
        
        // 临界区
        int temp = shared_counter;
        temp++;
        usleep(1); // 模拟一些处理时间
        shared_counter = temp;
        
        pthread_mutex_unlock(&mutex);
    }
    
    return NULL;
}

void mutex_example() {
    const int NUM_THREADS = 4;
    const int ITERATIONS = 1000;
    
    pthread_t threads[NUM_THREADS];
    int thread_iterations = ITERATIONS;
    
    printf("Starting with counter = %d\n", shared_counter);
    
    // 创建线程
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, increment_thread, &thread_iterations);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Final counter value: %d\n", shared_counter);
    printf("Expected value: %d\n", NUM_THREADS * ITERATIONS);
    
    pthread_mutex_destroy(&mutex);
}
```

### 3.2 递归互斥锁

```c
void recursive_mutex_example() {
    pthread_mutex_t recursive_mutex;
    pthread_mutexattr_t attr;
    
    // 创建递归互斥锁属性
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    
    // 初始化递归互斥锁
    pthread_mutex_init(&recursive_mutex, &attr);
    
    // 可以多次锁定同一个互斥锁
    pthread_mutex_lock(&recursive_mutex);
    printf("First lock acquired\n");
    
    pthread_mutex_lock(&recursive_mutex);
    printf("Second lock acquired (recursive)\n");
    
    pthread_mutex_lock(&recursive_mutex);
    printf("Third lock acquired (recursive)\n");
    
    // 必须相应地解锁相同次数
    pthread_mutex_unlock(&recursive_mutex);
    pthread_mutex_unlock(&recursive_mutex);
    pthread_mutex_unlock(&recursive_mutex);
    
    printf("All locks released\n");
    
    // 清理
    pthread_mutex_destroy(&recursive_mutex);
    pthread_mutexattr_destroy(&attr);
}
```

### 3.3 条件变量

```c
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int data_ready;
    int data;
    int consumers_waiting;
} shared_data_t;

shared_data_t shared = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .condition = PTHREAD_COND_INITIALIZER,
    .data_ready = 0,
    .consumers_waiting = 0
};

void* producer(void* arg) {
    int num_items = *(int*)arg;
    
    for (int i = 0; i < num_items; i++) {
        pthread_mutex_lock(&shared.mutex);
        
        // 等待直到没有数据等待被消费
        while (shared.data_ready) {
            pthread_cond_wait(&shared.condition, &shared.mutex);
        }
        
        // 生产数据
        shared.data = i;
        shared.data_ready = 1;
        
        printf("Producer: produced item %d\n", i);
        
        // 通知等待的消费者
        pthread_cond_broadcast(&shared.condition);
        
        pthread_mutex_unlock(&shared.mutex);
        
        usleep(100000); // 模拟生产时间
    }
    
    return NULL;
}

void* consumer(void* arg) {
    int consumer_id = *(int*)arg;
    
    while (1) {
        pthread_mutex_lock(&shared.mutex);
        
        shared.consumers_waiting++;
        
        // 等待数据准备好
        while (!shared.data_ready) {
            pthread_cond_wait(&shared.condition, &shared.mutex);
        }
        
        shared.consumers_waiting--;
        
        // 消费数据
        int consumed_data = shared.data;
        shared.data_ready = 0;
        
        printf("Consumer %d: consumed item %d\n", consumer_id, consumed_data);
        
        // 通知生产者可以生产更多数据
        pthread_cond_signal(&shared.condition);
        
        pthread_mutex_unlock(&shared.mutex);
        
        // 如果是结束标志，退出
        if (consumed_data == -1) {
            break;
        }
        
        usleep(150000); // 模拟消费时间
    }
    
    return NULL;
}

void producer_consumer_example() {
    const int NUM_CONSUMERS = 3;
    const int NUM_ITEMS = 10;
    
    pthread_t producer_thread;
    pthread_t consumer_threads[NUM_CONSUMERS];
    
    int num_items = NUM_ITEMS;
    int consumer_ids[NUM_CONSUMERS];
    
    // 创建消费者线程
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumer_ids[i] = i;
        pthread_create(&consumer_threads[i], NULL, consumer, &consumer_ids[i]);
    }
    
    // 创建生产者线程
    pthread_create(&producer_thread, NULL, producer, &num_items);
    
    // 等待生产者完成
    pthread_join(producer_thread, NULL);
    
    // 发送结束信号给消费者
    pthread_mutex_lock(&shared.mutex);
    shared.data = -1;
    shared.data_ready = 1;
    pthread_cond_broadcast(&shared.condition);
    pthread_mutex_unlock(&shared.mutex);
    
    // 等待所有消费者完成
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumer_threads[i], NULL);
    }
    
    printf("Producer-Consumer example completed\n");
}
```

### 3.4 读写锁

```c
pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
int shared_data = 0;
int read_count = 0;

void* reader_thread(void* arg) {
    int reader_id = *(int*)arg;
    
    for (int i = 0; i < 5; i++) {
        pthread_rwlock_rdlock(&rwlock);
        
        // 读取共享数据
        int local_copy = shared_data;
        read_count++;
        
        printf("Reader %d: read value %d (read #%d)\n", 
               reader_id, local_copy, read_count);
        
        pthread_rwlock_unlock(&rwlock);
        
        usleep(200000); // 模拟读取处理时间
    }
    
    return NULL;
}

void* writer_thread(void* arg) {
    int writer_id = *(int*)arg;
    
    for (int i = 0; i < 3; i++) {
        pthread_rwlock_wrlock(&rwlock);
        
        // 写入共享数据
        shared_data++;
        printf("Writer %d: wrote value %d\n", writer_id, shared_data);
        
        pthread_rwlock_unlock(&rwlock);
        
        usleep(500000); // 模拟写入处理时间
    }
    
    return NULL;
}

void reader_writer_example() {
    const int NUM_READERS = 3;
    const int NUM_WRITERS = 2;
    
    pthread_t readers[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    
    int reader_ids[NUM_READERS];
    int writer_ids[NUM_WRITERS];
    
    printf("Starting reader-writer example\n");
    
    // 创建读者线程
    for (int i = 0; i < NUM_READERS; i++) {
        reader_ids[i] = i;
        pthread_create(&readers[i], NULL, reader_thread, &reader_ids[i]);
    }
    
    // 创建写者线程
    for (int i = 0; i < NUM_WRITERS; i++) {
        writer_ids[i] = i;
        pthread_create(&writers[i], NULL, writer_thread, &writer_ids[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }
    
    for (int i = 0; i < NUM_WRITERS; i++) {
        pthread_join(writers[i], NULL);
    }
    
    printf("Reader-Writer example completed. Final value: %d\n", shared_data);
    
    pthread_rwlock_destroy(&rwlock);
}
```

### 3.5 屏障（Barrier）

```c
pthread_barrier_t barrier;
int barrier_result[4];

void* barrier_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    printf("Thread %d: Phase 1 starting\n", thread_id);
    
    // 模拟第一阶段工作（不同的工作时间）
    usleep((thread_id + 1) * 100000);
    
    printf("Thread %d: Phase 1 completed\n", thread_id);
    
    // 等待所有线程完成第一阶段
    int result = pthread_barrier_wait(&barrier);
    
    if (result == PTHREAD_BARRIER_SERIAL_THREAD) {
        printf("Thread %d: I am the serial thread at barrier\n", thread_id);
        // 串行线程可以执行一些特殊操作
    }
    
    printf("Thread %d: Phase 2 starting\n", thread_id);
    
    // 模拟第二阶段工作
    usleep((4 - thread_id) * 100000);
    barrier_result[thread_id] = thread_id * 10;
    
    printf("Thread %d: Phase 2 completed, result = %d\n", 
           thread_id, barrier_result[thread_id]);
    
    return NULL;
}

void barrier_example() {
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    // 初始化屏障
    pthread_barrier_init(&barrier, NULL, NUM_THREADS);
    
    printf("Starting barrier example with %d threads\n", NUM_THREADS);
    
    // 创建线程
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, barrier_worker, &thread_ids[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("All threads completed. Results: ");
    for (int i = 0; i < NUM_THREADS; i++) {
        printf("%d ", barrier_result[i]);
    }
    printf("\n");
    
    pthread_barrier_destroy(&barrier);
}
```

### 3.6 自旋锁

```c
pthread_spinlock_t spinlock;
volatile int spin_counter = 0;

void* spin_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < 1000000; i++) {
        pthread_spin_lock(&spinlock);
        
        // 非常短的临界区
        spin_counter++;
        
        pthread_spin_unlock(&spinlock);
        
        // 模拟少量非临界区工作
        if (i % 100000 == 0) {
            printf("Thread %d: iteration %d, counter = %d\n", 
                   thread_id, i, spin_counter);
        }
    }
    
    return NULL;
}

void spinlock_example() {
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    // 初始化自旋锁
    pthread_spin_init(&spinlock, PTHREAD_PROCESS_PRIVATE);
    
    printf("Starting spinlock example\n");
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // 创建线程
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, spin_worker, &thread_ids[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("Spinlock example completed in %.3f seconds\n", elapsed);
    printf("Final counter value: %d\n", spin_counter);
    printf("Expected value: %d\n", NUM_THREADS * 1000000);
    
    pthread_spin_destroy(&spinlock);
}
```

## 4. 线程本地存储

### 4.1 __thread关键字

```c
__thread int thread_local_counter = 0;
__thread char thread_local_buffer[1024];

void* tls_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    // 每个线程有自己的副本
    thread_local_counter = thread_id * 100;
    snprintf(thread_local_buffer, sizeof(thread_local_buffer), 
             "Thread %d data", thread_id);
    
    printf("Thread %d: Initial TLS counter = %d\n", thread_id, thread_local_counter);
    printf("Thread %d: TLS buffer = '%s'\n", thread_id, thread_local_buffer);
    
    for (int i = 0; i < 5; i++) {
        thread_local_counter++;
        
        printf("Thread %d: TLS counter = %d, buffer = '%s'\n", 
               thread_id, thread_local_counter, thread_local_buffer);
        
        usleep(100000);
    }
    
    return NULL;
}

void thread_local_storage_example() {
    const int NUM_THREADS = 3;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    printf("Thread Local Storage example\n");
    
    // 创建线程
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, tls_worker, &thread_ids[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Main thread TLS counter = %d\n", thread_local_counter);
}
```

### 4.2 pthread_key机制

```c
pthread_key_t thread_data_key;

typedef struct {
    int id;
    char name[64];
    double value;
    int *array;
} thread_specific_data_t;

void cleanup_thread_data(void* data) {
    if (data) {
        thread_specific_data_t* tsd = (thread_specific_data_t*)data;
        printf("Cleaning up thread data for thread %d (%s)\n", tsd->id, tsd->name);
        
        if (tsd->array) {
            free(tsd->array);
        }
        free(tsd);
    }
}

void* key_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    // 为当前线程分配特定数据
    thread_specific_data_t* data = malloc(sizeof(thread_specific_data_t));
    data->id = thread_id;
    snprintf(data->name, sizeof(data->name), "Thread_%d", thread_id);
    data->value = thread_id * 3.14159;
    data->array = malloc(100 * sizeof(int));
    
    // 初始化数组
    for (int i = 0; i < 100; i++) {
        data->array[i] = thread_id * 100 + i;
    }
    
    // 将数据与key关联
    pthread_setspecific(thread_data_key, data);
    
    printf("Thread %d: Set thread-specific data\n", thread_id);
    
    // 执行一些工作
    for (int i = 0; i < 3; i++) {
        // 获取线程特定数据
        thread_specific_data_t* my_data = 
            (thread_specific_data_t*)pthread_getspecific(thread_data_key);
        
        if (my_data) {
            my_data->value += 1.0;
            printf("Thread %d (%s): iteration %d, value = %.2f\n", 
                   thread_id, my_data->name, i, my_data->value);
            
            // 显示数组的一些元素
            printf("  Array elements: %d, %d, %d\n", 
                   my_data->array[0], my_data->array[50], my_data->array[99]);
        }
        
        usleep(200000);
    }
    
    return NULL;
}

void pthread_key_example() {
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    // 创建key并设置清理函数
    pthread_key_create(&thread_data_key, cleanup_thread_data);
    
    printf("pthread_key example starting\n");
    
    // 创建线程
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, key_worker, &thread_ids[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("All threads completed\n");
    
    // 清理key
    pthread_key_delete(thread_data_key);
}
```

## 5. 线程取消

### 5.1 基本取消操作

```c
void* cancelable_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    // 设置取消状态和类型
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    
    printf("Thread %d: Starting cancelable work\n", thread_id);
    
    for (int i = 0; i < 100; i++) {
        // 检查取消请求
        pthread_testcancel();
        
        printf("Thread %d: Working... iteration %d\n", thread_id, i);
        
        // 模拟工作
        usleep(100000);
        
        // 定期检查取消
        if (i % 10 == 0) {
            pthread_testcancel();
        }
    }
    
    printf("Thread %d: Work completed normally\n", thread_id);
    return NULL;
}

void cancellation_example() {
    pthread_t thread;
    int thread_id = 1;
    
    printf("Starting cancellation example\n");
    
    // 创建可取消的线程
    pthread_create(&thread, NULL, cancelable_worker, &thread_id);
    
    // 让线程运行一段时间
    sleep(2);
    
    printf("Main thread: Canceling worker thread\n");
    
    // 发送取消请求
    pthread_cancel(thread);
    
    // 等待线程终止
    void* result;
    pthread_join(thread, &result);
    
    if (result == PTHREAD_CANCELED) {
        printf("Main thread: Worker thread was canceled\n");
    } else {
        printf("Main thread: Worker thread completed normally\n");
    }
}
```

### 5.2 清理处理程序

```c
void cleanup_handler1(void* arg) {
    printf("Cleanup handler 1 called with arg: %s\n", (char*)arg);
}

void cleanup_handler2(void* arg) {
    printf("Cleanup handler 2 called with arg: %s\n", (char*)arg);
    if (arg) {
        free(arg);
    }
}

void* cleanup_worker(void* arg) {
    int thread_id = *(int*)arg;
    char *buffer = malloc(1024);
    
    printf("Thread %d: Starting work with cleanup handlers\n", thread_id);
    
    // 推入清理处理程序（后进先出顺序）
    pthread_cleanup_push(cleanup_handler1, "First handler");
    pthread_cleanup_push(cleanup_handler2, buffer);
    
    // 设置取消参数
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    
    for (int i = 0; i < 50; i++) {
        pthread_testcancel();
        
        printf("Thread %d: iteration %d\n", thread_id, i);
        usleep(200000);
    }
    
    printf("Thread %d: Work completed, popping cleanup handlers\n", thread_id);
    
    // 弹出清理处理程序（execute参数决定是否执行）
    pthread_cleanup_pop(1);  // 执行cleanup_handler2
    pthread_cleanup_pop(1);  // 执行cleanup_handler1
    
    return NULL;
}

void cleanup_example() {
    pthread_t thread;
    int thread_id = 2;
    
    printf("Starting cleanup example\n");
    
    pthread_create(&thread, NULL, cleanup_worker, &thread_id);
    
    // 让线程运行一段时间然后取消
    sleep(3);
    
    printf("Main thread: Sending cancel request\n");
    pthread_cancel(thread);
    
    void* result;
    pthread_join(thread, &result);
    
    if (result == PTHREAD_CANCELED) {
        printf("Main thread: Thread was canceled (cleanup handlers executed)\n");
    }
}
```

### 5.3 异步取消

```c
void* async_cancelable_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    // 设置异步取消
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    
    printf("Thread %d: Starting async cancelable work\n", thread_id);
    
    // 在异步取消模式下，线程可以在任何时候被取消
    for (int i = 0; i < 1000000; i++) {
        // 执行一些计算密集型工作
        double result = 0.0;
        for (int j = 0; j < 1000; j++) {
            result += sin(i * j * 0.001);
        }
        
        if (i % 100000 == 0) {
            printf("Thread %d: computation iteration %d\n", thread_id, i);
        }
    }
    
    printf("Thread %d: Async work completed\n", thread_id);
    return NULL;
}

void async_cancellation_example() {
    pthread_t thread;
    int thread_id = 3;
    
    printf("Starting async cancellation example\n");
    
    pthread_create(&thread, NULL, async_cancelable_worker, &thread_id);
    
    // 很快就取消线程
    usleep(500000);  // 0.5秒
    
    printf("Main thread: Sending async cancel request\n");
    pthread_cancel(thread);
    
    void* result;
    pthread_join(thread, &result);
    
    if (result == PTHREAD_CANCELED) {
        printf("Main thread: Async thread was canceled\n");
    }
}
```

## 6. 原子操作

### 6.1 GCC内置原子操作

```c
volatile int atomic_counter = 0;
volatile long atomic_sum = 0;

void* atomic_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    printf("Thread %d: Starting atomic operations\n", thread_id);
    
    for (int i = 0; i < 1000000; i++) {
        // 原子递增
        __sync_fetch_and_add(&atomic_counter, 1);
        
        // 原子加法
        __sync_fetch_and_add(&atomic_sum, i);
        
        // 原子比较和交换
        int expected = i;
        __sync_bool_compare_and_swap(&expected, i, i + 1);
        
        if (i % 200000 == 0) {
            printf("Thread %d: iteration %d, counter = %d\n", 
                   thread_id, i, atomic_counter);
        }
    }
    
    printf("Thread %d: Completed atomic operations\n", thread_id);
    return NULL;
}

void gcc_atomic_example() {
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    printf("GCC atomic operations example\n");
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // 创建线程
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, atomic_worker, &thread_ids[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("Atomic operations completed in %.3f seconds\n", elapsed);
    printf("Final counter: %d (expected: %d)\n", atomic_counter, NUM_THREADS * 1000000);
    printf("Final sum: %ld\n", atomic_sum);
}
```

### 6.2 C11原子操作

```c
#include <stdatomic.h>

atomic_int c11_counter = ATOMIC_VAR_INIT(0);
atomic_long c11_sum = ATOMIC_VAR_INIT(0);

void* c11_atomic_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    printf("Thread %d: Starting C11 atomic operations\n", thread_id);
    
    for (int i = 0; i < 500000; i++) {
        // C11原子操作
        atomic_fetch_add(&c11_counter, 1);
        atomic_fetch_add(&c11_sum, i);
        
        // 原子存储和加载
        if (i % 100000 == 0) {
            int current_counter = atomic_load(&c11_counter);
            printf("Thread %d: iteration %d, C11 counter = %d\n", 
                   thread_id, i, current_counter);
        }
        
        // 原子比较交换
        int expected = i;
        atomic_compare_exchange_weak(&expected, &expected, i + 1);
    }
    
    printf("Thread %d: Completed C11 atomic operations\n", thread_id);
    return NULL;
}

void c11_atomic_example() {
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    printf("C11 atomic operations example\n");
    
    // 创建线程
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, c11_atomic_worker, &thread_ids[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    int final_counter = atomic_load(&c11_counter);
    long final_sum = atomic_load(&c11_sum);
    
    printf("C11 atomic operations completed\n");
    printf("Final C11 counter: %d (expected: %d)\n", final_counter, NUM_THREADS * 500000);
    printf("Final C11 sum: %ld\n", final_sum);
}
```

### 6.3 内存序模型

```c
atomic_int data = ATOMIC_VAR_INIT(0);
atomic_int flag = ATOMIC_VAR_INIT(0);

void* memory_order_producer(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < 1000; i++) {
        // 使用release语义存储数据
        atomic_store_explicit(&data, i, memory_order_release);
        
        // 设置标志，使用release语义
        atomic_store_explicit(&flag, 1, memory_order_release);
        
        usleep(1000);
        
        // 重置标志
        atomic_store_explicit(&flag, 0, memory_order_relaxed);
    }
    
    printf("Producer %d completed\n", thread_id);
    return NULL;
}

void* memory_order_consumer(void* arg) {
    int thread_id = *(int*)arg;
    int last_value = -1;
    
    while (1) {
        // 使用acquire语义读取标志
        if (atomic_load_explicit(&flag, memory_order_acquire) == 1) {
            // 使用acquire语义读取数据
            int current_data = atomic_load_explicit(&data, memory_order_acquire);
            
            if (current_data != last_value) {
                printf("Consumer %d: read data = %d\n", thread_id, current_data);
                last_value = current_data;
                
                if (current_data >= 999) {
                    break;
                }
            }
        }
        
        usleep(500);
    }
    
    printf("Consumer %d completed\n", thread_id);
    return NULL;
}

void memory_order_example() {
    pthread_t producer, consumer;
    int producer_id = 1, consumer_id = 2;
    
    printf("Memory ordering example\n");
    
    pthread_create(&producer, NULL, memory_order_producer, &producer_id);
    pthread_create(&consumer, NULL, memory_order_consumer, &consumer_id);
    
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);
    
    printf("Memory ordering example completed\n");
}
```

## 7. 高级主题

### 7.1 简单线程池实现

```c
typedef struct task {
    void (*function)(void* arg);
    void* argument;
    struct task* next;
} task_t;

typedef struct {
    pthread_t* threads;
    task_t* task_queue;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_condition;
    int num_threads;
    bool shutdown;
} thread_pool_t;

void* worker_thread(void* arg) {
    thread_pool_t* pool = (thread_pool_t*)arg;
    
    while (1) {
        pthread_mutex_lock(&pool->queue_mutex);
        
        // 等待任务或关闭信号
        while (pool->task_queue == NULL && !pool->shutdown) {
            pthread_cond_wait(&pool->queue_condition, &pool->queue_mutex);
        }
        
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->queue_mutex);
            break;
        }
        
        // 获取任务
        task_t* task = pool->task_queue;
        pool->task_queue = task->next;
        
        pthread_mutex_unlock(&pool->queue_mutex);
        
        // 执行任务
        task->function(task->argument);
        free(task);
    }
    
    return NULL;
}

thread_pool_t* thread_pool_create(int num_threads) {
    thread_pool_t* pool = malloc(sizeof(thread_pool_t));
    
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    pool->task_queue = NULL;
    pool->num_threads = num_threads;
    pool->shutdown = false;
    
    pthread_mutex_init(&pool->queue_mutex, NULL);
    pthread_cond_init(&pool->queue_condition, NULL);
    
    // 创建工作线程
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&pool->threads[i], NULL, worker_thread, pool);
    }
    
    return pool;
}

void thread_pool_submit(thread_pool_t* pool, void (*function)(void*), void* arg) {
    task_t* task = malloc(sizeof(task_t));
    task->function = function;
    task->argument = arg;
    task->next = NULL;
    
    pthread_mutex_lock(&pool->queue_mutex);
    
    // 添加到队列尾部
    if (pool->task_queue == NULL) {
        pool->task_queue = task;
    } else {
        task_t* current = pool->task_queue;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = task;
    }
    
    pthread_cond_signal(&pool->queue_condition);
    pthread_mutex_unlock(&pool->queue_mutex);
}

void thread_pool_destroy(thread_pool_t* pool) {
    pthread_mutex_lock(&pool->queue_mutex);
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->queue_condition);
    pthread_mutex_unlock(&pool->queue_mutex);
    
    // 等待所有线程完成
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    // 清理剩余任务
    while (pool->task_queue != NULL) {
        task_t* task = pool->task_queue;
        pool->task_queue = task->next;
        free(task);
    }
    
    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->queue_condition);
    free(pool->threads);
    free(pool);
}

// 示例任务函数
void example_task(void* arg) {
    int task_id = *(int*)arg;
    printf("Executing task %d on thread %ld\n", task_id, pthread_self());
    usleep(100000 + (task_id % 5) * 50000);  // 模拟不同的工作时间
    printf("Task %d completed\n", task_id);
}

void thread_pool_example() {
    const int POOL_SIZE = 4;
    const int NUM_TASKS = 20;
    
    printf("Thread pool example with %d threads\n", POOL_SIZE);
    
    thread_pool_t* pool = thread_pool_create(POOL_SIZE);
    
    int* task_ids = malloc(NUM_TASKS * sizeof(int));
    
    // 提交任务
    for (int i = 0; i < NUM_TASKS; i++) {
        task_ids[i] = i;
        thread_pool_submit(pool, example_task, &task_ids[i]);
    }
    
    printf("Submitted %d tasks\n", NUM_TASKS);
    
    // 等待一段时间让任务完成
    sleep(5);
    
    thread_pool_destroy(pool);
    free(task_ids);
    
    printf("Thread pool example completed\n");
}
```

### 7.2 无锁编程示例

```c
#include <stdatomic.h>

// 无锁队列节点
typedef struct queue_node {
    atomic_intptr_t data;
    atomic(struct queue_node*) next;
} queue_node_t;

// 无锁队列
typedef struct {
    atomic(queue_node_t*) head;
    atomic(queue_node_t*) tail;
} lockfree_queue_t;

void lockfree_queue_init(lockfree_queue_t* queue) {
    queue_node_t* dummy = malloc(sizeof(queue_node_t));
    atomic_store(&dummy->data, 0);
    atomic_store(&dummy->next, NULL);
    
    atomic_store(&queue->head, dummy);
    atomic_store(&queue->tail, dummy);
}

void lockfree_queue_enqueue(lockfree_queue_t* queue, intptr_t data) {
    queue_node_t* new_node = malloc(sizeof(queue_node_t));
    atomic_store(&new_node->data, data);
    atomic_store(&new_node->next, NULL);
    
    while (1) {
        queue_node_t* last = atomic_load(&queue->tail);
        queue_node_t* next = atomic_load(&last->next);
        
        if (last == atomic_load(&queue->tail)) {
            if (next == NULL) {
                if (atomic_compare_exchange_weak(&last->next, &next, new_node)) {
                    break;
                }
            } else {
                atomic_compare_exchange_weak(&queue->tail, &last, next);
            }
        }
    }
    
    atomic_compare_exchange_weak(&queue->tail, &atomic_load(&queue->tail), new_node);
}

bool lockfree_queue_dequeue(lockfree_queue_t* queue, intptr_t* data) {
    while (1) {
        queue_node_t* first = atomic_load(&queue->head);
        queue_node_t* last = atomic_load(&queue->tail);
        queue_node_t* next = atomic_load(&first->next);
        
        if (first == atomic_load(&queue->head)) {
            if (first == last) {
                if (next == NULL) {
                    return false;  // 队列为空
                }
                atomic_compare_exchange_weak(&queue->tail, &last, next);
            } else {
                *data = atomic_load(&next->data);
                if (atomic_compare_exchange_weak(&queue->head, &first, next)) {
                    free(first);
                    return true;
                }
            }
        }
    }
}

lockfree_queue_t global_queue;
atomic_int producer_count = ATOMIC_VAR_INIT(0);
atomic_int consumer_count = ATOMIC_VAR_INIT(0);

void* lockfree_producer(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < 1000; i++) {
        intptr_t data = thread_id * 1000 + i;
        lockfree_queue_enqueue(&global_queue, data);
        atomic_fetch_add(&producer_count, 1);
        
        if (i % 200 == 0) {
            printf("Producer %d: enqueued %d items\n", thread_id, i + 1);
        }
    }
    
    printf("Producer %d completed\n", thread_id);
    return NULL;
}

void* lockfree_consumer(void* arg) {
    int thread_id = *(int*)arg;
    int consumed = 0;
    
    while (consumed < 500) {  // 每个消费者尝试消费500个元素
        intptr_t data;
        if (lockfree_queue_dequeue(&global_queue, &data)) {
            consumed++;
            atomic_fetch_add(&consumer_count, 1);
            
            if (consumed % 100 == 0) {
                printf("Consumer %d: dequeued %ld (total: %d)\n", 
                       thread_id, data, consumed);
            }
        } else {
            usleep(1000);  // 队列空，短暂等待
        }
    }
    
    printf("Consumer %d completed (consumed %d items)\n", thread_id, consumed);
    return NULL;
}

void lockfree_queue_example() {
    const int NUM_PRODUCERS = 2;
    const int NUM_CONSUMERS = 4;
    
    printf("Lock-free queue example\n");
    
    lockfree_queue_init(&global_queue);
    
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
    int producer_ids[NUM_PRODUCERS];
    int consumer_ids[NUM_CONSUMERS];
    
    // 创建生产者线程
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producer_ids[i] = i;
        pthread_create(&producers[i], NULL, lockfree_producer, &producer_ids[i]);
    }
    
    // 创建消费者线程
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumer_ids[i] = i;
        pthread_create(&consumers[i], NULL, lockfree_consumer, &consumer_ids[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }
    
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumers[i], NULL);
    }
    
    printf("Lock-free queue example completed\n");
    printf("Total produced: %d\n", atomic_load(&producer_count));
    printf("Total consumed: %d\n", atomic_load(&consumer_count));
}
```

### 7.3 性能优化技术

```c
#include <sched.h>

// 缓存行对齐避免伪共享
typedef struct {
    volatile long counter;
    char padding[64 - sizeof(long)];  // 缓存行填充
} aligned_counter_t;

aligned_counter_t counters[16] __attribute__((aligned(64)));

void* cache_aligned_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    // 每个线程使用自己的计数器，避免伪共享
    for (int i = 0; i < 1000000; i++) {
        counters[thread_id].counter++;
    }
    
    printf("Thread %d: final counter = %ld\n", 
           thread_id, counters[thread_id].counter);
    
    return NULL;
}

void cache_alignment_example() {
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    printf("Cache alignment example\n");
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, cache_aligned_worker, &thread_ids[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("Cache-aligned operations completed in %.3f seconds\n", elapsed);
}

// CPU亲和性设置
void* cpu_affinity_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    // 设置CPU亲和性
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread_id % sysconf(_SC_NPROCESSORS_ONLN), &cpuset);
    
    int result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        printf("Thread %d: Failed to set CPU affinity\n", thread_id);
    } else {
        printf("Thread %d: Set to CPU %d\n", thread_id, 
               thread_id % sysconf(_SC_NPROCESSORS_ONLN));
    }
    
    // 执行计算密集型工作
    double result_sum = 0.0;
    for (int i = 0; i < 1000000; i++) {
        result_sum += sin(i * 0.001) * cos(i * 0.001);
    }
    
    printf("Thread %d: computation result = %.6f\n", thread_id, result_sum);
    
    return NULL;
}

void cpu_affinity_example() {
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    printf("CPU affinity example (available CPUs: %d)\n", num_cpus);
    
    const int NUM_THREADS = num_cpus;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, cpu_affinity_worker, &thread_ids[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("CPU affinity example completed\n");
}
```

## 8. 调试和性能分析

### 8.1 死锁预防

```c
// 锁排序预防死锁
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

void lock_in_order(pthread_mutex_t* first, pthread_mutex_t* second) {
    // 始终按照地址顺序获取锁
    if (first < second) {
        pthread_mutex_lock(first);
        pthread_mutex_lock(second);
    } else {
        pthread_mutex_lock(second);
        pthread_mutex_lock(first);
    }
}

void unlock_in_order(pthread_mutex_t* first, pthread_mutex_t* second) {
    // 释放锁的顺序无关紧要，但保持一致性
    pthread_mutex_unlock(first);
    pthread_mutex_unlock(second);
}

void* safe_thread_a(void* arg) {
    printf("Thread A: acquiring locks\n");
    
    lock_in_order(&mutex1, &mutex2);
    
    printf("Thread A: got both locks, working...\n");
    usleep(100000);
    
    unlock_in_order(&mutex1, &mutex2);
    
    printf("Thread A: released locks\n");
    return NULL;
}

void* safe_thread_b(void* arg) {
    printf("Thread B: acquiring locks\n");
    
    lock_in_order(&mutex1, &mutex2);
    
    printf("Thread B: got both locks, working...\n");
    usleep(100000);
    
    unlock_in_order(&mutex1, &mutex2);
    
    printf("Thread B: released locks\n");
    return NULL;
}

void deadlock_prevention_example() {
    pthread_t thread_a, thread_b;
    
    printf("Deadlock prevention example\n");
    
    pthread_create(&thread_a, NULL, safe_thread_a, NULL);
    pthread_create(&thread_b, NULL, safe_thread_b, NULL);
    
    pthread_join(thread_a, NULL);
    pthread_join(thread_b, NULL);
    
    printf("Deadlock prevention example completed safely\n");
}
```

### 8.2 竞态条件检测

```c
volatile int race_counter = 0;
pthread_mutex_t race_mutex = PTHREAD_MUTEX_INITIALIZER;

void* race_condition_thread(void* arg) {
    int thread_id = *(int*)arg;
    bool use_mutex = (thread_id % 2 == 0);
    
    for (int i = 0; i < 100000; i++) {
        if (use_mutex) {
            pthread_mutex_lock(&race_mutex);
        }
        
        // 潜在的竞态条件
        int temp = race_counter;
        usleep(1);  // 增加竞态条件发生的概率
        race_counter = temp + 1;
        
        if (use_mutex) {
            pthread_mutex_unlock(&race_mutex);
        }
    }
    
    printf("Thread %d (mutex: %s) completed\n", 
           thread_id, use_mutex ? "yes" : "no");
    return NULL;
}

void race_condition_demo() {
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    printf("Race condition demonstration\n");
    printf("Threads 0,2 use mutex, threads 1,3 don't\n");
    
    race_counter = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, race_condition_thread, &thread_ids[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Final counter: %d (expected: %d)\n", 
           race_counter, NUM_THREADS * 100000);
    
    if (race_counter != NUM_THREADS * 100000) {
        printf("Race condition detected!\n");
    } else {
        printf("No race condition detected (lucky run)\n");
    }
}
```

### 8.3 性能基准测试

```c
#include <sys/time.h>

double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void benchmark_synchronization() {
    const int iterations = 1000000;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_spinlock_t spinlock;
    atomic_int atomic_var = ATOMIC_VAR_INIT(0);
    
    pthread_spin_init(&spinlock, PTHREAD_PROCESS_PRIVATE);
    
    printf("Synchronization benchmarks (%d iterations)\n", iterations);
    
    // 测试互斥锁性能
    double start = get_time();
    for (int i = 0; i < iterations; i++) {
        pthread_mutex_lock(&mutex);
        pthread_mutex_unlock(&mutex);
    }
    double mutex_time = get_time() - start;
    
    // 测试自旋锁性能
    start = get_time();
    for (int i = 0; i < iterations; i++) {
        pthread_spin_lock(&spinlock);
        pthread_spin_unlock(&spinlock);
    }
    double spin_time = get_time() - start;
    
    // 测试原子操作性能
    start = get_time();
    for (int i = 0; i < iterations; i++) {
        atomic_fetch_add(&atomic_var, 1);
    }
    double atomic_time = get_time() - start;
    
    printf("Mutex time:    %.3f seconds (%.0f ops/sec)\n", 
           mutex_time, iterations / mutex_time);
    printf("Spinlock time: %.3f seconds (%.0f ops/sec)\n", 
           spin_time, iterations / spin_time);
    printf("Atomic time:   %.3f seconds (%.0f ops/sec)\n", 
           atomic_time, iterations / atomic_time);
    
    printf("Speedup - Spinlock vs Mutex: %.2fx\n", mutex_time / spin_time);
    printf("Speedup - Atomic vs Mutex:   %.2fx\n", mutex_time / atomic_time);
    
    pthread_mutex_destroy(&mutex);
    pthread_spin_destroy(&spinlock);
}
```

## 9. 最佳实践

### 9.1 设计原则

1. **最小化锁的粒度**：使用细粒度锁减少竞争
2. **避免锁嵌套**：按固定顺序获取多个锁
3. **使用适当的同步原语**：选择最适合场景的同步机制
4. **考虑缓存友好性**：避免伪共享，使用缓存行对齐

### 9.2 常见错误

1. **忘记初始化同步对象**
2. **不匹配的lock/unlock调用**
3. **竞态条件**
4. **死锁**
5. **忘记加入线程（资源泄漏）**
6. **在信号处理程序中使用非异步信号安全函数**

### 9.3 调试工具

- **Valgrind/Helgrind**：检测竞态条件和死锁
- **ThreadSanitizer**：检测数据竞争
- **GDB**：多线程调试
- **Intel Inspector**：线程和内存错误检测

### 9.4 性能优化提示

1. 使用原子操作替代简单的锁
2. 考虑无锁数据结构
3. 设置CPU亲和性
4. 避免频繁的线程创建/销毁
5. 使用线程池
6. 优化数据局部性

## 10. 总结

Pthread提供了完整的多线程编程解决方案，包括：

- **线程管理**：创建、连接、分离线程
- **同步机制**：互斥锁、条件变量、读写锁、屏障、自旋锁
- **线程本地存储**：线程特定数据
- **原子操作**：无锁编程支持
- **高级特性**：线程池、无锁数据结构

正确使用Pthread可以显著提升程序性能，但需要注意避免常见的并发编程陷阱如死锁、竞态条件等。通过遵循最佳实践和使用适当的调试工具，可以开发出高效、可靠的多线程应用程序。