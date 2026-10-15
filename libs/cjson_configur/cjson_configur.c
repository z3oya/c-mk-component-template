/**
  ******************************************************************************
  * File Name          : cjson_configur.c
  * Description        : source for cjson_configur
  ******************************************************************************
  * @attention
  *
  * Copyright (c) lib3yu(neon).
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
***/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "cjson_configur.h"
/* Private includes ----------------------------------------------------------*/
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <pthread.h>
#include "cJSON.h"

/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
struct cJSONConfigur_Schema
{
    cJSON *root;            /* 声明表根节点: object 树, 叶节点携带各类型默认值 */
};

/* 配置项类型族: String、Number、Bool 各自成族, Number 族内 Int 与 Double 互通 */
typedef enum
{
    CONFIG_FAMILY_STRING = 0,
    CONFIG_FAMILY_NUMBER,
    CONFIG_FAMILY_BOOL,
    CONFIG_FAMILY_NONE
} config_family_t;

/* 声明过程中已创建中间节点的记录, 用于失败时回滚声明表 */
typedef struct
{
    cJSON *parent;          /* 已创建中间节点挂载的父节点 */
    char *name;             /* 已创建中间节点的成员名 (堆拷贝) */
} config_created_t;

/* Private variables ---------------------------------------------------------*/
static cJSON *s_config_tree = NULL;      /* 配置树 */
static char *s_config_path = NULL;       /* 配置文件路径副本 */
static bool s_initialized = false;       /* 单例初始化标记 */
static pthread_mutex_t s_config_lock = PTHREAD_MUTEX_INITIALIZER; /* 单例互斥锁, Deinit 不销毁 */
/* Private define (typed) ----------------------------------------------------*/
/* Private macro (typed) -----------------------------------------------------*/
/* Private variables (typed) -------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
// 校验 pointer 格式: 以 / 开头, ~ 后紧跟 0 或 1
static bool pointer_check(const char *pointer);
// 统计 pointer 的 token 数
static size_t pointer_token_count(const char *pointer);
// 从 *cursor 处解码一个 token 到 out (容量不小于指针长度 + 1), cursor 推进到下一分隔符
static void pointer_decode_token(const char **cursor, char *out);
// 取节点类型族
static config_family_t family_of(const cJSON *item);
// 判定两类型族是否同族
static bool family_match(config_family_t a, config_family_t b);
// 字符串堆拷贝
static char *dup_str(const char *src);
// 释放声明表本体
static void schema_release(cJSONConfigur_Schema_t *schema);
// 将携带默认值的叶子节点写入声明表, 失败时回滚本次已创建节点
static int schema_declare(cJSON *root, const char *pointer, cJSON *value);
// 读取配置文件并按声明逐项取值合并进默认值树
static int init_load_file(const cJSON *schema_root, cJSON *default_root, const char *path);
// 按声明树逐项遍历: 类型匹配采用文件值, 缺失或 null 保留默认值, 类型不符整体拒绝
static void merge_declared(const cJSON *schema_node, cJSON *default_node, const cJSON *file_node, int *status);
// 定位配置树中 pointer 的末级节点, 返回 0 时 *child_out 为末级节点 (可能为 NULL), -5 路径不存在, -8 分配失败
static int config_locate(cJSON *root, const char *pointer, cJSON **child_out);
/* Private define (auxiliary) ------------------------------------------------*/
/* Private macro (auxiliary) -------------------------------------------------*/
/* Private function code -----------------------------------------------------*/
static bool pointer_check(const char *pointer)
{
    if ((pointer == NULL) || (pointer[0] != '/'))
    {
        return false;
    }

    for (size_t i = 1; pointer[i] != '\0'; i++)
    {
        if (pointer[i] == '~')
        {
            if ((pointer[i + 1] != '0') && (pointer[i + 1] != '1'))
            {
                return false;
            }
            i++;
        }
    }
    return true;
}

static size_t pointer_token_count(const char *pointer)
{
    size_t count = 0;
    for (size_t i = 0; pointer[i] != '\0'; i++)
    {
        if (pointer[i] == '/')
        {
            count++;
        }
    }
    return count;
}

static void pointer_decode_token(const char **cursor, char *out)
{
    const char *p = *cursor + 1;             /* 跳过分隔符 '/' */
    const char *end = strchr(p, '/');
    if (end == NULL)
    {
        end = p + strlen(p);
    }

    size_t j = 0;
    for (const char *q = p; q < end; q++)
    {
        if (*q == '~')                       /* ~1 -> '/', ~0 -> '~', 合法性由 pointer_check 保证 */
        {
            out[j++] = (q[1] == '1') ? '/' : '~';
            q++;
        }
        else
        {
            out[j++] = *q;
        }
    }
    out[j] = '\0';
    *cursor = end;
}

