# CJSON_CONFIGUR 模块设计

## 1. 对外数据结构

### `cJSONConfigur_t`

用途：配置实例句柄，多实例各自绑定一个配置文件。内部封装 cJSON 树、文件路径与实例级读写锁，定义对外不透明。

```c
typedef struct cJSONConfigur cJSONConfigur_t;   /* 不透明配置实例句柄 */
```

| 属性 | 单位/取值范围 | 说明 |
|------|---------------|------|
| — | — | 仅经接口操作；Deinit 后句柄失效 |

### 配置项类型

不定义自定义枚举，直接复用 cJSON Types：

| cJSON 类型 | 适用接口 | 说明 |
|------------|----------|------|
| cJSON_String | GetString / SetString | JSON string |
| cJSON_Number | GetInt / SetInt / GetDouble / SetDouble | JSON number（同族覆盖整数与浮点） |
| cJSON_Bool | GetBool / SetBool | JSON true / false |

### 配置文件结构声明

库不强制 schema；顶层须为 JSON object。模块通过 `cJSONConfigur_Register` 声明所需配置项（JSON Pointer 路径 + cJSON 类型 + 默认值）。示例：

```json
{
    "server": {
        "host": "127.0.0.1",
        "port": 8080
    },
    "log": {
        "level": "info",
        "enable": true
    }
}
```

对应注册：`/server/port` cJSON_Number 默认 8080；`/log/level` cJSON_String 默认 "info"；`/log/enable` cJSON_Bool 默认 true。

## 2. 对外接口一览表

| 签名 | 说明 |
|------|------|
| int cJSONConfigur_Init(cJSONConfigur_t **out_cfg, const char *path) | 创建实例并加载配置文件 |
| int cJSONConfigur_Save(cJSONConfigur_t *cfg) | 将实例当前配置写回文件 |
| int cJSONConfigur_Deinit(cJSONConfigur_t *cfg) | 销毁实例（不自动保存） |
| int cJSONConfigur_Register(cJSONConfigur_t *cfg, const char *pointer, int type, const void *default_value) | 注册配置项（type 为 cJSON 类型常量，缺省自动填充默认值） |
| int cJSONConfigur_GetString(cJSONConfigur_t *cfg, const char *pointer, char *out, size_t size) | 按路径读字符串 |
| int cJSONConfigur_GetInt(cJSONConfigur_t *cfg, const char *pointer, int *out) | 按路径读整数 |
| int cJSONConfigur_GetBool(cJSONConfigur_t *cfg, const char *pointer, bool *out) | 按路径读布尔 |
| int cJSONConfigur_GetDouble(cJSONConfigur_t *cfg, const char *pointer, double *out) | 按路径读浮点 |
| int cJSONConfigur_SetString(cJSONConfigur_t *cfg, const char *pointer, const char *value) | 按路径写字符串 |
| int cJSONConfigur_SetInt(cJSONConfigur_t *cfg, const char *pointer, int value) | 按路径写整数 |
| int cJSONConfigur_SetBool(cJSONConfigur_t *cfg, const char *pointer, bool value) | 按路径写布尔 |
| int cJSONConfigur_SetDouble(cJSONConfigur_t *cfg, const char *pointer, double value) | 按路径写浮点 |

## 3. 对外接口详情

### 通用约定

- 所有 `cfg` 参数为 Init 返回的实例句柄；`cfg == NULL` 属参数非法，返回 -1。
- 所有 `pointer` 参数为 RFC 6901 JSON Pointer：空字符串表示整个文档；以 `/` 分隔 token；`~1` 转义 `/`，`~0` 转义 `~`；token 为 object 成员名（大小写敏感，采用 `cJSONUtils_GetPointerCaseSensitive` 语义）或数组十进制索引。
- 紧耦合约定：实例内部直接持有 `cJSON *` 树；定位读复用 cJSONUtils；定位写（中间节点自动创建）在 cJSON 树上实现，不引入中间抽象层。
- 同一文件路径不得绑定多个实例（各实例 Save 互不感知，后写覆盖先写）；库不检测此冲突。

### 通用返回码

| 返回值 | 含义 | 产生的接口 |
|--------|------|------------|
| 0 | 成功 | 全部 |
| -1 | 参数非法（NULL 指针、pointer 格式错误、type 非 cJSON_String/cJSON_Number/cJSON_Bool） | 全部 |
| -4 | 文件打开失败或 JSON 解析失败 / 写文件失败 | Init / Save |
| -5 | 路径不存在（Get）/ 路径创建失败、内存分配失败（Init/Set/Register） | Init / Register / Get / Set |
| -6 | 值类型不匹配（现有类型与操作不符） | Register / Get / Set |
| -7 | 输出缓冲区不足 | GetString |

### `cJSONConfigur_Init`

