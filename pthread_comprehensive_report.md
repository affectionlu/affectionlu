# Pthread多线程编程全面技术报告

## 1. 概述与基础

### 1.1 Pthread简介
POSIX线程（Pthread）是IEEE POSIX 1003.1c标准定义的线程API，是在Unix和类Unix系统上实现多线程编程的标准接口。

```c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// 基本线程示例
void* thread_function(void* arg) {
    int thread_id = *(int*)arg;
    printf("Thread %d is running\n", thread_id);
    return NULL;
}

int main() {
    pthread_t thread;
    int thread_id = 1;
    
    pthread_create(&thread, NULL, thread_function, &thread_id);
    pthread_join(thread, NULL);
    
    printf("Main thread finished\n");
    return 0;
}
```

### 1.2 线程模型与特点
- **轻量级进程**：线程共享进程地址空间
- **并发执行**：多线程可在多核系统上并行运行
- **资源共享**：共享代码段、数据段、文件描述符
- **独立栈空间**：每个线程有独立的栈和寄存器

## 2. 线程管理

### 2.1 线程创建和终止

#### 2.1.1 pthread_create
```c
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg);

// 详细示例
typedef struct {
    int id;
    char name[64];
    double data[1000];
} thread_data_t;

void* worker_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    printf("Worker thread %d (%s) starting\n", data->id, data->name);
    
    // 执行工作
    for (int i = 0; i < 1000; i++) {
        data->data[i] = i * data->id;
    }
    
    printf("Worker thread %d completed\n", data->id);
    return (void*)(long)data->id;  // 返回线程ID
}

int main() {
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    
    // 创建多个线程
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].id = i;
        snprintf(thread_data[i].name, sizeof(thread_data[i].name), "Worker_%d", i);
        
        int result = pthread_create(&threads[i], NULL, worker_thread, &thread_data[i]);
        if (result != 0) {
            perror("pthread_create failed");
            exit(1);
        }
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        void* return_value;
        pthread_join(threads[i], &return_value);
        printf("Thread %d returned: %ld\n", i, (long)return_value);
    }
    
    return 0;
}
```

#### 2.1.2 线程终止方式
```c
// 1. 从线程函数返回
void* thread_return(void* arg) {
    return (void*)42;  // 正常返回
}

// 2. 调用pthread_exit
void* thread_exit(void* arg) {
    pthread_exit((void*)42);  // 显式退出
}

// 3. 被其他线程取消
void* thread_cancelable(void* arg) {
    while (1) {
        pthread_testcancel();  // 设置取消点
        // 执行工作
        usleep(100000);
    }
    return NULL;
}

// 4. 整个进程退出（所有线程终止）
```

### 2.2 线程属性

#### 2.2.1 线程属性设置
```c
#include <pthread.h>
#include <sys/resource.h>

void create_custom_thread() {
    pthread_t thread;
    pthread_attr_t attr;
    
    // 初始化属性
    pthread_attr_init(&attr);
    
    // 设置分离状态
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    // 设置栈大小
    size_t stack_size = 1024 * 1024;  // 1MB
    pthread_attr_setstacksize(&attr, stack_size);
    
    // 设置调度策略
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    
    // 设置调度参数
    struct sched_param param;
    param.sched_priority = 50;
    pthread_attr_setschedparam(&attr, &param);
    
    // 设置继承调度
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    
    // 设置作用域
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    
    // 创建线程
    pthread_create(&thread, &attr, worker_function, NULL);
    
    // 清理属性
    pthread_attr_destroy(&attr);
}
```

#### 2.2.2 栈保护和栈地址
```c
void stack_management_example() {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    
    // 自定义栈地址
    void* stack_addr = malloc(1024 * 1024);
    pthread_attr_setstack(&attr, stack_addr, 1024 * 1024);
    
    // 设置栈保护大小
    pthread_attr_setguardsize(&attr, 4096);
    
    pthread_t thread;
    pthread_create(&thread, &attr, worker_function, NULL);
    
    pthread_join(thread, NULL);
    pthread_attr_destroy(&attr);
    free(stack_addr);
}
```

