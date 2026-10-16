# 0003 C 编码规范（草稿）

本规范与 0002 配套：0002 规定模块设计文档的写法，本规范规定模块实现代码的写法。适用范围与 0001 第 1 条一致：所有非第三方 C 源文件。

条文分级：必须、不得为强制；倾向、建议为默认取向，偏离须在评审时说明理由。

## 1. goto 与资源清理

- goto 仅允许一种用途：函数内错误回退时的资源清理，即向前跳转到本函数出口 return 之前的清理标签。
- 只能向前跳转：不得用 goto 构成循环、重试或回跳，不得跳入任何语句块内部。
- 清理标签以 `out_` 开头，后接该出口仍需执行的清理对象或动作，如 `out_lock`、`out_text`、`out_rollback`；仅单一清理出口时命名为 `out`。
- 清理标签按资源获取的逆序排列成阶梯：跳转到较早的标签即跳过其后标签的清理；正常路径自上而下落入各级标签，依次完成全部清理。
- 每个标签只清理到达它的路径中已获取的资源，不得重复释放或清理未获取的资源。

常规形态：

```c
int load(const char *path)
{
    int ret = -1;

    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        return ret;                      /* 无资源需清理, 直接返回 */
    }

    char *buf = malloc(256);
    if (buf == NULL)
    {
        goto out_fp;                     /* 仅需关闭 fp */
    }

    /* 正常路径 */

    ret = 0;
out_buf:
    free(buf);
out_fp:
    fclose(fp);
    return ret;
}
```

## 2. 模块状态与单例

- 单例模块的文件级可变状态必须聚合为一个 `static` 匿名结构体实例，不得散落为多个 `s_` / `g_` 文件级变量。
- 该结构体类型不带 tag、不 typedef，仅在定义处声明这一个实例；类型与实例均不对外导出。
- 实例命名为 `g_<module>`，`<module>` 为模块名的小写下划线形式，如模块 `sample` 对应 `g_sample`。
- 实例定义置于 .c 文件的 Private variables 分区；是否已初始化由实例内显式成员表达，如 `initialized`。
- `static const` 修饰的文件级数据（查找表、字符串常量）不属于模块可变状态，不受本条约束。
- 多实例模块不得拥有文件级可变状态，全部状态位于对外导出的实例结构体内（参考 `queue_t`）。

示例：

```c
/* 模块 sample 的单例状态: 唯一实例, 类型匿名 */
static struct
{
    void *ctx;                   /* 模块内部资源 */
    bool initialized;            /* 初始化标记 */
    pthread_mutex_t lock;        /* 模块互斥锁 */
} g_sample = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
};
```
