# Pthread多线程编程完整技术报告

## 1. Pthread概述

POSIX线程（Pthread）是IEEE POSIX 1003.1c标准定义的线程API，是Unix和类Unix系统上多线程编程的标准接口。

```c
#include <pthread.h>
#include <stdio.h>

int main() {
    pthread_t thread;
    pthread_create(&thread, NULL, thread_function, NULL);
    pthread_join(thread, NULL);
    return 0;
}
```

## 2. 线程管理

### 2.1 线程创建和生命周期

```c
typedef struct {
    int id;
    double *data;
    int size;
} thread_data_t;

void* worker_thread(void* arg) {
    thread_data_t* tdata = (thread_data_t*)arg;
    
    for (int i = 0; i < tdata->size; i++) {
        tdata->data[i] = tdata->id * i;
    }
    
    return (void*)(long)tdata->id;
}

int main() {
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].id = i;
        thread_data[i].data = malloc(1000 * sizeof(double));
        thread_data[i].size = 1000;
        
        pthread_create(&threads[i], NULL, worker_thread, &thread_data[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        void* result;
        pthread_join(threads[i], &result);
        printf("Thread %d returned: %ld\n", i, (long)result);
        free(thread_data[i].data);
    }
    
    return 0;
}
```

### 2.2 线程属性管理

```c
void thread_attributes_example() {
    pthread_t thread;
    pthread_attr_t attr;
    
    pthread_attr_init(&attr);
    
    // 设置分离状态
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    // 设置栈大小
    size_t stack_size = 2 * 1024 * 1024;  // 2MB
    pthread_attr_setstacksize(&attr, stack_size);
    
    // 设置调度策略
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    
    struct sched_param param;
    param.sched_priority = 50;
    pthread_attr_setschedparam(&attr, &param);
    
    pthread_create(&thread, &attr, worker_function, NULL);
    pthread_attr_destroy(&attr);
}
```

## 3. 同步机制

### 3.1 互斥锁详解

```c
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int shared_counter = 0;

void* increment_thread(void* arg) {
    int iterations = *(int*)arg;
    
    for (int i = 0; i < iterations; i++) {
        pthread_mutex_lock(&mutex);
        shared_counter++;
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// 递归互斥锁示例
void recursive_mutex_example() {
    pthread_mutex_t recursive_mutex;
    pthread_mutexattr_t attr;
    
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&recursive_mutex, &attr);
    
    // 可以多次锁定
    pthread_mutex_lock(&recursive_mutex);
    pthread_mutex_lock(&recursive_mutex);
    
    pthread_mutex_unlock(&recursive_mutex);
    pthread_mutex_unlock(&recursive_mutex);
    
    pthread_mutex_destroy(&recursive_mutex);
    pthread_mutexattr_destroy(&attr);
}
```

### 3.2 条件变量详解

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
    for (int i = 0; i < 10; i++) {
        pthread_mutex_lock(&shared.mutex);
        
        shared.data = i;
        shared.data_ready = 1;
        
        printf("Producer: produced %d\n", i);
        pthread_cond_signal(&shared.condition);
        
        pthread_mutex_unlock(&shared.mutex);
        usleep(100000);
    }
    return NULL;
}

void* consumer(void* arg) {
    int consumer_id = *(int*)arg;
    
    while (1) {
        pthread_mutex_lock(&shared.mutex);
        
        shared.consumers_waiting++;
        while (!shared.data_ready) {
            pthread_cond_wait(&shared.condition, &shared.mutex);
        }
        shared.consumers_waiting--;
        
        printf("Consumer %d: consumed %d\n", consumer_id, shared.data);
        shared.data_ready = 0;
        
        pthread_mutex_unlock(&shared.mutex);
    }
    return NULL;
}
```

### 3.3 读写锁详解

```c
pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
int shared_data = 0;

void* reader_thread(void* arg) {
    int reader_id = *(int*)arg;
    
    for (int i = 0; i < 5; i++) {
        pthread_rwlock_rdlock(&rwlock);
        printf("Reader %d: read value %d\n", reader_id, shared_data);
        pthread_rwlock_unlock(&rwlock);
        usleep(200000);
    }
    return NULL;
}

