# CJSON_CONFIGUR 模块设计

## 1. 对外数据结构

模块为**全局单例**，采用结构优先模型：实例状态为配置树、配置文件路径与互斥锁，由库内静态存储承载，对外不导出任何数据结构。配置结构由声明定义：调用者先构建 `cJSONConfigur_Schema_t` 声明表，`Init` 时一次性生成配置树并从文件取值，此后结构冻结。

### 配置项类型

不定义自定义枚举，直接复用 cJSON Types：

| cJSON 类型 | 适用接口 | 说明 |
|------------|----------|------|
| cJSON_String | SchemaDeclareString / GetString / SetString | JSON string |
| cJSON_Number | SchemaDeclareNumber / GetInt / SetInt / GetDouble / SetDouble | JSON number |
| cJSON_Bool | SchemaDeclareBool / GetBool / SetBool | JSON true / false |

### `pointer` 参数格式

所有以 `pointer` 命名的参数为 RFC 6901 JSON Pointer 的**成员名子集**：
 - 以 `/` 开头；
 - token 为大小写敏感的 object 成员名，`~1` 转义 `/`、`~0` 转义 `~`；
 - 不支持数组索引语义。
 - NULL、不以 `/` 开头、`~` 后未紧跟 `0` 或 `1`，均视为格式非法。

### 返回值约定

| 返回值 | 含义 |
|--------|------|
| 0 | 成功 |
| -1 | 参数非法 |
| -2 | 单例未初始化 |
| -3 | 单例已初始化 |
| -4 | 配置文件无效或读写失败 |
| -5 | 路径不存在 |
| -6 | 值类型不匹配 |
| -7 | 输出缓冲区不足 |
| -8 | 内存分配失败 |

## 2. 对外接口一览表

| 签名 | 说明 |
|------|------|
| cJSONConfigur_Schema_t *cJSONConfigur_SchemaCreate(void) | 创建空声明表 |
| int cJSONConfigur_SchemaDeclareString(cJSONConfigur_Schema_t *schema, const char *pointer, const char *default_value) | 声明字符串配置项 |
| int cJSONConfigur_SchemaDeclareNumber(cJSONConfigur_Schema_t *schema, const char *pointer, double default_value) | 声明数值配置项 |
| int cJSONConfigur_SchemaDeclareBool(cJSONConfigur_Schema_t *schema, const char *pointer, bool default_value) | 声明布尔配置项 |
| void cJSONConfigur_SchemaDestroy(cJSONConfigur_Schema_t *schema) | 释放声明表 |
| int cJSONConfigur_Init(cJSONConfigur_Schema_t *schema, const char *path) | 由声明建树、读取配置文件取值 |
| int cJSONConfigur_Save(void) | 原子写回 Init 的文件 |
| int cJSONConfigur_Deinit(void) | 释放单例 |
| int cJSONConfigur_GetString(const char *pointer, char *out, size_t size) | 按路径读字符串 |
| int cJSONConfigur_GetInt(const char *pointer, int *out) | 按路径读整数 |
| int cJSONConfigur_GetBool(const char *pointer, bool *out) | 按路径读布尔 |
| int cJSONConfigur_GetDouble(const char *pointer, double *out) | 按路径读浮点 |
| int cJSONConfigur_SetString(const char *pointer, const char *value) | 按路径写字符串 |
| int cJSONConfigur_SetInt(const char *pointer, int value) | 按路径写整数 |
| int cJSONConfigur_SetBool(const char *pointer, bool value) | 按路径写布尔 |
| int cJSONConfigur_SetDouble(const char *pointer, double value) | 按路径写浮点 |

## 3. 对外接口详情


### `cJSONConfigur_SchemaCreate / SchemaDeclare* / SchemaDestroy`

- 参数检查条件：SchemaDeclare* 查 `schema == NULL`、`pointer == NULL` 或格式非法返回 -1；SchemaDeclareString 另查 `default_value == NULL` 返回 -1。SchemaCreate 失败返回 NULL；SchemaDestroy 接受 NULL，为空操作。
- 数据生命周期：默认值以**值拷贝**进声明表，字符串由库深拷贝，调用返回后与调用者变量无关联。声明表传入 Init 后即失效，含 SchemaDestroy 在内的任何后续使用均为未定义行为。
- 输入输出行为：
  - pointer 路径已存在且现有类型同族：幂等，保留首个默认值，返回 0；异族类型返回 -6，声明表不改动。同族判定：String、Number、Bool 各自成族，Number 族内 Int 与 Double 互通。
  - pointer 路径不存在：按接口类型以默认值创建，缺失的中间节点自动创建为 object，返回 0；创建或内存分配失败返回 -8。
  - 空声明表合法，Init 后配置树为空 object。
- 线程安全：无锁，调用者保证单线程。

### `cJSONConfigur_Init`

