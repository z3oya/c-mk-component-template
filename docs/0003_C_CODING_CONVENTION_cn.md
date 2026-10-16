# 0003 C 编码规范

本规范与 0002 配套，规定模块实现代码的写法；适用范围同 0001 第 1 条。

## 1. goto 与资源清理

- goto 仅允许一种用途：函数内错误回退时的资源清理，向前跳转到本函数出口 return 之前的清理标签。
- 无资源需清理的失败路径直接 return。
- 清理标签以 `out_` 开头，后接该出口仍需清理的对象或动作，如 `out_lock`、`out_text`；仅单一清理出口时命名为 `out`。
- 每个标签只清理到达它的路径中已获取的资源。
- 返回值在进入清理区前确定，标签内不再修改。

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

    /* 正常路径: 读入内容 */
    ret = (fread(buf, 1, 256, fp) > 0) ? 0 : -2;
out_buf:
    free(buf);
out_fp:
    fclose(fp);
    return ret;
}
```

## 2. 模块状态与单例

- 文件级可变状态必须聚合为一个 `static` 匿名结构体实例。
- 结构体类型不带 tag、不 typedef。
- 实例命名为 `g_<module>`，`<module>` 为模块名的小写下划线形式。
- 实例定义置于 .c 文件的 Private variables 分区；是否已初始化由实例内显式成员表达。
- 文件级 `static const` 数据不受本条约束。
- 多实例模块不得拥有文件级可变状态，全部状态位于对外导出的实例结构体内。

示例：

```c
static struct
{
    void *ctx;
    bool initialized;
    pthread_mutex_t lock;
} g_sample = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
};
```