static config_family_t family_of(const cJSON *item)
{
    if (item == NULL)
    {
        return CONFIG_FAMILY_NONE;
    }
    if (cJSON_IsString(item))
    {
        return CONFIG_FAMILY_STRING;
    }
    if (cJSON_IsNumber(item))
    {
        return CONFIG_FAMILY_NUMBER;
    }
    if (cJSON_IsBool(item))
    {
        return CONFIG_FAMILY_BOOL;
    }
    return CONFIG_FAMILY_NONE;
}

static bool family_match(config_family_t a, config_family_t b)
{
    return (a == b) && (a != CONFIG_FAMILY_NONE);
}

static char *dup_str(const char *src)
{
    size_t len = strlen(src);
    char *dst = malloc(len + 1);
    if (dst != NULL)
    {
        memcpy(dst, src, len + 1);
    }
    return dst;
}

static void schema_release(cJSONConfigur_Schema_t *schema)
{
    if (schema == NULL)
    {
        return;
    }
    cJSON_Delete(schema->root);
    free(schema);
}

static int schema_declare(cJSON *root, const char *pointer, cJSON *value)
{
    if (value == NULL)
    {
        return -8;
    }

    size_t count = pointer_token_count(pointer);
    char *scratch = malloc(strlen(pointer) + 1);
    config_created_t *created = malloc(count * sizeof(config_created_t));
    if ((scratch == NULL) || (created == NULL))
    {
        free(scratch);
        free(created);
        cJSON_Delete(value);
        return -8;
    }

    cJSON *cur = root;
    const char *cursor = pointer;
    size_t created_count = 0;
    int ret = 0;

    for (size_t i = 0; i < count; i++)
    {
        pointer_decode_token(&cursor, scratch);
        cJSON *child = cJSON_GetObjectItemCaseSensitive(cur, scratch);

        if (i + 1 < count)                   /* 中间节点 */
        {
            if (child == NULL)
            {
                cJSON *node = cJSON_CreateObject();
                if ((node == NULL) || !cJSON_AddItemToObject(cur, scratch, node))
                {
                    cJSON_Delete(node);
                    ret = -8;
                    break;
                }
                created[created_count].parent = cur;
                created[created_count].name = dup_str(scratch);
                if (created[created_count].name == NULL)
                {
                    cJSON_DeleteItemFromObjectCaseSensitive(cur, scratch);
                    ret = -8;
                    break;
                }
                created_count++;
                cur = node;
            }
            else if (!cJSON_IsObject(child))
            {
                ret = -6;                    /* 中间节点已存在且非 object, 无法挂载声明子树 */
                break;
            }
            else
            {
                cur = child;
            }
        }
        else                                 /* 叶子节点 */
        {
            if (child == NULL)
            {
                if (!cJSON_AddItemToObject(cur, scratch, value))
                {
                    ret = -8;                /* value 未挂载, 仍归本函数释放 */
                }
                else
                {
                    value = NULL;            /* 所有权移入声明表 */
                }
            }
            else
            {
                config_family_t want = family_of(value);
                cJSON_Delete(value);
                value = NULL;
                if (!family_match(family_of(child), want))
                {
                    ret = -6;                /* 异族: 声明表不改动 */
                }
                /* 同族: 幂等, 保留首个默认值 */
            }
        }
    }

    if (ret != 0)
    {
        while (created_count > 0)            /* 回滚本次已创建的中间节点, 声明表保持原状 */
        {
            created_count--;
            cJSON_DeleteItemFromObjectCaseSensitive(created[created_count].parent, created[created_count].name);
            free(created[created_count].name);
        }
    }
    for (size_t i = 0; i < created_count; i++)
    {
        free(created[i].name);               /* 成功路径: 释放未回滚条目的成员名拷贝 */
    }
    if (value != NULL)                       /* 中途失败: 释放未挂载的默认值节点 */
    {
        cJSON_Delete(value);
    }

    free(created);
    free(scratch);
    return ret;
}

