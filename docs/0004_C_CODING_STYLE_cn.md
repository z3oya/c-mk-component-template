# 0004 C 编码风格

## 1. 缩进与行宽

- 缩进一律用 4 个空格，不用 Tab。
- 缩进超过 3 层即应重排代码。
- 用户可见字符串不得跨行拆分。
- 行尾不留空白。
- 一行只写一条语句、一个赋值。
- switch 的各 case 比 switch 缩进一层。

```c
switch (suffix)
{
    case 'G':
        mem <<= 30;
        break;
    case 'M':
        mem <<= 20;
        break;
    default:
        break;
}
```

## 2. 花括号与空格

- struct、union、enum 定义的开括号可留行尾；
- 其余情况开括号一律另起一行。
- else 另起一行；do-while 的闭括号与 while 连写：`} while (...);`。
- 条件与循环体一律写完整的花括号，即使只有一条语句。
- if、switch、case、for、do、while 后加空格；
- sizeof 按函数形式书写，后不加空格。
- 二元与三元运算符两侧各留一个空格；
- 一元运算符、前后缀 ++ 与 --、`.` 与 `->` 两侧不留。
- 括号内部不留空格。
- 指针声明的星号贴数据名，不贴类型名。

## 3. 命名

- 仅文件内可见的符号取短名：循环计数用 i，临时对象用 tmp；需要复杂名字区分的局部变量，先拆函数。
- 宏与枚举常量全大写；形似函数的宏可小写，但优先改写为 static inline 函数。
- typedef 定义的类型名推荐以 _t 结尾；仅以 struct 声明的类型名不使用 _t。

## 4. 类型与声明

- 固定宽度整数使用 <stdint.h> 的 uintN_t 与 intN_t。
- 一行只声明一个对象，使其可逐项注释。

## 5. 函数

- 一个函数只做一件事。
- 局部变量不超过 10 个。
- 复杂逻辑拆出描述性命名的辅助函数，是否内联由编译器决定。
- 函数之间空两行。
- 原型写出参数名，不加 extern 修饰。

## 6. 注释

- 只用 /* */，不用 //。
- 注释说明用途与约束（做什么、为什么），不复述代码怎么做；需要靠注释分段解释的函数先拆分。
- 数据注释优于代码注释：先写清字段与全局变量的用途。
- 多行注释首行只有 /*，末行只有 */，中间行星号对齐。

```c
/*
 * 多行注释首行写完整句子。
 * 后续行与首行对齐。
 */
```

## 7. 宏与枚举

- 多语句宏必须用 do { } while (0) 包裹。
- 宏不隐藏控制流，不把参数当左值，参数表达式必须加括号。

## 8. 返回值与布尔

- 动作型函数返回整数错误码：0 表示成功，负值表示失败。
- 谓词型函数返回 bool，true 表示成立。
- bool 只取 true 与 false，不用于对布局或大小敏感的结构。

## 9. goto 与资源清理

- goto 仅用于函数内错误回退的资源清理；
- 清理标签以 `out_` 开头，后接需清理的对象或动作，如 `out_lock`、`out_text`；
- 仅单一出口时可命名为 `out`。
- 返回值在进入清理区前确定，标签内不再修改。

示例形态：

```c
int load(const char *path)
{
    int ret = -1;

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) { return ret; }              /* 无资源需清理, 直接返回 */

    char *buf = malloc(256);
    if (buf == NULL) { goto out_fp; }            /* 仅需关闭 fp */

    /* 正常路径: 读入内容 */
    ret = (fread(buf, 1, 256, fp) > 0) ? 0 : -2;
    
out_buf:
    free(buf);
out_fp:
    fclose(fp);
    return ret;
}
```

## 10. 模块与单例

- 文件级私有变量 >1 时改用聚合 `static` 匿名结构体实例：不带 tag、不 typedef；如命名为 `g_<module>`。
- 是否已初始化由实例内显式成员表达；文件级 `static const` 数据不受本节约束。
- 多实例模块不应拥有文件级私有变量。

示例：

```c
static struct {
    void *ctx;
    bool initialized;
    pthread_mutex_t lock;
} g_sample = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
};
```