### 2.3 线程分离和连接

#### 2.3.1 分离线程
```c
// 创建分离线程
void* detached_worker(void* arg) {
    printf("Detached thread running\n");
    sleep(2);
    printf("Detached thread finishing\n");
    return NULL;
}

void create_detached_thread() {
    pthread_t thread;
    pthread_attr_t attr;
    
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    pthread_create(&thread, &attr, detached_worker, NULL);
    pthread_attr_destroy(&attr);
    
    // 不需要pthread_join，线程自动清理资源
}

// 运行时分离
void runtime_detach_example() {
    pthread_t thread;
    pthread_create(&thread, NULL, worker_function, NULL);
    
    // 分离线程
    pthread_detach(thread);
    
    // 现在不能调用pthread_join
}
```

## 3. 同步机制

### 3.1 互斥锁（Mutex）

#### 3.1.1 基本互斥锁
```c
#include <pthread.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int shared_counter = 0;

void* increment_counter(void* arg) {
    int iterations = *(int*)arg;
    
    for (int i = 0; i < iterations; i++) {
        pthread_mutex_lock(&mutex);
        
        // 临界区
        int temp = shared_counter;
        temp++;
        shared_counter = temp;
        
        pthread_mutex_unlock(&mutex);
    }
    
    return NULL;
}

// 动态初始化互斥锁
void dynamic_mutex_example() {
    pthread_mutex_t dynamic_mutex;
    pthread_mutex_init(&dynamic_mutex, NULL);
    
    // 使用互斥锁
    pthread_mutex_lock(&dynamic_mutex);
    // 临界区代码
    pthread_mutex_unlock(&dynamic_mutex);
    
    // 销毁互斥锁
    pthread_mutex_destroy(&dynamic_mutex);
}
```

#### 3.1.2 互斥锁类型
```c
// 设置互斥锁属性
void mutex_types_example() {
    pthread_mutex_t mutex;
    pthread_mutexattr_t attr;
    
    pthread_mutexattr_init(&attr);
    
    // 普通互斥锁（默认）
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
    
    // 递归互斥锁
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    
    // 错误检查互斥锁
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    
    pthread_mutex_init(&mutex, &attr);
    
    // 递归锁示例
    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) == 0) {
        pthread_mutex_init(&mutex, &attr);
        
        // 可以多次锁定
        pthread_mutex_lock(&mutex);
        pthread_mutex_lock(&mutex);  // 不会死锁
        
        // 必须相应次数解锁
        pthread_mutex_unlock(&mutex);
        pthread_mutex_unlock(&mutex);
    }
    
    pthread_mutex_destroy(&mutex);
    pthread_mutexattr_destroy(&attr);
}
```

#### 3.1.3 尝试锁和超时锁
```c
#include <errno.h>
#include <time.h>

void try_lock_example() {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    
    // 非阻塞尝试锁定
    int result = pthread_mutex_trylock(&mutex);
    if (result == 0) {
        printf("Lock acquired\n");
        // 临界区
        pthread_mutex_unlock(&mutex);
    } else if (result == EBUSY) {
        printf("Lock is busy\n");
    }
    
    // 带超时的锁定
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 1;  // 1秒超时
    
    result = pthread_mutex_timedlock(&mutex, &timeout);
    if (result == 0) {
        printf("Timed lock acquired\n");
        pthread_mutex_unlock(&mutex);
    } else if (result == ETIMEDOUT) {
        printf("Lock timed out\n");
    }
}
```

### 3.2 条件变量

