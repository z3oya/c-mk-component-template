# THREAD_SAFE_QUEUE 模块设计

## 1. 对外数据结构

### `queue_t`

用途：线程安全环形消息队列的控制块。实例本身的存储由调用者提供（栈或静态），内部缓冲区由模块分配。

```c
typedef struct {
    void *buffer;           // 缓冲区指针
    size_t item_size;       // 单个消息大小
    int capacity;           // 总容量
    int count;              // 当前消息数量
    int head;               // 弹出位置
    int tail;               // 写入位置
    int shutting_down;      // 标记队列是否处于销毁状态

    pthread_mutex_t lock;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} queue_t;
```

| 字段 | 单位/取值范围 | 说明 |
|------|---------------|------|
| buffer | — | 环形缓冲区，newqueue 分配，delequeue 释放并置 NULL |
| item_size | 字节，> 0 | 单条消息大小 |
| capacity | 条，> 0 | 队列总容量 |
| count | 0 .. capacity | 当前消息数 |
| head | 0 .. capacity-1 | 出队位置 |
| tail | 0 .. capacity-1 | 入队位置 |
| shutting_down | 0 / 1 | 关闭标记，delequeue 置位 |
| lock | — | 内部互斥锁 |
| not_full | — | “非满”条件变量 |
| not_empty | — | “非空”条件变量 |

除通过 API 访问外，调用者不得直接读写上述字段。

## 2. 对外接口一览表

| 签名 | 说明 |
|------|------|
| int newqueue(queue_t *q, size_t item_size, int num) | 初始化队列 |
| int delequeue(queue_t *q) | 销毁队列，唤醒所有等待线程 |
| int enqueue(queue_t *q, void *data, int timeout) | 入队一条消息 |
| int dequeue(queue_t *q, void *out_data, int timeout) | 出队一条消息 |

## 3. 对外接口详情

### 通用返回码

| 返回值 | 含义 | 产生的接口 |
|--------|------|------------|
| 0 | 成功 | 全部 |
| -1 | 参数非法；或队列满（空）且 timeout == 0 | 全部 |
| -2 | 等待超时 | enqueue / dequeue |
| -3 | 队列正在关闭（shutting_down 已置位） | enqueue / dequeue |
| -errno | pthread 原语错误（取错误码负值） | enqueue / dequeue |

### `newqueue`

- 参数检查条件：`q == NULL`、`item_size == 0`、`num <= 0` 返回 -1；`item_size * num` 溢出 SIZE_MAX 返回 -1；malloc 失败或 pthread 初始化失败返回 -1，失败时回滚已创建的资源（释放缓冲区、销毁已初始化的锁/条件变量）。
- 数据生命周期：内部缓冲区由模块 malloc；`queue_t` 实例本身的存储由调用者提供；缓冲区在 delequeue 中释放。实例初始化完成前不得并发使用。
- 输入输出行为：成功返回 0；队列初始为空、容量为 num，可立即 enqueue/dequeue。可能的返回值：0、-1。
- 线程安全：单次初始化，调用者须保证初始化完成后才将其交给其他线程使用。

### `delequeue`

- 参数检查条件：`q == NULL` 返回 -1；无其他校验。可能的返回值：0、-1。
- 数据生命周期：持锁置位 shutting_down，广播 not_full / not_empty 唤醒所有等待者；随后释放缓冲区并置 NULL，销毁互斥锁与条件变量。调用返回后队列不可再用，缓冲区内存已释放，调用者无需再释放。
- 输入输出行为：返回 0。
- 使用约束：调用者须确保被唤醒的等待线程返回 -3 后不再以任何方式访问该队列；delequeue 返回后对队列的任何访问（含再次 delequeue 以外的一切调用）均为未定义行为。
- 线程安全：与 enqueue / dequeue 通过内部锁互斥。

### `enqueue`

- 参数检查条件：`q == NULL` 或 `data == NULL` 返回 -1。
- 数据生命周期：data 由调用者提供，接口按 item_size 字节 memcpy 入内部缓冲区，返回后调用者可立即复用该内存；队列不持有指向调用者数据的指针。
- 输入输出行为（timeout 单位毫秒，< -1 视为 -1）：
  - 有空位：写入 tail 位置，tail 环形前移，count 加 1，signal not_empty，返回 0。
  - 队列满：进入等待循环，**每轮先查 shutting_down 再查 timeout**，因此满且已关闭时无论 timeout 取值均返回 -3：
    - shutting_down 已置位 → 返回 -3；
    - timeout == 0 → 不等待，返回 -1；
    - timeout > 0 → 等待至多 timeout 毫秒，超时返回 -2；
    - timeout < 0 → 永久阻塞，直至有空位或队列关闭。
- 线程安全：多生产者并发安全（内部锁）。

### `dequeue`

- 参数检查条件：`q == NULL` 或 `out_data == NULL` 返回 -1。
- 数据生命周期：out_data 由调用者提供，容量须不小于 item_size 字节；接口从内部缓冲区 memcpy 出队消息。
- 输入输出行为（timeout 语义同 enqueue）：
  - 有消息：读取 head 位置，head 环形前移，count 减 1，signal not_full，返回 0。
  - 队列空：每轮先查 shutting_down 再查 timeout（语义同上：关闭 → -3；timeout == 0 → -1；timeout > 0 超时 → -2；timeout < 0 永久阻塞）。
  - 防御检查：等待成功获得消息后若发现 buffer 已为 NULL，返回 -3。
- 线程安全：多消费者并发安全（内部锁）。

## 4. 典型用例

完整生产者/消费者示例见 `libs/queue/testqueue.c`。最小用法：

```c
int item = 42, out;
queue_t q;

if (newqueue(&q, sizeof(int), 4) != 0) { /* 处理创建失败 */ }

enqueue(&q, &item, 100);   /* 最多等待 100 ms */
dequeue(&q, &out, -1);     /* 永久阻塞直至有消息 */

delequeue(&q);
```

## 5. 依赖库

- pthread
- libc