static int init_load_file(const cJSON *schema_root, cJSON *default_root, const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        if (errno == ENOENT)
        {
            return 0;                        /* 文件不存在: 保留默认值 */
        }
        return -4;
    }

    if (fseek(fp, 0, SEEK_END) != 0)
    {
        fclose(fp);
        return -4;
    }
    long size = ftell(fp);
    if (size < 0)
    {
        fclose(fp);
        return -4;
    }
    if (fseek(fp, 0, SEEK_SET) != 0)
    {
        fclose(fp);
        return -4;
    }

    char *text = malloc((size_t)size + 1);
    if (text == NULL)
    {
        fclose(fp);
        return -8;
    }
    size_t nread = fread(text, 1, (size_t)size, fp);
    bool read_error = ferror(fp);
    fclose(fp);
    if (read_error)
    {
        free(text);
        return -4;
    }
    text[nread] = '\0';

    bool blank = true;                       /* 内容为空或纯空白: 保留默认值 */
    for (size_t i = 0; i < nread; i++)
    {
        if (!isspace((unsigned char)text[i]))
        {
            blank = false;
            break;
        }
    }
    if (blank)
    {
        free(text);
        return 0;
    }

    cJSON *file_root = cJSON_Parse(text);
    free(text);
    if (file_root == NULL)
    {
        return -4;                           /* 解析失败 */
    }
    if (!cJSON_IsObject(file_root))          /* 顶层非 object */
    {
        cJSON_Delete(file_root);
        return -4;
    }

    int status = 0;
    merge_declared(schema_root, default_root, file_root, &status);
    cJSON_Delete(file_root);
    return status;
}

static void merge_declared(const cJSON *schema_node, cJSON *default_node, const cJSON *file_node, int *status)
{
    for (const cJSON *s = schema_node->child; (s != NULL) && (*status == 0); s = s->next)
    {
        cJSON *d = cJSON_GetObjectItemCaseSensitive(default_node, s->string);
        cJSON *f = NULL;
        if ((file_node != NULL) && cJSON_IsObject(file_node))
        {
            f = cJSON_GetObjectItemCaseSensitive(file_node, s->string);
        }

        if (cJSON_IsObject(s))               /* 中间声明节点 */
        {
            if ((f != NULL) && cJSON_IsObject(f))
            {
                merge_declared(s, d, f, status);
            }
            /* 文件中缺失或非 object: 该子树未声明, 保留默认值 */
        }
        else                                 /* 叶子声明节点 */
        {
            if ((f == NULL) || cJSON_IsNull(f))
            {
                continue;                    /* 文件无该项或值为 null: 保留默认值 */
            }
            if (!family_match(family_of(f), family_of(s)))
            {
                *status = -4;                /* 类型不符: 整体拒绝 */
                return;
            }

            cJSON *copy = cJSON_Duplicate(f, 1);
            if (copy == NULL)
            {
                *status = -8;
                return;
            }
            if ((d == NULL) || !cJSON_ReplaceItemViaPointer(default_node, d, copy))
            {
                cJSON_Delete(copy);
                *status = -8;
                return;
            }
            /* 被替换节点已由 cJSON_ReplaceItemViaPointer 释放 */
        }
    }
}

static int config_locate(cJSON *root, const char *pointer, cJSON **child_out)
{
    size_t count = pointer_token_count(pointer);
    char *scratch = malloc(strlen(pointer) + 1);
    if (scratch == NULL)
    {
        return -8;
    }

    cJSON *cur = root;
    cJSON *child = NULL;
    const char *cursor = pointer;
    int ret = 0;

    for (size_t i = 0; i < count; i++)
    {
        pointer_decode_token(&cursor, scratch);
        child = cJSON_GetObjectItemCaseSensitive(cur, scratch);

        if (i + 1 < count)
        {
            if ((child == NULL) || !cJSON_IsObject(child))
            {
                ret = -5;                    /* 中间节点缺失或中间节点为标量, 无法定位 */
                break;
            }
            cur = child;
        }
    }

    free(scratch);
    if (ret == 0)
    {
        *child_out = child;                  /* count >= 1, child 至少被赋值一次 */
    }
    return ret;
}

/* Public application code ---------------------------------------------------*/

// 创建空声明表 (失败返回 NULL)
cJSONConfigur_Schema_t *cJSONConfigur_SchemaCreate(void)
{
    cJSONConfigur_Schema_t *schema = malloc(sizeof(cJSONConfigur_Schema_t));
    if (schema == NULL)
    {
        return NULL;
    }

    schema->root = cJSON_CreateObject();
    if (schema->root == NULL)
    {
        free(schema);
        return NULL;
    }
    return schema;
}