#### 3.2.1 基本条件变量
```c
#include <pthread.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int data_ready;
    int data;
} shared_data_t;

shared_data_t shared = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .condition = PTHREAD_COND_INITIALIZER,
    .data_ready = 0,
    .data = 0
};

// 生产者线程
void* producer(void* arg) {
    for (int i = 0; i < 10; i++) {
        pthread_mutex_lock(&shared.mutex);
        
        // 生产数据
        shared.data = i;
        shared.data_ready = 1;
        
        printf("Produced: %d\n", i);
        
        // 通知消费者
        pthread_cond_signal(&shared.condition);
        
        pthread_mutex_unlock(&shared.mutex);
        
        usleep(100000);  // 模拟生产时间
    }
    
    return NULL;
}

// 消费者线程
void* consumer(void* arg) {
    while (1) {
        pthread_mutex_lock(&shared.mutex);
        
        // 等待数据就绪
        while (!shared.data_ready) {
            pthread_cond_wait(&shared.condition, &shared.mutex);
        }
        
        // 消费数据
        printf("Consumed: %d\n", shared.data);
        shared.data_ready = 0;
        
        pthread_mutex_unlock(&shared.mutex);
    }
    
    return NULL;
}
```

#### 3.2.2 广播和超时等待
```c
// 多消费者示例
pthread_cond_t broadcast_condition = PTHREAD_COND_INITIALIZER;
pthread_mutex_t broadcast_mutex = PTHREAD_MUTEX_INITIALIZER;
int broadcast_data = 0;
int consumers_waiting = 0;

void* broadcast_consumer(void* arg) {
    int consumer_id = *(int*)arg;
    
    pthread_mutex_lock(&broadcast_mutex);
    consumers_waiting++;
    
    printf("Consumer %d waiting\n", consumer_id);
    pthread_cond_wait(&broadcast_condition, &broadcast_mutex);
    
    printf("Consumer %d received data: %d\n", consumer_id, broadcast_data);
    consumers_waiting--;
    
    pthread_mutex_unlock(&broadcast_mutex);
    return NULL;
}

void* broadcast_producer(void* arg) {
    sleep(2);  // 等待消费者就绪
    
    pthread_mutex_lock(&broadcast_mutex);
    
    broadcast_data = 42;
    printf("Broadcasting data to %d consumers\n", consumers_waiting);
    
    // 唤醒所有等待的线程
    pthread_cond_broadcast(&broadcast_condition);
    
    pthread_mutex_unlock(&broadcast_mutex);
    return NULL;
}

// 带超时的条件等待
void timed_condition_wait_example() {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
    
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 5;  // 5秒超时
    
    pthread_mutex_lock(&mutex);
    
    int result = pthread_cond_timedwait(&condition, &mutex, &timeout);
    if (result == ETIMEDOUT) {
        printf("Condition wait timed out\n");
    }
    
    pthread_mutex_unlock(&mutex);
}
```

### 3.3 读写锁

#### 3.3.1 基本读写锁
```c
#include <pthread.h>

pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
int shared_data = 0;
int reader_count = 0;

// 读者线程
void* reader_thread(void* arg) {
    int reader_id = *(int*)arg;
    
    while (1) {
        pthread_rwlock_rdlock(&rwlock);
        
        // 读取共享数据
        printf("Reader %d read: %d\n", reader_id, shared_data);
        
        pthread_rwlock_unlock(&rwlock);
        
        usleep(500000);  // 休眠500ms
    }
    
    return NULL;
}

// 写者线程
void* writer_thread(void* arg) {
    int writer_id = *(int*)arg;
    
    while (1) {
        pthread_rwlock_wrlock(&rwlock);
        
        // 修改共享数据
        shared_data++;
        printf("Writer %d wrote: %d\n", writer_id, shared_data);
        
        pthread_rwlock_unlock(&rwlock);
        
        usleep(1000000);  // 休眠1s
    }
    
    return NULL;
}
```