- 参数检查条件：`out_cfg == NULL`、`path == NULL` 或空串返回 -1。
- 数据生命周期：`malloc` 实例与锁，加载的 JSON 树由实例持有直至 Deinit；`path` 在 Init 返回后不再被引用（内部拷贝保存）。失败时 `*out_cfg` 置 NULL，实例不留存。
- 输入输出行为：解析成功返回 0 并经 `*out_cfg` 交出句柄；文件不存在、不可读或解析失败返回 -4；内存分配失败返回 -5。
- 线程安全：实例间独立；同一实例须以返回的句柄并发使用，接口内部加锁。

### `cJSONConfigur_Save`

- 参数检查条件：`cfg == NULL` 返回 -1。
- 数据生命周期：序列化实例内 cJSON 树写入该实例 Init 时的路径；不改变内存配置。
- 输入输出行为：成功返回 0；打开或写入失败返回 -4。写入非原子，进程中途终止可能损坏目标文件。
- 线程安全：实例内独占锁。

### `cJSONConfigur_Deinit`

- 参数检查条件：`cfg == NULL` 返回 -1。
- 数据生命周期：`cJSON_Delete` 释放树，释放路径副本、销毁锁并 `free` 实例本身。
- 输入输出行为：返回 0。**不自动保存**——需持久化须先显式调用 Save；返回后句柄立即失效，调用者不得再使用。
- 线程安全：实例内独占锁；调用者须保证 Deinit 后不再有该实例的并发调用。

### `cJSONConfigur_Register`

- 参数检查条件：`pointer == NULL`、`default_value == NULL` 返回 -1；`type` 不属于 cJSON_String / cJSON_Number / cJSON_Bool 返回 -1。
- 数据生命周期：按 `type` 解释 `default_value`（cJSON_String→`const char *`，cJSON_Number→`const double *`，cJSON_Bool→`const bool *`）并拷贝进 JSON 树，不保留调用者指针。
- 输入输出行为：
  - 路径已存在：现有类型与 `type` 同族则返回 0（不覆盖现有值），否则返回 -6。
  - 路径不存在：按 `type` 以默认值创建（中间节点自动创建 object），返回 0；创建或内存分配失败返回 -5。
  - 重复注册同一 pointer：幂等，按“路径已存在”处理。
- 线程安全：实例内独占锁。

### `cJSONConfigur_Get*`（四个读取接口）

- 参数检查条件：`pointer == NULL` 或输出指针为 NULL 返回 -1。
- 数据生命周期：值拷贝给调用者；GetString 拷贝至多 `size` 字节（含结尾 `\0`），不足返回 -7；其余直接解引用写出。
- 输入输出行为：定位用 `cJSONUtils_GetPointerCaseSensitive`；路径不存在返回 -5；类型不匹配（如对 string 调 GetInt，或 Number 调 GetString）返回 -6；GetInt 取数值的整数部分（截断，数值超出 int 表示范围时结果未定义）；成功返回 0。
- 线程安全：实例内共享锁（同实例多读者并发，不同实例互不影响）。

### `cJSONConfigur_Set*`（四个写入接口）

- 参数检查条件：`pointer == NULL` 返回 -1；SetString 另查 `value == NULL`。
- 数据生命周期：值拷贝进 JSON 树（cJSON_CreateString / CreateNumber / CreateBool）；仅影响内存，须显式 Save 持久化。
- 输入输出行为：
  - 路径已存在且为同族标量：替换值（string 经 `cJSON_SetValuestring`，number/bool 经 ReplaceItem），返回 0。
  - 路径不存在：按接口类型创建，中间节点自动创建 object（数组索引定位要求中间节点已存在），返回 0；创建或内存分配失败返回 -5。
  - 目标为 object/array 而以标量 Set：返回 -6。
- 线程安全：实例内独占锁。

## 4. 典型用例

```c
cJSONConfigur_t *cfg = NULL;

/* 创建实例并加载 */
if (cJSONConfigur_Init(&cfg, "config.json") != 0) { /* 处理失败 */ }

/* 注册缺省项：文件缺失该路径时自动填充 */
double port_def = 8080;
bool enable_def = true;
cJSONConfigur_Register(cfg, "/server/port", cJSON_Number, &port_def);
cJSONConfigur_Register(cfg, "/log/enable", cJSON_Bool, &enable_def);

/* 读 */
int port = 0;
char level[16] = {0};
cJSONConfigur_GetInt(cfg, "/server/port", &port);
cJSONConfigur_GetString(cfg, "/log/level", level, sizeof(level));

/* 写 + 持久化 */
cJSONConfigur_SetString(cfg, "/log/level", "debug");
cJSONConfigur_Save(cfg);

cJSONConfigur_Deinit(cfg);
```

## 5. 依赖库

- cJSON
- cJSONUtils
- libc
- pthread