// 声明字符串配置项 (0 成功, -1 参数非法, -6 类型不匹配, -8 内存分配失败)
int cJSONConfigur_SchemaDeclareString(cJSONConfigur_Schema_t *schema, const char *pointer, const char *default_value)
{
    if ((schema == NULL) || (pointer == NULL) || (default_value == NULL) || !pointer_check(pointer))
    {
        return -1;
    }

    cJSON *value = cJSON_CreateString(default_value);   /* 默认值深拷贝进声明表 */
    if (value == NULL)
    {
        return -8;
    }
    return schema_declare(schema->root, pointer, value);
}

// 声明数值配置项 (0 成功, -1 参数非法, -6 类型不匹配, -8 内存分配失败)
int cJSONConfigur_SchemaDeclareNumber(cJSONConfigur_Schema_t *schema, const char *pointer, double default_value)
{
    if ((schema == NULL) || (pointer == NULL) || !pointer_check(pointer))
    {
        return -1;
    }

    cJSON *value = cJSON_CreateNumber(default_value);
    if (value == NULL)
    {
        return -8;
    }
    return schema_declare(schema->root, pointer, value);
}

// 声明布尔配置项 (0 成功, -1 参数非法, -6 类型不匹配, -8 内存分配失败)
int cJSONConfigur_SchemaDeclareBool(cJSONConfigur_Schema_t *schema, const char *pointer, bool default_value)
{
    if ((schema == NULL) || (pointer == NULL) || !pointer_check(pointer))
    {
        return -1;
    }

    cJSON *value = cJSON_CreateBool(default_value);
    if (value == NULL)
    {
        return -8;
    }
    return schema_declare(schema->root, pointer, value);
}

// 释放声明表 (接受 NULL, 为空操作)
void cJSONConfigur_SchemaDestroy(cJSONConfigur_Schema_t *schema)
{
    schema_release(schema);
}

// 由声明建树、读取配置文件取值 (0 成功, -1 参数非法, -3 已初始化, -4 配置文件无效, -8 内存分配失败)
int cJSONConfigur_Init(cJSONConfigur_Schema_t *schema, const char *path)
{
    cJSON *tree = NULL;
    char *path_copy = NULL;
    int ret = 0;

    if (s_initialized)
    {
        schema_release(schema);              /* 无论成败 Init 返回后 schema 即失效并由库释放 */
        return -3;
    }

    if ((schema == NULL) || (path == NULL) || (path[0] == '\0'))
    {
        schema_release(schema);
        return -1;
    }

    /* 1. 由声明表生成纯默认值树 */
    tree = cJSON_Duplicate(schema->root, 1);
    path_copy = dup_str(path);
    if ((tree == NULL) || (path_copy == NULL))
    {
        ret = -8;
        goto out_rollback;
    }

    /* 2..5. 读文件取值 */
    ret = init_load_file(schema->root, tree, path);
    if (ret != 0)
    {
        goto out_rollback;
    }

    s_config_tree = tree;
    s_config_path = path_copy;
    s_initialized = true;
    schema_release(schema);
    return 0;

out_rollback:                                /* 6. 任何 -4/-8 全量回滚到未初始化, 不修改目标文件 */
    cJSON_Delete(tree);
    free(path_copy);
    schema_release(schema);
    return ret;
}

// 原子写回 Init 的文件 (0 成功, -2 未初始化, -4 读写失败, -8 内存分配失败)
int cJSONConfigur_Save(void)
{
    int ret = 0;

    if (!s_initialized)
    {
        return -2;
    }

    pthread_mutex_lock(&s_config_lock);

    char *text = cJSON_Print(s_config_tree);
    if (text == NULL)
    {
        ret = -8;                            /* 序列化失败 */
        goto out_lock;
    }

    size_t len = strlen(s_config_path);
    char *tmp_path = malloc(len + sizeof(".tmp"));
    if (tmp_path == NULL)
    {
        ret = -8;
        goto out_text;
    }
    memcpy(tmp_path, s_config_path, len);
    memcpy(tmp_path + len, ".tmp", sizeof(".tmp"));

    FILE *fp = fopen(tmp_path, "wb");
    if (fp == NULL)
    {
        ret = -4;
        goto out_tmp_path;
    }

    size_t text_len = strlen(text);
    int write_failed = (fwrite(text, 1, text_len, fp) != text_len);
    int close_failed = (fclose(fp) != 0);    /* 先取写结果再关闭, 同时检测写入与关闭错误 */
    if (write_failed || close_failed || (rename(tmp_path, s_config_path) != 0))
    {
        remove(tmp_path);                    /* 任一步失败: 删除临时文件, 目标不受损 */
        ret = -4;
    }

out_tmp_path:
    free(tmp_path);
out_text:
    cJSON_free(text);
out_lock:
    pthread_mutex_unlock(&s_config_lock);
    return ret;
}