#### 3.3.2 读写锁属性和尝试锁
```c
void rwlock_attributes_example() {
    pthread_rwlock_t rwlock;
    pthread_rwlockattr_t attr;
    
    pthread_rwlockattr_init(&attr);
    
    // 设置读写锁偏好
    pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_READER_NP);
    // 或者 PTHREAD_RWLOCK_PREFER_WRITER_NP
    // 或者 PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP
    
    pthread_rwlock_init(&rwlock, &attr);
    
    // 尝试锁定（非阻塞）
    int result = pthread_rwlock_tryrdlock(&rwlock);
    if (result == 0) {
        printf("Read lock acquired\n");
        pthread_rwlock_unlock(&rwlock);
    }
    
    result = pthread_rwlock_trywrlock(&rwlock);
    if (result == 0) {
        printf("Write lock acquired\n");
        pthread_rwlock_unlock(&rwlock);
    }
    
    // 带超时的锁定
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 1;
    
    result = pthread_rwlock_timedrdlock(&rwlock, &timeout);
    if (result == 0) {
        pthread_rwlock_unlock(&rwlock);
    }
    
    pthread_rwlock_destroy(&rwlock);
    pthread_rwlockattr_destroy(&attr);
}
```

### 3.4 屏障

#### 3.4.1 基本屏障
```c
#include <pthread.h>

pthread_barrier_t barrier;
int num_threads = 4;

void* barrier_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    // 第一阶段工作
    printf("Thread %d: Phase 1 starting\n", thread_id);
    sleep(thread_id + 1);  // 模拟不同的工作时间
    printf("Thread %d: Phase 1 completed\n", thread_id);
    
    // 等待所有线程完成第一阶段
    int result = pthread_barrier_wait(&barrier);
    if (result == PTHREAD_BARRIER_SERIAL_THREAD) {
        printf("Thread %d: I'm the serial thread!\n", thread_id);
    }
    
    // 第二阶段工作
    printf("Thread %d: Phase 2 starting\n", thread_id);
    sleep(1);
    printf("Thread %d: Phase 2 completed\n", thread_id);
    
    return NULL;
}

void barrier_example() {
    pthread_t threads[4];
    int thread_ids[4];
    
    // 初始化屏障
    pthread_barrier_init(&barrier, NULL, num_threads);
    
    // 创建线程
    for (int i = 0; i < num_threads; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, barrier_worker, &thread_ids[i]);
    }
    
    // 等待线程完成
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 销毁屏障
    pthread_barrier_destroy(&barrier);
}
```

### 3.5 自旋锁

#### 3.5.1 基本自旋锁
```c
#include <pthread.h>

pthread_spinlock_t spinlock;

void* spin_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < 1000000; i++) {
        pthread_spin_lock(&spinlock);
        
        // 极短的临界区
        volatile int temp = i;
        temp++;
        
        pthread_spin_unlock(&spinlock);
    }
    
    printf("Thread %d completed\n", thread_id);
    return NULL;
}

void spinlock_example() {
    // 初始化自旋锁
    pthread_spin_init(&spinlock, PTHREAD_PROCESS_PRIVATE);
    
    pthread_t threads[2];
    int thread_ids[2] = {1, 2};
    
    for (int i = 0; i < 2; i++) {
        pthread_create(&threads[i], NULL, spin_worker, &thread_ids[i]);
    }
    
    for (int i = 0; i < 2; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 销毁自旋锁
    pthread_spin_destroy(&spinlock);
}
```

## 4. 线程本地存储

### 4.1 Thread-Local Storage (TLS)

#### 4.1.1 __thread关键字
```c
#include <pthread.h>
#include <stdio.h>

// 线程本地存储变量
__thread int thread_local_counter = 0;
__thread char thread_local_buffer[1024];

void* tls_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    // 每个线程都有自己的副本
    thread_local_counter = thread_id * 100;
    snprintf(thread_local_buffer, sizeof(thread_local_buffer), 
             "Thread %d buffer", thread_id);
    
    for (int i = 0; i < 5; i++) {
        thread_local_counter++;
        printf("Thread %d: counter = %d, buffer = %s\n", 
               thread_id, thread_local_counter, thread_local_buffer);
        usleep(100000);
    }
    
    return NULL;
}
```