- 参数检查条件：已初始化返回 -3；`schema == NULL`、`path == NULL` 或空串返回 -1。
- 数据生命周期：**无论成败，Init 返回后 schema 即失效并由库释放**；`path` 为借用输入，成功后由库拷贝保存；返回 -1/-3 时不改变既有初始化状态。
- 输入输出行为，依次判定：
  1. 由声明表生成纯默认值树；
  2. 文件不存在或内容为空 / 纯空白：保留默认值，返回 0。存在性以打开错误 ENOENT 判定；
  3. 文件打开失败且错误非 ENOENT：返回 -4；
  4. 解析失败：返回 -4；顶层非 object，即标量、数组或 null：返回 -4；
  5. 按声明逐项从文件树取值：类型匹配则采用文件值；文件无该项或值为 `null` 则保留默认值；任一类型不符即整体拒绝，返回 -4；解析到的文件节点经数组定位命中，即父节点非 object 时，同样视为类型不符，返回 -4；声明路径中间节点在文件中为数组时定位失败，视为该子树未声明，保留默认值；
  6. 任何 -4/-8 全量回滚：单例回到未初始化，不修改目标文件，允许修复或更换文件后重新 Init；损坏文件由调用者处理，库不做自动隔离。
  - 成功：文件中未声明的内容一律忽略，不进入配置树；返回 0。配置结构自此冻结，单例侧无声明接口。
- 线程安全：调用者保证单线程调用；就绪后各接口经单例互斥锁互斥访问。

### `cJSONConfigur_Save`

- 参数检查条件：单例未初始化返回 -2。
- 数据生命周期：以 `cJSON_Print` 做格式化序列化，写入 `path` + `".tmp"` 后 `rename` 覆盖 `path`：目标文件要么是完整旧内容、要么是完整新内容，进程中途终止最坏残留临时文件，目标不受损；掉电级持久化是否追加 fsync 由实现层决定；任一步失败删除临时文件；序列化缓冲用毕释放；不改变内存配置。
- 输入输出行为：成功返回 0；序列化失败返回 -8；临时文件创建、写入或 rename 失败返回 -4。
- 破坏性契约：Save 以内存树整体覆写目标文件，文件中未被声明的内容将被清除。
- 线程安全：独占锁。

### `cJSONConfigur_Deinit`

- 参数检查条件：单例未初始化返回 -2。
- 数据生命周期：`cJSON_Delete` 释放树、释放路径副本；互斥锁不销毁；单例回到未初始化状态，可重新 Init。
- 输入输出行为：返回 0。**不自动保存**，需持久化须先显式调用 Save。
- 线程安全：调用者保证调用期间该单例上没有其他并发调用。

### `cJSONConfigur_GetString / GetInt / GetBool / GetDouble`

- 参数检查条件：单例未初始化返回 -2；`pointer == NULL` 或格式非法、输出指针为 `NULL` 返回 -1。
- 数据生命周期：值拷贝给调用者。GetString：`out` 非空且 `size > 0` 时总以 `'\0'` 结尾，`strlen + 1 <= size` 完整拷贝返回 0；否则拷贝前 `size - 1` 字节并返回 -7；`size == 0` 返回 -7。
- 输入输出行为：路径不存在返回 -5；异族读取返回 -6，如对 string 调 GetInt，同族 Number 内 GetInt / GetDouble 互通；GetInt 截断取数值的整数部分，数值超出 int 表示范围时结果未定义，需要完整值域用 GetDouble；成功返回 0。
- 线程安全：独占锁。

### `cJSONConfigur_SetString / SetInt / SetBool / SetDouble`

- 参数检查条件：单例未初始化返回 -2；`pointer == NULL` 或格式非法返回 -1；SetString 另查 `value == NULL` 返回 -1。
- 数据生命周期：值拷贝进 JSON 树；内存分配失败返回 -8；仅影响内存，须显式 Save 持久化。
- 输入输出行为：Set 不增减结构，只就地替换。
  - 路径已存在且为同族标量：替换值，返回 0。
  - 路径不存在，包括中间节点缺失或中间节点为标量无法定位：返回 -5，不创建任何节点。
  - 路径存在但类型不符，目标是 object 或异族标量：返回 -6。
- 线程安全：独占锁。

## 4. 典型用例

```c
int main(void)
{
    cJSONConfigur_Schema_t *s = cJSONConfigur_SchemaCreate();
    if (s == NULL) {
        return -1;
    }

    cJSONConfigur_SchemaDeclareString(s, "/server/host", "127.0.0.1");
    cJSONConfigur_SchemaDeclareNumber(s, "/server/port", 8080);
    cJSONConfigur_SchemaDeclareString(s, "/log/level", "info");
    cJSONConfigur_SchemaDeclareBool(s, "/log/enable", true);

    if (cJSONConfigur_Init(s, "/cfg/app.json") != 0) {
        return -1;
    }

    int port = 0;
    if (cJSONConfigur_GetInt("/server/port", &port) == 0) {
        /* 使用 port */
    }

    cJSONConfigur_SetString("/log/level", "debug");
    cJSONConfigur_Save();

    cJSONConfigur_Deinit();
    return 0;
}
```

## 5. 依赖库

- cJSON
- cJSONUtils
- libc
- pthread

## 6. 设计围栏

| 排除能力 |
|----------|
| 不支持多实例 |
| 不考虑多进程安全 |
| 不支持数组类型 |
| 不做违规项的精确诊断输出 |
| 不做 Strerror |
| 不考虑 64 位整数接口 |
| 不考虑热重载与配置变更通知 / 回调 |
| 不支持层叠配置 |
| 不做配置版本迁移 |
| 不做存储后绑定 |
| 不做无文件内存模式 |
| 不考虑损坏文件自动隔离 |