// 释放单例 (0 成功, -2 未初始化)
int cJSONConfigur_Deinit(void)
{
    if (!s_initialized)
    {
        return -2;
    }

    cJSON_Delete(s_config_tree);
    s_config_tree = NULL;
    free(s_config_path);
    s_config_path = NULL;
    s_initialized = false;                   /* 锁不销毁, 单例可重新 Init; 不自动保存 */
    return 0;
}

// 按路径读字符串 (0 成功, -1 参数非法, -2 未初始化, -5 路径不存在, -6 类型不匹配, -7 输出缓冲区不足)
int cJSONConfigur_GetString(const char *pointer, char *out, size_t size)
{
    if (!s_initialized)
    {
        return -2;
    }
    if ((pointer == NULL) || !pointer_check(pointer) || (out == NULL))
    {
        return -1;
    }

    int ret = 0;
    pthread_mutex_lock(&s_config_lock);

    cJSON *item = NULL;
    ret = config_locate(s_config_tree, pointer, &item);
    if (ret == 0)
    {
        if (item == NULL)
        {
            ret = -5;                        /* 路径不存在 */
        }
        else if (family_of(item) != CONFIG_FAMILY_STRING)
        {
            ret = -6;                        /* 异族读取 */
        }
        else
        {
            const char *value = cJSON_GetStringValue(item);
            size_t len = strlen(value);
            if (len + 1 <= size)
            {
                memcpy(out, value, len + 1); /* 完整拷贝, 总以 '\0' 结尾 */
                ret = 0;
            }
            else
            {
                if (size > 0)                /* size == 0 不写 out */
                {
                    memcpy(out, value, size - 1);
                    out[size - 1] = '\0';
                }
                ret = -7;                    /* 输出缓冲区不足 */
            }
        }
    }

    pthread_mutex_unlock(&s_config_lock);
    return ret;
}

// 按路径读整数 (0 成功, -1 参数非法, -2 未初始化, -5 路径不存在, -6 类型不匹配)
int cJSONConfigur_GetInt(const char *pointer, int *out)
{
    if (!s_initialized)
    {
        return -2;
    }
    if ((pointer == NULL) || !pointer_check(pointer) || (out == NULL))
    {
        return -1;
    }

    int ret = 0;
    pthread_mutex_lock(&s_config_lock);

    cJSON *item = NULL;
    ret = config_locate(s_config_tree, pointer, &item);
    if (ret == 0)
    {
        if (item == NULL)
        {
            ret = -5;
        }
        else if (family_of(item) != CONFIG_FAMILY_NUMBER)
        {
            ret = -6;                        /* Number 族内 GetInt / GetDouble 互通, 其余异族 */
        }
        else
        {
            *out = item->valueint;           /* 截断取整数部分 (cJSON 维护 valueint) */
        }
    }

    pthread_mutex_unlock(&s_config_lock);
    return ret;
}

// 按路径读布尔 (0 成功, -1 参数非法, -2 未初始化, -5 路径不存在, -6 类型不匹配)
int cJSONConfigur_GetBool(const char *pointer, bool *out)
{
    if (!s_initialized)
    {
        return -2;
    }
    if ((pointer == NULL) || !pointer_check(pointer) || (out == NULL))
    {
        return -1;
    }

    int ret = 0;
    pthread_mutex_lock(&s_config_lock);

    cJSON *item = NULL;
    ret = config_locate(s_config_tree, pointer, &item);
    if (ret == 0)
    {
        if (item == NULL)
        {
            ret = -5;
        }
        else if (family_of(item) != CONFIG_FAMILY_BOOL)
        {
            ret = -6;
        }
        else
        {
            *out = cJSON_IsTrue(item);
        }
    }

    pthread_mutex_unlock(&s_config_lock);
    return ret;
}