void* writer_thread(void* arg) {
    int writer_id = *(int*)arg;
    
    for (int i = 0; i < 3; i++) {
        pthread_rwlock_wrlock(&rwlock);
        shared_data++;
        printf("Writer %d: wrote value %d\n", writer_id, shared_data);
        pthread_rwlock_unlock(&rwlock);
        usleep(500000);
    }
    return NULL;
}
```

### 3.4 屏障机制

```c
pthread_barrier_t barrier;

void* barrier_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    printf("Thread %d: Phase 1 starting\n", thread_id);
    usleep((thread_id + 1) * 100000);  // 模拟不同工作时间
    printf("Thread %d: Phase 1 completed\n", thread_id);
    
    int result = pthread_barrier_wait(&barrier);
    if (result == PTHREAD_BARRIER_SERIAL_THREAD) {
        printf("Thread %d: Serial thread at barrier\n", thread_id);
    }
    
    printf("Thread %d: Phase 2 starting\n", thread_id);
    return NULL;
}
```

## 4. 线程本地存储

### 4.1 __thread关键字

```c
__thread int thread_local_counter = 0;
__thread char thread_local_buffer[1024];

void* tls_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    thread_local_counter = thread_id * 100;
    snprintf(thread_local_buffer, sizeof(thread_local_buffer), 
             "Thread %d data", thread_id);
    
    for (int i = 0; i < 5; i++) {
        thread_local_counter++;
        printf("Thread %d: counter=%d, buffer=%s\n", 
               thread_id, thread_local_counter, thread_local_buffer);
    }
    
    return NULL;
}
```

### 4.2 pthread_key机制

```c
pthread_key_t thread_data_key;

typedef struct {
    int id;
    char name[64];
    double value;
} thread_specific_data_t;

void cleanup_thread_data(void* data) {
    if (data) {
        printf("Cleaning up thread data\n");
        free(data);
    }
}

void* key_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    thread_specific_data_t* data = malloc(sizeof(thread_specific_data_t));
    data->id = thread_id;
    snprintf(data->name, sizeof(data->name), "Thread_%d", thread_id);
    data->value = thread_id * 3.14;
    
    pthread_setspecific(thread_data_key, data);
    
    for (int i = 0; i < 3; i++) {
        thread_specific_data_t* my_data = 
            (thread_specific_data_t*)pthread_getspecific(thread_data_key);
        
        printf("Thread %d: value=%.2f\n", thread_id, my_data->value);
        my_data->value += 1.0;
    }
    
    return NULL;
}
```

## 5. 线程取消机制

### 5.1 基本取消操作

```c
void* cancelable_worker(void* arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    
    for (int i = 0; i < 100; i++) {
        pthread_testcancel();
        printf("Working... %d\n", i);
        usleep(100000);
    }
    
    return NULL;
}

void cancellation_example() {
    pthread_t thread;
    
    pthread_create(&thread, NULL, cancelable_worker, NULL);
    sleep(2);
    
    printf("Canceling thread\n");
    pthread_cancel(thread);
    
    void* result;
    pthread_join(thread, &result);
    
    if (result == PTHREAD_CANCELED) {
        printf("Thread was canceled\n");
    }
}
```

### 5.2 清理处理程序

```c
void cleanup_handler(void* arg) {
    printf("Cleanup handler called\n");
    if (arg) {
        free(arg);
    }
}

void* cleanup_worker(void* arg) {
    char* buffer = malloc(1024);
    
    pthread_cleanup_push(cleanup_handler, buffer);
    
    for (int i = 0; i < 50; i++) {
        pthread_testcancel();
        usleep(100000);
    }
    
    pthread_cleanup_pop(1);
    return NULL;
}
```

## 6. 原子操作

### 6.1 GCC内置原子操作

```c
volatile int atomic_counter = 0;