#### 4.1.2 pthread_key机制
```c
#include <pthread.h>

pthread_key_t thread_data_key;

typedef struct {
    int id;
    char name[64];
    double value;
} thread_specific_data_t;

// 清理函数
void cleanup_thread_data(void* data) {
    if (data) {
        printf("Cleaning up thread data\n");
        free(data);
    }
}

void* key_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    // 为当前线程分配专用数据
    thread_specific_data_t* data = malloc(sizeof(thread_specific_data_t));
    data->id = thread_id;
    snprintf(data->name, sizeof(data->name), "Thread_%d", thread_id);
    data->value = thread_id * 3.14;
    
    // 关联数据到键
    pthread_setspecific(thread_data_key, data);
    
    // 使用线程专用数据
    for (int i = 0; i < 3; i++) {
        thread_specific_data_t* my_data = 
            (thread_specific_data_t*)pthread_getspecific(thread_data_key);
        
        printf("Thread %d: id=%d, name=%s, value=%.2f\n",
               thread_id, my_data->id, my_data->name, my_data->value);
        
        my_data->value += 1.0;
        usleep(200000);
    }
    
    return NULL;
}

void pthread_key_example() {
    // 创建键
    pthread_key_create(&thread_data_key, cleanup_thread_data);
    
    pthread_t threads[3];
    int thread_ids[3] = {1, 2, 3};
    
    for (int i = 0; i < 3; i++) {
        pthread_create(&threads[i], NULL, key_worker, &thread_ids[i]);
    }
    
    for (int i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 销毁键
    pthread_key_delete(thread_data_key);
}
```

## 5. 线程取消

### 5.1 线程取消机制

#### 5.1.1 基本取消操作
```c
#include <pthread.h>

void* cancelable_worker(void* arg) {
    int thread_id = *(int*)arg;
    int old_state, old_type;
    
    // 设置可取消状态
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
    
    // 设置取消类型
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &old_type);
    
    printf("Thread %d: Starting work\n", thread_id);
    
    for (int i = 0; i < 100; i++) {
        // 设置取消点
        pthread_testcancel();
        
        printf("Thread %d: Working... %d\n", thread_id, i);
        usleep(100000);
        
        // 另一个取消点
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
    
    pthread_create(&thread, NULL, cancelable_worker, &thread_id);
    
    // 让线程运行一段时间
    sleep(2);
    
    // 取消线程
    printf("Main: Canceling thread\n");
    pthread_cancel(thread);
    
    // 等待线程结束
    void* result;
    pthread_join(thread, &result);
    
    if (result == PTHREAD_CANCELED) {
        printf("Thread was canceled\n");
    } else {
        printf("Thread completed normally\n");
    }
}
```

#### 5.1.2 清理处理程序
```c
#include <pthread.h>

typedef struct {
    FILE* file;
    pthread_mutex_t* mutex;
    char* buffer;
} cleanup_data_t;

// 清理处理程序
void cleanup_handler(void* arg) {
    cleanup_data_t* data = (cleanup_data_t*)arg;
    
    printf("Cleanup handler called\n");
    
    if (data->file) {
        fclose(data->file);
        printf("File closed\n");
    }
    
    if (data->mutex) {
        pthread_mutex_unlock(data->mutex);
        printf("Mutex unlocked\n");
    }
    
    if (data->buffer) {
        free(data->buffer);
        printf("Buffer freed\n");
    }
}

void* cleanup_worker(void* arg) {
    cleanup_data_t cleanup_data;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    
    // 初始化清理数据
    cleanup_data.file = fopen("/tmp/test.txt", "w");
    cleanup_data.mutex = &mutex;
    cleanup_data.buffer = malloc(1024);
    
    // 注册清理处理程序
    pthread_cleanup_push(cleanup_handler, &cleanup_data);
    
    pthread_mutex_lock(&mutex);
    
    // 模拟可能被取消的工作
    for (int i = 0; i < 50; i++) {
        fprintf(cleanup_data.file, "Line %d\n", i);
        fflush(cleanup_data.file);
        
        pthread_testcancel();  // 取消点
        usleep(100000);
    }
    
    pthread_mutex_unlock(&mutex);
    
    // 弹出清理处理程序（不执行）
    pthread_cleanup_pop(0);
    
    // 手动清理
    cleanup_handler(&cleanup_data);
    
    return NULL;
}
```

