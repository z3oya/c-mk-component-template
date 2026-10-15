# THREAD_SAFE_QUEUE 模块设计

## 1. 对外数据结构

enqueue 与 dequeue 的 timeout 参数单位为毫秒：等于 0 表示不等待；大于 0 表示至多等待该数值；小于 0 表示无限等待。

| 返回值 | 含义 |
|--------|------|
| 0 | 成功 |
| -1 | 参数非法；或 timeout 等于 0 而队列已满或已空 |
| -2 | 等待超时 |
| -3 | 队列正在关闭 |
| -errno | pthread 原语或缓冲区分配错误，取错误码负值 |

### `queue_t`

用途：线程安全环形消息队列的控制块。实例本身的存储由调用者提供，可为栈或静态存储；内部缓冲区由模块分配。

```c
typedef struct {
    void *buffer;
    size_t item_size;
    int capacity;
    int count;
    int head;
    int tail;
    int shutting_down;

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

## 2. 对外接口一览表

| 签名 | 说明 |
|------|------|
| int newqueue(queue_t *q, size_t item_size, int num) | 初始化队列 |
| int delequeue(queue_t *q) | 销毁队列 |
| int enqueue(queue_t *q, void *data, int timeout) | 入队一条消息 |
| int dequeue(queue_t *q, void *out_data, int timeout) | 出队一条消息 |

## 3. 对外接口详情

### `newqueue`

- 参数检查条件：`q == NULL`、`item_size == 0`、`num <= 0` 返回 -1；`item_size * num` 溢出 SIZE_MAX 返回 -1。
- 数据生命周期：`queue_t` 实例本身的存储由调用者提供；创建失败时模块释放全部已创建资源，调用者无需清理。
- 输入输出行为：成功返回 0，队列初始为空、容量为 num，可立即 enqueue 与 dequeue；缓冲区分配或 pthread 初始化失败返回 -errno。可能的返回值：0、-1、-errno。
- 线程安全：单次初始化，调用者须保证初始化完成后才将其交给其他线程使用。
- 使用约束：`queue_t` 的字段仅供模块内部访问，调用者不得直接读写。

### `delequeue`

- 参数检查条件：`q == NULL` 返回 -1；无其他校验。
- 数据生命周期：持锁置位 shutting_down，广播 not_full / not_empty 唤醒所有等待者，随后释放缓冲区并置 NULL，销毁互斥锁与条件变量；调用返回后无需再释放任何资源。
- 输入输出行为：成功返回 0。可能的返回值：0、-1。
- 线程安全：与 enqueue / dequeue 通过内部互斥锁互斥。
- 使用约束：调用者须确保被唤醒的等待线程返回 -3 后不再以任何方式访问该队列；delequeue 返回后对队列的任何访问均为未定义行为。

### `enqueue`

- 参数检查条件：`q == NULL` 或 `data == NULL` 返回 -1。
- 数据生命周期：data 由调用者提供，接口按 item_size 复制入内部缓冲区，返回后调用者可立即复用该内存；队列不持有指向调用者数据的指针。
- 输入输出行为：
  - shutting_down 已置位：无论队列是否已满、timeout 取值如何，返回 -3。
  - shutting_down 未置位且队列有空位：消息入队，后续 dequeue 按入队顺序取得，返回 0。
  - shutting_down 未置位且队列已满：timeout 等于 0，返回 -1；timeout 大于 0，至多等待 timeout，超时返回 -2，等待中获得空位返回 0，shutting_down 置位返回 -3；timeout 小于 0，阻塞直至获得空位返回 0 或 shutting_down 置位返回 -3。
  - 等待中的 pthread 原语错误：返回 -errno。

  可能的返回值：0、-1、-2、-3、-errno。
- 线程安全：多个生产者线程可并发调用，由内部互斥锁与条件变量串行化。

### `dequeue`

- 参数检查条件：`q == NULL` 或 `out_data == NULL` 返回 -1。
- 数据生命周期：out_data 由调用者提供，容量须不小于 item_size；接口从内部缓冲区复制出队消息。
- 输入输出行为：
  - shutting_down 已置位：无论 timeout 取值如何，返回 -3。
  - shutting_down 未置位且队列有消息：最早入队的消息复制到 out_data，返回 0。
  - shutting_down 未置位且队列为空：timeout 等于 0，返回 -1；timeout 大于 0，至多等待 timeout，超时返回 -2，等待中获得消息返回 0，shutting_down 置位返回 -3；timeout 小于 0，阻塞直至获得消息返回 0 或 shutting_down 置位返回 -3。
  - 等待中的 pthread 原语错误：返回 -errno。

  可能的返回值：0、-1、-2、-3、-errno。
- 线程安全：多个消费者线程可并发调用，由内部互斥锁与条件变量串行化。

## 4. 典型用例

```c
#include <stdio.h>
#include "queue.h"

int main(void)
{
    int item = 42, out;
    queue_t q;

    if (newqueue(&q, sizeof(int), 4) != 0)
    {
        return 1;
    }

    enqueue(&q, &item, 100);   /* 发送一条消息 */
    dequeue(&q, &out, -1);     /* 接收一条消息 */
    printf("%d\n", out);

    delequeue(&q);
    return 0;
}
```

## 5. 依赖库

- pthread
- libc

## 6. 设计围栏

| 排除能力 |
|----------|
| 可变长度消息 |
| 消息优先级 |
| 批量入队出队 |
| 零拷贝接口 |
| 无锁实现 |
| 运行时扩容 |
| 跨进程共享 |