// 按路径读浮点 (0 成功, -1 参数非法, -2 未初始化, -5 路径不存在, -6 类型不匹配)
int cJSONConfigur_GetDouble(const char *pointer, double *out)
{
    if (!s_initialized)
    {
        return -2;
    }
    if ((pointer == NULL) || !pointer_check(pointer) || (out == NULL))
    {
        return -1;
    }

    int ret = 0;
    pthread_mutex_lock(&s_config_lock);

    cJSON *item = NULL;
    ret = config_locate(s_config_tree, pointer, &item);
    if (ret == 0)
    {
        if (item == NULL)
        {
            ret = -5;
        }
        else if (family_of(item) != CONFIG_FAMILY_NUMBER)
        {
            ret = -6;
        }
        else
        {
            *out = item->valuedouble;
        }
    }

    pthread_mutex_unlock(&s_config_lock);
    return ret;
}

// 按路径写字符串 (0 成功, -1 参数非法, -2 未初始化, -5 路径不存在, -6 类型不匹配, -8 内存分配失败)
int cJSONConfigur_SetString(const char *pointer, const char *value)
{
    if (!s_initialized)
    {
        return -2;
    }
    if ((pointer == NULL) || !pointer_check(pointer) || (value == NULL))
    {
        return -1;
    }

    int ret = 0;
    pthread_mutex_lock(&s_config_lock);

    cJSON *item = NULL;
    ret = config_locate(s_config_tree, pointer, &item);
    if (ret == 0)
    {
        if (item == NULL)
        {
            ret = -5;                        /* 路径不存在, 不创建任何节点 */
        }
        else if (family_of(item) != CONFIG_FAMILY_STRING)
        {
            ret = -6;                        /* 目标是 object 或异族标量 */
        }
        else if (cJSON_SetValuestring(item, value) == NULL)
        {
            ret = -8;                        /* 值拷贝分配失败 (同长度时原缓冲就地覆盖, 不会失败) */
        }
    }

    pthread_mutex_unlock(&s_config_lock);
    return ret;
}

// 按路径写整数 (0 成功, -1 参数非法, -2 未初始化, -5 路径不存在, -6 类型不匹配)
int cJSONConfigur_SetInt(const char *pointer, int value)
{
    if (!s_initialized)
    {
        return -2;
    }
    if ((pointer == NULL) || !pointer_check(pointer))
    {
        return -1;
    }

    int ret = 0;
    pthread_mutex_lock(&s_config_lock);

    cJSON *item = NULL;
    ret = config_locate(s_config_tree, pointer, &item);
    if (ret == 0)
    {
        if (item == NULL)
        {
            ret = -5;
        }
        else if (family_of(item) != CONFIG_FAMILY_NUMBER)
        {
            ret = -6;
        }
        else
        {
            (void)cJSON_SetNumberHelper(item, (double)value);
        }
    }

    pthread_mutex_unlock(&s_config_lock);
    return ret;
}

// 按路径写布尔 (0 成功, -1 参数非法, -2 未初始化, -5 路径不存在, -6 类型不匹配)
int cJSONConfigur_SetBool(const char *pointer, bool value)
{
    if (!s_initialized)
    {
        return -2;
    }
    if ((pointer == NULL) || !pointer_check(pointer))
    {
        return -1;
    }

    int ret = 0;
    pthread_mutex_lock(&s_config_lock);

    cJSON *item = NULL;
    ret = config_locate(s_config_tree, pointer, &item);
    if (ret == 0)
    {
        if (item == NULL)
        {
            ret = -5;
        }
        else if (family_of(item) != CONFIG_FAMILY_BOOL)
        {
            ret = -6;
        }
        else
        {
            (void)cJSON_SetBoolValue(item, value);
        }
    }

    pthread_mutex_unlock(&s_config_lock);
    return ret;
}

// 按路径写浮点 (0 成功, -1 参数非法, -2 未初始化, -5 路径不存在, -6 类型不匹配)
int cJSONConfigur_SetDouble(const char *pointer, double value)
{
    if (!s_initialized)
    {
        return -2;
    }
    if ((pointer == NULL) || !pointer_check(pointer))
    {
        return -1;
    }

    int ret = 0;
    pthread_mutex_lock(&s_config_lock);

    cJSON *item = NULL;
    ret = config_locate(s_config_tree, pointer, &item);
    if (ret == 0)
    {
        if (item == NULL)
        {
            ret = -5;
        }
        else if (family_of(item) != CONFIG_FAMILY_NUMBER)
        {
            ret = -6;
        }
        else
        {
            (void)cJSON_SetNumberHelper(item, value);
        }
    }

    pthread_mutex_unlock(&s_config_lock);
    return ret;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