## 6. 原子操作

### 6.1 GCC内置原子操作

#### 6.1.1 基本原子操作
```c
#include <pthread.h>
#include <stdatomic.h>

// 使用GCC内置原子操作
volatile int atomic_counter = 0;
volatile long atomic_sum = 0;

void* atomic_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < 1000000; i++) {
        // 原子增加
        __sync_fetch_and_add(&atomic_counter, 1);
        
        // 原子累加
        __sync_fetch_and_add(&atomic_sum, i);
        
        // 原子比较和交换
        int old_val = atomic_counter;
        if (old_val % 1000 == 0) {
            __sync_bool_compare_and_swap(&atomic_counter, old_val, old_val + 100);
        }
    }
    
    printf("Thread %d completed\n", thread_id);
    return NULL;
}

// C11原子操作
atomic_int c11_counter = ATOMIC_VAR_INIT(0);
atomic_long c11_sum = ATOMIC_VAR_INIT(0);

void* c11_atomic_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < 1000000; i++) {
        // C11原子操作
        atomic_fetch_add(&c11_counter, 1);
        atomic_fetch_add(&c11_sum, i);
        
        // 原子比较交换
        int expected = i;
        atomic_compare_exchange_weak(&c11_counter, &expected, i + 1);
    }
    
    printf("C11 Thread %d completed\n", thread_id);
    return NULL;
}
```

#### 6.1.2 内存序和原子操作
```c
#include <stdatomic.h>

atomic_int flag1 = ATOMIC_VAR_INIT(0);
atomic_int flag2 = ATOMIC_VAR_INIT(0);
atomic_int data = ATOMIC_VAR_INIT(0);

void* producer_thread(void* arg) {
    // 写入数据
    atomic_store_explicit(&data, 42, memory_order_relaxed);
    
    // 发布操作
    atomic_store_explicit(&flag1, 1, memory_order_release);
    
    return NULL;
}

void* consumer_thread(void* arg) {
    // 获取操作
    while (atomic_load_explicit(&flag1, memory_order_acquire) == 0) {
        usleep(1000);
    }
    
    // 现在可以安全读取数据
    int value = atomic_load_explicit(&data, memory_order_relaxed);
    printf("Consumer read: %d\n", value);
    
    return NULL;
}

// 更复杂的内存序示例
void memory_ordering_example() {
    atomic_int x = ATOMIC_VAR_INIT(0);
    atomic_int y = ATOMIC_VAR_INIT(0);
    
    // 顺序一致性
    atomic_store_explicit(&x, 1, memory_order_seq_cst);
    atomic_store_explicit(&y, 1, memory_order_seq_cst);
    
    // 获取-释放
    atomic_store_explicit(&x, 2, memory_order_release);
    int val = atomic_load_explicit(&x, memory_order_acquire);
    
    // 松散序
    atomic_store_explicit(&y, 2, memory_order_relaxed);
    val = atomic_load_explicit(&y, memory_order_relaxed);
}
```

## 7. 高级主题

### 7.1 线程池

#### 7.1.1 简单线程池实现
```c
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

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
    
    // 创建工作线程
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&pool->threads[i], NULL, worker_thread, pool);
    }
    
    return pool;
}

void* worker_thread(void* arg) {
    thread_pool_t* pool = (thread_pool_t*)arg;
    
    while (true) {
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

void thread_pool_add_task(thread_pool_t* pool, void (*function)(void*), void* arg) {
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
    
    // 等待所有线程结束
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
```

### 7.2 无锁编程