void* atomic_worker(void* arg) {
    for (int i = 0; i < 1000000; i++) {
        __sync_fetch_and_add(&atomic_counter, 1);
    }
    return NULL;
}

// C11原子操作
#include <stdatomic.h>

atomic_int c11_counter = ATOMIC_VAR_INIT(0);

void* c11_atomic_worker(void* arg) {
    for (int i = 0; i < 1000000; i++) {
        atomic_fetch_add(&c11_counter, 1);
    }
    return NULL;
}
```

## 7. 高级主题

### 7.1 线程池实现

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

thread_pool_t* thread_pool_create(int num_threads) {
    thread_pool_t* pool = malloc(sizeof(thread_pool_t));
    
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    pool->task_queue = NULL;
    pool->num_threads = num_threads;
    pool->shutdown = false;
    
    pthread_mutex_init(&pool->queue_mutex, NULL);
    pthread_cond_init(&pool->queue_condition, NULL);
    
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&pool->threads[i], NULL, worker_thread, pool);
    }
    
    return pool;
}

void* worker_thread(void* arg) {
    thread_pool_t* pool = (thread_pool_t*)arg;
    
    while (true) {
        pthread_mutex_lock(&pool->queue_mutex);
        
        while (pool->task_queue == NULL && !pool->shutdown) {
            pthread_cond_wait(&pool->queue_condition, &pool->queue_mutex);
        }
        
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->queue_mutex);
            break;
        }
        
        task_t* task = pool->task_queue;
        pool->task_queue = task->next;
        
        pthread_mutex_unlock(&pool->queue_mutex);
        
        task->function(task->argument);
        free(task);
    }
    
    return NULL;
}
```

### 7.2 性能优化

```c
// 缓存行对齐避免伪共享
typedef struct {
    volatile long counter;
    char padding[64 - sizeof(long)];
} aligned_counter_t;

aligned_counter_t counters[16] __attribute__((aligned(64)));

// CPU亲和性设置
#include <sched.h>

void set_thread_affinity() {
    cpu_set_t cpuset;
    pthread_t thread = pthread_self();
    
    CPU_ZERO(&cpuset);
    CPU_SET(2, &cpuset);
    
    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
}
```

## 8. 调试和性能分析

### 8.1 死锁预防

```c
// 锁排序预防死锁
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

void* safe_thread(void* arg) {
    // 总是按相同顺序获取锁
    pthread_mutex_lock(&mutex1);
    pthread_mutex_lock(&mutex2);
    
    // 临界区工作
    
    pthread_mutex_unlock(&mutex2);
    pthread_mutex_unlock(&mutex1);
    return NULL;
}
```

### 8.2 性能基准测试

```c
double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void benchmark_synchronization() {
    const int iterations = 1000000;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    atomic_int atomic_var = ATOMIC_VAR_INIT(0);
    
    // 测试互斥锁
    double start = get_time();
    for (int i = 0; i < iterations; i++) {
        pthread_mutex_lock(&mutex);
        pthread_mutex_unlock(&mutex);
    }
    double mutex_time = get_time() - start;
    
    // 测试原子操作
    start = get_time();
    for (int i = 0; i < iterations; i++) {
        atomic_fetch_add(&atomic_var, 1);
    }
    double atomic_time = get_time() - start;
    
    printf("Mutex time: %.3f seconds\n", mutex_time);
    printf("Atomic time: %.3f seconds\n", atomic_time);
}
```

## 9. 最佳实践

### 9.1 设计原则
1. 最小化锁的粒度
2. 避免锁嵌套
3. 使用适当的同步原语
4. 考虑缓存友好性

### 9.2 常见错误
1. 忘记初始化同步对象
2. 不匹配的lock/unlock调用
3. 竞态条件
4. 死锁

### 9.3 调试工具
- Valgrind/Helgrind
- ThreadSanitizer
- GDB多线程调试
- Intel Inspector

## 10. 总结

Pthread提供了完整的多线程编程解决方案，正确使用可以显著提升程序性能。关键是理解各种同步机制的适用场景，避免常见的并发编程陷阱。