#### 7.2.1 无锁队列
```c
#include <stdatomic.h>

typedef struct node {
    atomic_intptr_t data;
    atomic(struct node*) next;
} node_t;

typedef struct {
    atomic(node_t*) head;
    atomic(node_t*) tail;
} lock_free_queue_t;

void queue_init(lock_free_queue_t* queue) {
    node_t* dummy = malloc(sizeof(node_t));
    atomic_init(&dummy->data, 0);
    atomic_init(&dummy->next, NULL);
    
    atomic_init(&queue->head, dummy);
    atomic_init(&queue->tail, dummy);
}

void queue_enqueue(lock_free_queue_t* queue, intptr_t data) {
    node_t* new_node = malloc(sizeof(node_t));
    atomic_init(&new_node->data, data);
    atomic_init(&new_node->next, NULL);
    
    while (true) {
        node_t* tail = atomic_load(&queue->tail);
        node_t* next = atomic_load(&tail->next);
        
        if (tail == atomic_load(&queue->tail)) {  // 尾部未改变
            if (next == NULL) {
                // 尝试链接新节点
                if (atomic_compare_exchange_weak(&tail->next, &next, new_node)) {
                    break;
                }
            } else {
                // 帮助推进尾指针
                atomic_compare_exchange_weak(&queue->tail, &tail, next);
            }
        }
    }
    
    // 推进尾指针
    node_t* tail = atomic_load(&queue->tail);
    atomic_compare_exchange_weak(&queue->tail, &tail, new_node);
}

bool queue_dequeue(lock_free_queue_t* queue, intptr_t* data) {
    while (true) {
        node_t* head = atomic_load(&queue->head);
        node_t* tail = atomic_load(&queue->tail);
        node_t* next = atomic_load(&head->next);
        
        if (head == atomic_load(&queue->head)) {  // 头部未改变
            if (head == tail) {
                if (next == NULL) {
                    return false;  // 队列为空
                }
                // 帮助推进尾指针
                atomic_compare_exchange_weak(&queue->tail, &tail, next);
            } else {
                if (next == NULL) {
                    continue;
                }
                
                // 读取数据
                *data = atomic_load(&next->data);
                
                // 推进头指针
                if (atomic_compare_exchange_weak(&queue->head, &head, next)) {
                    free(head);
                    return true;
                }
            }
        }
    }
}
```

### 7.3 性能优化

#### 7.3.1 缓存行对齐
```c
#include <pthread.h>

// 避免伪共享
typedef struct {
    volatile long counter;
    char padding[64 - sizeof(long)];  // 缓存行填充
} aligned_counter_t;

aligned_counter_t counters[16] __attribute__((aligned(64)));

void* optimized_worker(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < 1000000; i++) {
        counters[thread_id].counter++;
    }
    
    return NULL;
}

// CPU亲和性设置
#include <sched.h>

void set_thread_affinity() {
    cpu_set_t cpuset;
    pthread_t thread = pthread_self();
    
    CPU_ZERO(&cpuset);
    CPU_SET(2, &cpuset);  // 绑定到CPU 2
    
    int result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        perror("pthread_setaffinity_np");
    }
    
    // 获取当前亲和性
    CPU_ZERO(&cpuset);
    result = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (result == 0) {
        printf("Thread is running on CPU: ");
        for (int i = 0; i < CPU_SETSIZE; i++) {
            if (CPU_ISSET(i, &cpuset)) {
                printf("%d ", i);
            }
        }
        printf("\n");
    }
}
```

## 8. 调试和性能分析

### 8.1 常见问题和调试

#### 8.1.1 死锁检测和预防
```c
// 死锁示例
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

void* thread1_deadlock(void* arg) {
    pthread_mutex_lock(&mutex1);
    printf("Thread 1: Acquired mutex1\n");
    sleep(1);
    
    printf("Thread 1: Trying to acquire mutex2\n");
    pthread_mutex_lock(&mutex2);  // 可能死锁
    printf("Thread 1: Acquired mutex2\n");
    
    pthread_mutex_unlock(&mutex2);
    pthread_mutex_unlock(&mutex1);
    return NULL;
}

void* thread2_deadlock(void* arg) {
    pthread_mutex_lock(&mutex2);
    printf("Thread 2: Acquired mutex2\n");
    sleep(1);
    
    printf("Thread 2: Trying to acquire mutex1\n");
    pthread_mutex_lock(&mutex1);  // 可能死锁
    printf("Thread 2: Acquired mutex1\n");
    
    pthread_mutex_unlock(&mutex1);
    pthread_mutex_unlock(&mutex2);
    return NULL;
}

// 死锁预防：锁排序
void* thread1_safe(void* arg) {
    // 总是按相同顺序获取锁
    pthread_mutex_lock(&mutex1);
    pthread_mutex_lock(&mutex2);
    
    // 工作代码
    
    pthread_mutex_unlock(&mutex2);
    pthread_mutex_unlock(&mutex1);
    return NULL;
}

void* thread2_safe(void* arg) {
    // 总是按相同顺序获取锁
    pthread_mutex_lock(&mutex1);
    pthread_mutex_lock(&mutex2);
    
    // 工作代码
    
    pthread_mutex_unlock(&mutex2);
    pthread_mutex_unlock(&mutex1);
    return NULL;
}
```

#### 8.1.2 竞态条件检测
```c
// 竞态条件示例
int shared_variable = 0;

void* race_condition_thread(void* arg) {
    for (int i = 0; i < 1000000; i++) {
        shared_variable++;  // 非原子操作，存在竞态条件
    }
    return NULL;
}

// 修复竞态条件
pthread_mutex_t race_mutex = PTHREAD_MUTEX_INITIALIZER;

void* safe_thread(void* arg) {
    for (int i = 0; i < 1000000; i++) {
        pthread_mutex_lock(&race_mutex);
        shared_variable++;
        pthread_mutex_unlock(&race_mutex);
    }
    return NULL;
}

// 或使用原子操作
atomic_int atomic_variable = ATOMIC_VAR_INIT(0);

void* atomic_thread(void* arg) {
    for (int i = 0; i < 1000000; i++) {
        atomic_fetch_add(&atomic_variable, 1);
    }
    return NULL;
}
```

### 8.2 性能测量

#### 8.2.1 基准测试
```c
#include <time.h>
#include <sys/time.h>

double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void benchmark_mutex_vs_atomic() {
    const int iterations = 10000000;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    atomic_int atomic_counter = ATOMIC_VAR_INIT(0);
    int regular_counter = 0;
    
    // 测试互斥锁
    double start = get_time();
    for (int i = 0; i < iterations; i++) {
        pthread_mutex_lock(&mutex);
        regular_counter++;
        pthread_mutex_unlock(&mutex);
    }
    double mutex_time = get_time() - start;
    
    // 测试原子操作
    start = get_time();
    for (int i = 0; i < iterations; i++) {
        atomic_fetch_add(&atomic_counter, 1);
    }
    double atomic_time = get_time() - start;
    
    printf("Mutex time: %.3f seconds\n", mutex_time);
    printf("Atomic time: %.3f seconds\n", atomic_time);
    printf("Speedup: %.2fx\n", mutex_time / atomic_time);
}
```

## 9. 最佳实践

### 9.1 设计原则

1. **最小化锁的粒度**：只保护必要的临界区
2. **避免锁嵌套**：减少死锁风险
3. **使用适当的同步原语**：根据场景选择mutex、rwlock、atomic等
4. **考虑缓存友好性**：避免伪共享，使用缓存行对齐

### 9.2 常见陷阱

1. **忘记初始化同步对象**
2. **不匹配的lock/unlock调用**
3. **在signal handler中使用非异步安全函数**
4. **线程安全函数的误用**

### 9.3 调试工具

1. **Valgrind/Helgrind**：检测竞态条件和死锁
2. **ThreadSanitizer**：动态检测数据竞争
3. **GDB**：多线程调试支持
4. **perf**：性能分析工具

## 10. 总结

Pthread提供了完整的多线程编程解决方案，从基本的线程创建管理到高级的同步机制，再到性能优化技术。掌握这些知识点对于开发高性能、可靠的多线程应用程序至关重要。正确使用pthread可以充分利用多核处理器的性能，但也需要注意避免常见的并发编程陷阱。