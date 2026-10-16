/**
  ******************************************************************************
  * File Name          : test_cjson_configur.c
  * Description        : Unity unit tests for cjson_configur (docs/0200 §3)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) z3oya(neon).
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
#include "unity.h"
#include "cjson_configur.h"
/* Private includes ----------------------------------------------------------*/
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include "cJSON.h"

/* Private define ------------------------------------------------------------*/
#define TEST_PATH_FMT      "/tmp/cjson_configur_test_%s.json"
#define TEST_DIR_FMT       "/tmp/cjson_configur_test_%s.d"
#define TEST_PATH_MAX      128
#define TEST_FILE_BUF_MAX  512
#define TEST_STR_BUF_MAX   64
#define TEST_DOUBLE_EPS    1e-9
/* Private macro -------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static char g_cfg_path[TEST_PATH_MAX];   /* 本用例创建的临时配置文件, tearDown 清理 */
static char g_cfg_dir[TEST_PATH_MAX];    /* 本用例创建的临时目录 (目录注入类用例), tearDown 清理 */
/* Private define (typed) ----------------------------------------------------*/
/* Private macro (typed) -----------------------------------------------------*/
/* Private variables (typed) -------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void remove_path_pair(const char *path);
static void remove_temp_files(void);
static void remove_temp_dir(void);
static void new_temp_path(const char *tag);
static const char *new_temp_dir(const char *tag);
static bool write_file(const char *path, const char *text);
static long read_file(const char *path, char *buf, size_t cap);
static cJSON *parse_file(const char *path);
static cJSONConfigur_Schema_t *new_default_schema(void);

static void test_schema_create_destroy(void);
static void test_schema_declare_null_args(void);
static void test_pointer_format_declare_invalid(void);
static void test_schema_declare_string_deep_copy(void);
static void test_schema_declare_new_paths(void);
static void test_schema_declare_idempotent_same_family(void);
static void test_schema_declare_conflict_family(void);
static void test_init_empty_schema_empty_object(void);
static void test_init_null_schema_and_path(void);
static void test_init_twice_minus3(void);
static void test_init_missing_file_keeps_defaults(void);
static void test_init_blank_file_keeps_defaults(void);
static void test_init_file_open_error_minus4(void);
static void test_init_bad_json_rollback(void);
static void test_init_top_level_non_object(void);
static void test_init_type_mismatch_rejects(void);
static void test_init_file_null_keeps_default(void);
static void test_init_ignores_undeclared(void);
static void test_init_intermediate_array_keeps_default(void);
static void test_init_file_values_override(void);
static void test_init_path_copied_by_library(void);
static void test_get_before_init_minus2(void);
static void test_get_set_pointer_format_minus1(void);
static void test_get_null_out_minus1(void);
static void test_get_missing_path_minus5(void);
static void test_get_family_mismatch_minus6(void);
static void test_get_number_interop_truncation(void);
static void test_get_bool_double_values(void);
static void test_get_string_buffer_contract(void);
static void test_get_nested_and_escaped(void);
static void test_set_before_init_minus2(void);
static void test_set_string_replace(void);
static void test_set_string_value_copy(void);
static void test_set_number_interop(void);
static void test_set_bool_replace(void);
static void test_set_missing_path_minus5(void);
static void test_set_type_mismatch_minus6(void);
static void test_save_before_init_minus2(void);
static void test_deinit_before_init_minus2(void);
static void test_save_roundtrip_no_tmp(void);
static void test_save_tmp_create_fail_minus4(void);
static void test_save_write_fail_minus4(void);
static void test_save_rename_fail_minus4(void);
static void test_save_destructive_overwrite(void);
static void test_deinit_no_autosave(void);
static void test_deinit_then_reinit(void);
/* Private define (auxiliary) ------------------------------------------------*/
/* Private macro (auxiliary) -------------------------------------------------*/
/* Private function code -----------------------------------------------------*/
static void remove_path_pair(const char *path)
{
    char tmp_path[TEST_PATH_MAX + 8];

    (void)snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    (void)remove(tmp_path);
    (void)remove(path);
}

static void remove_temp_files(void)
{
    if (g_cfg_path[0] != '\0')
    {
        remove_path_pair(g_cfg_path);
        g_cfg_path[0] = '\0';
    }
}

static void remove_temp_dir(void)
{
    if (g_cfg_dir[0] != '\0')
    {
        char target[TEST_PATH_MAX * 2];              /* <dir>/<basename> 最坏组合长度 */
        char tmp_path[TEST_PATH_MAX * 2 + 8];
        const char *name = strrchr(g_cfg_dir, '/') + 1;

        (void)chmod(g_cfg_dir, 0755);                /* 恢复可写后再清理 */
        (void)snprintf(target, sizeof(target), "%s/%s", g_cfg_dir, name);
        (void)snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", target);
        (void)remove(tmp_path);
        (void)remove(target);
        (void)rmdir(g_cfg_dir);
        g_cfg_dir[0] = '\0';
    }
}

static void new_temp_path(const char *tag)
{
    remove_temp_files();
    (void)snprintf(g_cfg_path, sizeof(g_cfg_path), TEST_PATH_FMT, tag);
    remove_path_pair(g_cfg_path);
}

static const char *new_temp_dir(const char *tag)
{
    remove_temp_dir();
    (void)snprintf(g_cfg_dir, sizeof(g_cfg_dir), TEST_DIR_FMT, tag);
    (void)rmdir(g_cfg_dir);
    TEST_ASSERT_EQUAL_INT(0, mkdir(g_cfg_dir, 0755));
    return g_cfg_dir;
}

static bool write_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");
    size_t len = strlen(text);
    bool ok;

    if (fp == NULL)
    {
        return false;
    }
    ok = (fwrite(text, 1, len, fp) == len);
    ok = (fclose(fp) == 0) && ok;
    return ok;
}

static long read_file(const char *path, char *buf, size_t cap)
{
    FILE *fp = fopen(path, "rb");
    size_t nread;
    bool read_error;

    if (fp == NULL)
    {
        return -1;
    }
    nread = fread(buf, 1, cap, fp);
    read_error = ferror(fp);
    (void)fclose(fp);
    if (read_error)
    {
        return -1;
    }
    return (long)nread;
}

static cJSON *parse_file(const char *path)
{
    char buf[TEST_FILE_BUF_MAX];
    long len = read_file(path, buf, sizeof(buf) - 1);

    if (len < 0)
    {
        return NULL;
    }
    buf[len] = '\0';
    return cJSON_Parse(buf);
}

static cJSONConfigur_Schema_t *new_default_schema(void)
{
    cJSONConfigur_Schema_t *s = cJSONConfigur_SchemaCreate();

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/server/host", "127.0.0.1"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareNumber(s, "/server/port", 8080.0));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/log/level", "info"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareBool(s, "/log/enable", true));
    return s;
}

/* Public application code ---------------------------------------------------*/

/* Unity Test Setup / Teardown -----------------------------------------------*/
void setUp(void)
{
    /* 保证单例在每个用例前处于未初始化状态 (必要时先 Deinit) */
    (void)cJSONConfigur_Deinit();
}

void tearDown(void)
{
    (void)cJSONConfigur_Deinit();
    remove_temp_files();                     /* 删除本用例创建的临时文件与 *.tmp */
    remove_temp_dir();                       /* 删除本用例创建的临时目录及其内容 */
}

/* §3 SchemaCreate/SchemaDestroy: 创建成功, Destroy 接受 NULL 为空操作 */
static void test_schema_create_destroy(void)
{
    cJSONConfigur_Schema_t *s = cJSONConfigur_SchemaCreate();

    TEST_ASSERT_NOT_NULL(s);
    cJSONConfigur_SchemaDestroy(s);

    cJSONConfigur_SchemaDestroy(NULL);       /* NULL 空操作后库仍可用 */
    s = cJSONConfigur_SchemaCreate();
    TEST_ASSERT_NOT_NULL(s);
    cJSONConfigur_SchemaDestroy(s);
}

/* §3 参数检查: schema/pointer 为 NULL 返回 -1, String 另查 default_value 为 NULL */
static void test_schema_declare_null_args(void)
{
    cJSONConfigur_Schema_t *s = cJSONConfigur_SchemaCreate();

    TEST_ASSERT_NOT_NULL(s);

    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareString(NULL, "/k", "v"));
    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareNumber(NULL, "/k", 1.0));
    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareBool(NULL, "/k", true));

    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareString(s, NULL, "v"));
    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareNumber(s, NULL, 1.0));
    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareBool(s, NULL, true));

    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareString(s, "/ks", NULL));

    cJSONConfigur_SchemaDestroy(s);
}

/* §1 pointer 格式: NULL/不以 / 开头/~ 后未紧跟 0 或 1 对声明接口一律 -1 */
static void test_pointer_format_declare_invalid(void)
{
    cJSONConfigur_Schema_t *s = cJSONConfigur_SchemaCreate();

    TEST_ASSERT_NOT_NULL(s);

    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareString(s, "server", "v"));
    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareString(s, "server/port", "v"));
    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareString(s, "/bad~", "v"));
    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareString(s, "/bad~2", "v"));
    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareString(s, "/bad~~", "v"));
    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareString(s, "/~", "v"));

    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareNumber(s, "server/port", 1.0));
    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareNumber(s, "/bad~2", 1.0));

    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareBool(s, "flag", true));
    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SchemaDeclareBool(s, "/bad~~", true));

    cJSONConfigur_SchemaDestroy(s);
}

/* §3 数据生命周期: 默认值以值拷贝进声明表, 字符串由库深拷贝 */
static void test_schema_declare_string_deep_copy(void)
{
    cJSONConfigur_Schema_t *s = cJSONConfigur_SchemaCreate();
    char default_value[16];
    char out[TEST_STR_BUF_MAX];

    TEST_ASSERT_NOT_NULL(s);
    (void)snprintf(default_value, sizeof(default_value), "origin");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/k", default_value));

    memset(default_value, 'Z', sizeof(default_value));   /* 破坏调用者缓冲 */

    new_temp_path("decl_deepcopy");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/k", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("origin", out);
}

/* §3 路径不存在: 按类型创建, 缺失中间节点自动补为 object, 返回 0 */
static void test_schema_declare_new_paths(void)
{
    cJSONConfigur_Schema_t *s = cJSONConfigur_SchemaCreate();
    char out[TEST_STR_BUF_MAX];
    double d = 0.0;
    bool b = false;

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/a", "x"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareNumber(s, "/b/c/d", 3.0));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareBool(s, "/b/e", true));

    new_temp_path("decl_new");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/a", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("x", out);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetDouble("/b/c/d", &d));
    TEST_ASSERT_TRUE(fabs(d - 3.0) < TEST_DOUBLE_EPS);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetBool("/b/e", &b));
    TEST_ASSERT_TRUE(b);
}

/* §3 同族幂等: 保留首个默认值返回 0 */
static void test_schema_declare_idempotent_same_family(void)
{
    cJSONConfigur_Schema_t *s = cJSONConfigur_SchemaCreate();
    char out[TEST_STR_BUF_MAX];
    double d = 0.0;
    bool b = false;

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/k", "first"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/k", "second"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareNumber(s, "/n", 1.5));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareNumber(s, "/n", 9.75));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareBool(s, "/b", true));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareBool(s, "/b", false));

    new_temp_path("decl_idem");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));   /* 空文件: 纯默认值树 */

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/k", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("first", out);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetDouble("/n", &d));
    TEST_ASSERT_TRUE(fabs(d - 1.5) < TEST_DOUBLE_EPS);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetBool("/b", &b));
    TEST_ASSERT_TRUE(b);
}

/* §3 异族类型返回 -6 (object 与标量族互异), 声明表不改动 */
static void test_schema_declare_conflict_family(void)
{
    cJSONConfigur_Schema_t *s = cJSONConfigur_SchemaCreate();
    char out[TEST_STR_BUF_MAX];
    double d = 0.0;
    bool b = false;

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/k", "v"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareNumber(s, "/n", 2.0));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareBool(s, "/b", true));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/p/q", "pq"));   /* /p 为 object */

    /* 叶路径已存在, 标量-标量异族 */
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SchemaDeclareNumber(s, "/k", 1.0));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SchemaDeclareBool(s, "/k", true));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SchemaDeclareString(s, "/n", "x"));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SchemaDeclareBool(s, "/n", false));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SchemaDeclareString(s, "/b", "x"));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SchemaDeclareNumber(s, "/b", 1.0));

    /* 叶路径已存在且为 object */
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SchemaDeclareNumber(s, "/p", 1.0));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SchemaDeclareString(s, "/p", "x"));

    /* 中间节点已存在且为标量: 无法挂载子树 */
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SchemaDeclareBool(s, "/k/v", true));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SchemaDeclareNumber(s, "/b/x/y", 1.0));

    new_temp_path("decl_conflict");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/k", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("v", out);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetDouble("/n", &d));
    TEST_ASSERT_TRUE(fabs(d - 2.0) < TEST_DOUBLE_EPS);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetBool("/b", &b));
    TEST_ASSERT_TRUE(b);

    /* -6 未改动声明表: /k 无子树, /p 保持 object */
    TEST_ASSERT_EQUAL_INT(-5, cJSONConfigur_GetString("/k/v", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_GetString("/p", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/p/q", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("pq", out);
}

/* §3 空声明表合法: Init 后配置树为空 object */
static void test_init_empty_schema_empty_object(void)
{
    cJSONConfigur_Schema_t *s = cJSONConfigur_SchemaCreate();
    char out[TEST_STR_BUF_MAX];
    int iv = 0;
    cJSON *root = NULL;

    TEST_ASSERT_NOT_NULL(s);
    new_temp_path("init_empty");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));

    TEST_ASSERT_EQUAL_INT(-5, cJSONConfigur_GetString("/any", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(-5, cJSONConfigur_GetInt("/any", &iv));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Save());
    root = parse_file(g_cfg_path);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_TRUE(cJSON_IsObject(root));
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(root));
    cJSON_Delete(root);
}

/* §3 Init 参数检查: schema == NULL / path == NULL / path 空串返回 -1, 不改变未初始化状态 */
static void test_init_null_schema_and_path(void)
{
    char out[TEST_STR_BUF_MAX];

    new_temp_path("init_args");

    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_Init(NULL, g_cfg_path));
    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_Init(cJSONConfigur_SchemaCreate(), NULL));
    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_Init(cJSONConfigur_SchemaCreate(), ""));

    TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
}

/* §3 已初始化再 Init 返回 -3, 既有实例不受影响 */
static void test_init_twice_minus3(void)
{
    cJSONConfigur_Schema_t *s = new_default_schema();
    char out[TEST_STR_BUF_MAX];

    new_temp_path("init_twice");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));

    TEST_ASSERT_EQUAL_INT(-3, cJSONConfigur_Init(new_default_schema(), g_cfg_path));
    TEST_ASSERT_EQUAL_INT(-3, cJSONConfigur_Init(NULL, g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("127.0.0.1", out);
}

/* §3 文件不存在 (ENOENT): 保留默认值返回 0 */
static void test_init_missing_file_keeps_defaults(void)
{
    cJSONConfigur_Schema_t *s = new_default_schema();
    char out[TEST_STR_BUF_MAX];
    int port = 0;
    bool enable = false;

    new_temp_path("init_missing");           /* 不创建该文件 */
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("127.0.0.1", out);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetInt("/server/port", &port));
    TEST_ASSERT_EQUAL_INT(8080, port);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/log/level", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("info", out);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetBool("/log/enable", &enable));
    TEST_ASSERT_TRUE(enable);
}

/* §3 文件为空 / 纯空白: 保留默认值返回 0 */
static void test_init_blank_file_keeps_defaults(void)
{
    cJSONConfigur_Schema_t *s = NULL;
    char out[TEST_STR_BUF_MAX];

    new_temp_path("init_blank");

    /* 空文件 */
    s = new_default_schema();
    TEST_ASSERT_TRUE(write_file(g_cfg_path, ""));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("127.0.0.1", out);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Deinit());

    /* 纯空白文件 */
    s = new_default_schema();
    TEST_ASSERT_TRUE(write_file(g_cfg_path, " \t\r\n  "));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/log/level", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("info", out);
}

/* §3 文件打开失败且错误非 ENOENT: 返回 -4, 单例回到未初始化 */
static void test_init_file_open_error_minus4(void)
{
    char out[TEST_STR_BUF_MAX];

    if (geteuid() == 0)
    {
        TEST_IGNORE_MESSAGE("root bypasses file permissions; injection unavailable");
    }

    new_temp_path("init_eacces");
    TEST_ASSERT_TRUE(write_file(g_cfg_path, "{}"));
    TEST_ASSERT_EQUAL_INT(0, chmod(g_cfg_path, 0000));

    TEST_ASSERT_EQUAL_INT(-4, cJSONConfigur_Init(new_default_schema(), g_cfg_path));
    TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
}

/* §3 解析失败返回 -4: 全量回滚到未初始化, 目标文件字节不变, 修复后可重新 Init */
static void test_init_bad_json_rollback(void)
{
    char before[TEST_FILE_BUF_MAX];
    char after[TEST_FILE_BUF_MAX];
    char out[TEST_STR_BUF_MAX];
    long before_len = 0;
    long after_len = 0;

    new_temp_path("init_badjson");
    TEST_ASSERT_TRUE(write_file(g_cfg_path, "{\"server\": {\"host\""));
    before_len = read_file(g_cfg_path, before, sizeof(before));
    TEST_ASSERT_TRUE(before_len > 0);

    TEST_ASSERT_EQUAL_INT(-4, cJSONConfigur_Init(new_default_schema(), g_cfg_path));

    TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_Deinit());

    after_len = read_file(g_cfg_path, after, sizeof(after));
    TEST_ASSERT_EQUAL_INT(before_len, after_len);
    TEST_ASSERT_EQUAL_MEMORY(before, after, (size_t)before_len);

    /* 修复文件后可重新 Init */
    TEST_ASSERT_TRUE(write_file(g_cfg_path, "{\"server\": {\"host\": \"h\"}}"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("h", out);
}

/* §3 顶层非 object (标量/数组/null) 返回 -4, 单例回到未初始化 */
static void test_init_top_level_non_object(void)
{
    static const char *bads[] = {"[1, 2, 3]", "42", "\"text\"", "null", "true"};
    char out[TEST_STR_BUF_MAX];
    size_t i = 0;

    new_temp_path("init_toplevel");
    for (i = 0; i < (sizeof(bads) / sizeof(bads[0])); i++)
    {
        TEST_ASSERT_TRUE(write_file(g_cfg_path, bads[i]));
        TEST_ASSERT_EQUAL_INT(-4, cJSONConfigur_Init(new_default_schema(), g_cfg_path));
        TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
    }
}

/* §3 文件值类型不符整体拒绝返回 -4 (错误侧含 number/string/bool/object/array), 单例回到未初始化 */
static void test_init_type_mismatch_rejects(void)
{
    static const char *bads[] = {
        "{\"server\": {\"host\": 1}}",                                      /* number  vs string */
        "{\"server\": {\"port\": \"8080\"}}",                               /* string  vs number */
        "{\"log\": {\"enable\": 1}}",                                       /* number  vs bool   */
        "{\"log\": {\"enable\": \"true\"}}",                                /* string  vs bool   */
        "{\"server\": {\"host\": true}}",                                   /* bool    vs string */
        "{\"server\": {\"port\": {\"x\": 1}}}",                             /* object  vs number */
        "{\"server\": {\"host\": [1]}}",                                    /* array   vs string */
        "{\"server\": {\"host\": \"ok\"}, \"log\": {\"enable\": \"bad\"}}"  /* 一项相符一项不符: 整体拒绝 */
    };
    char out[TEST_STR_BUF_MAX];
    size_t i = 0;

    new_temp_path("init_mismatch");
    for (i = 0; i < (sizeof(bads) / sizeof(bads[0])); i++)
    {
        TEST_ASSERT_TRUE(write_file(g_cfg_path, bads[i]));
        TEST_ASSERT_EQUAL_INT(-4, cJSONConfigur_Init(new_default_schema(), g_cfg_path));
        TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
    }
}

/* §3 文件该项为 null: 保留默认值 */
static void test_init_file_null_keeps_default(void)
{
    char out[TEST_STR_BUF_MAX];
    int port = 0;
    bool enable = false;

    new_temp_path("init_null");
    TEST_ASSERT_TRUE(write_file(g_cfg_path,
        "{\"server\": {\"host\": null, \"port\": null}, \"log\": {\"level\": null, \"enable\": null}}"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("127.0.0.1", out);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetInt("/server/port", &port));
    TEST_ASSERT_EQUAL_INT(8080, port);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/log/level", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("info", out);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetBool("/log/enable", &enable));
    TEST_ASSERT_TRUE(enable);
}

/* §3 文件中未声明的内容一律忽略, 不进入配置树 */
static void test_init_ignores_undeclared(void)
{
    char out[TEST_STR_BUF_MAX];
    int iv = 0;

    new_temp_path("init_ignore");
    TEST_ASSERT_TRUE(write_file(g_cfg_path,
        "{\"server\": {\"host\": \"1.2.3.4\", \"extra\": \"x\"}, \"other\": [1, 2], \"num\": 7}"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("1.2.3.4", out);
    TEST_ASSERT_EQUAL_INT(-5, cJSONConfigur_GetString("/server/extra", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(-5, cJSONConfigur_GetInt("/num", &iv));
    TEST_ASSERT_EQUAL_INT(-5, cJSONConfigur_GetString("/other", out, sizeof(out)));
}

/* §3 声明路径中间节点在文件中为数组: 定位失败, 该子树保留默认值 */
static void test_init_intermediate_array_keeps_default(void)
{
    cJSONConfigur_Schema_t *s = cJSONConfigur_SchemaCreate();
    int v = 0;

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareNumber(s, "/a/b", 7.0));

    new_temp_path("init_arr");
    TEST_ASSERT_TRUE(write_file(g_cfg_path, "{\"a\": [1, 2]}"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetInt("/a/b", &v));
    TEST_ASSERT_EQUAL_INT(7, v);
}

/* §3 类型匹配则采用文件值, 文件无该项保留默认值 */
static void test_init_file_values_override(void)
{
    char out[TEST_STR_BUF_MAX];
    int port = 0;
    bool enable = false;

    new_temp_path("init_override");
    TEST_ASSERT_TRUE(write_file(g_cfg_path,
        "{\"server\": {\"host\": \"10.0.0.1\", \"port\": 9090}, \"log\": {\"enable\": false}}"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("10.0.0.1", out);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetInt("/server/port", &port));
    TEST_ASSERT_EQUAL_INT(9090, port);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetBool("/log/enable", &enable));
    TEST_ASSERT_FALSE(enable);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/log/level", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("info", out);
}

/* §3 数据生命周期: path 为借用输入, 成功后由库拷贝保存 */
static void test_init_path_copied_by_library(void)
{
    cJSONConfigur_Schema_t *s = cJSONConfigur_SchemaCreate();
    char path_buf[TEST_PATH_MAX];
    char out[TEST_STR_BUF_MAX];
    cJSON *root = NULL;
    cJSON *k = NULL;

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/k", "v"));

    new_temp_path("path_copy");
    (void)snprintf(path_buf, sizeof(path_buf), "%s", g_cfg_path);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, path_buf));

    memset(path_buf, 'Z', sizeof(path_buf));             /* 破坏调用者缓冲 */

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SetString("/k", "saved"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Save());      /* 仍写 Init 时的原路径 */

    root = parse_file(g_cfg_path);
    TEST_ASSERT_NOT_NULL(root);
    k = cJSON_GetObjectItemCaseSensitive(root, "k");
    TEST_ASSERT_TRUE(cJSON_IsString(k));
    TEST_ASSERT_EQUAL_STRING("saved", k->valuestring);
    cJSON_Delete(root);

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/k", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("saved", out);
}

/* §3 单例未初始化 Get* 返回 -2 */
static void test_get_before_init_minus2(void)
{
    char out[TEST_STR_BUF_MAX];
    int iv = 0;
    bool bv = false;
    double dv = 0.0;

    TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_GetString("/any", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_GetInt("/any", &iv));
    TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_GetBool("/any", &bv));
    TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_GetDouble("/any", &dv));
}

/* §1 pointer 格式对 Get/Set 族一律 -1; SetString 另查 value == NULL */
static void test_get_set_pointer_format_minus1(void)
{
    static const char *bads[] = {NULL, "server", "server/port", "/x~", "/x~2", "/x~~", "/~"};
    char out[TEST_STR_BUF_MAX];
    int iv = 0;
    bool bv = false;
    double dv = 0.0;
    size_t i = 0;

    new_temp_path("fmt");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));

    for (i = 0; i < (sizeof(bads) / sizeof(bads[0])); i++)
    {
        TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_GetString(bads[i], out, sizeof(out)));
        TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_GetInt(bads[i], &iv));
        TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_GetBool(bads[i], &bv));
        TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_GetDouble(bads[i], &dv));
        TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SetString(bads[i], "v"));
        TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SetInt(bads[i], 1));
        TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SetBool(bads[i], true));
        TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SetDouble(bads[i], 1.0));
    }

    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_SetString("/server/host", NULL));
}

/* §3 输出指针为 NULL 返回 -1 */
static void test_get_null_out_minus1(void)
{
    char out[TEST_STR_BUF_MAX];

    new_temp_path("get_nullout");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));

    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_GetString("/server/host", NULL, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_GetInt("/server/port", NULL));
    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_GetBool("/log/enable", NULL));
    TEST_ASSERT_EQUAL_INT(-1, cJSONConfigur_GetDouble("/server/port", NULL));
}

/* §3 路径不存在返回 -5 (含中间节点为标量无法定位) */
static void test_get_missing_path_minus5(void)
{
    char out[TEST_STR_BUF_MAX];
    int iv = 0;

    new_temp_path("get_missing");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));

    TEST_ASSERT_EQUAL_INT(-5, cJSONConfigur_GetString("/nope", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(-5, cJSONConfigur_GetInt("/server/nope", &iv));
    TEST_ASSERT_EQUAL_INT(-5, cJSONConfigur_GetString("/server/host/deeper", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(-5, cJSONConfigur_GetInt("/log/enable/x", &iv));
}

/* §3 异族读取返回 -6 */
static void test_get_family_mismatch_minus6(void)
{
    char out[TEST_STR_BUF_MAX];
    int iv = 0;
    bool bv = false;
    double dv = 0.0;

    new_temp_path("get_family");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));

    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_GetInt("/server/host", &iv));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_GetDouble("/server/host", &dv));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_GetBool("/server/host", &bv));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_GetString("/server/port", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_GetBool("/server/port", &bv));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_GetString("/log/enable", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_GetInt("/log/enable", &iv));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_GetDouble("/log/enable", &dv));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_GetString("/server", out, sizeof(out)));   /* 目标为 object */
}

/* §3 Number 族内 GetInt / GetDouble 互通, GetInt 截断取整数部分 */
static void test_get_number_interop_truncation(void)
{
    cJSONConfigur_Schema_t *s = new_default_schema();
    int v = 0;
    double d = 0.0;

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareNumber(s, "/frac", 3.8));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareNumber(s, "/neg", -2.5));

    new_temp_path("get_num");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetInt("/server/port", &v));
    TEST_ASSERT_EQUAL_INT(8080, v);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetDouble("/server/port", &d));
    TEST_ASSERT_TRUE(fabs(d - 8080.0) < TEST_DOUBLE_EPS);

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetInt("/frac", &v));
    TEST_ASSERT_EQUAL_INT(3, v);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetDouble("/frac", &d));
    TEST_ASSERT_TRUE(fabs(d - 3.8) < TEST_DOUBLE_EPS);

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetInt("/neg", &v));
    TEST_ASSERT_EQUAL_INT(-2, v);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetDouble("/neg", &d));
    TEST_ASSERT_TRUE(fabs(d + 2.5) < TEST_DOUBLE_EPS);
}

/* §3 布尔与浮点取值正确 */
static void test_get_bool_double_values(void)
{
    cJSONConfigur_Schema_t *s = new_default_schema();
    bool b = true;
    double d = 0.0;

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareBool(s, "/off", false));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareNumber(s, "/pi", 3.14159));

    new_temp_path("get_vals");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetBool("/log/enable", &b));
    TEST_ASSERT_TRUE(b);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetBool("/off", &b));
    TEST_ASSERT_FALSE(b);

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetDouble("/pi", &d));
    TEST_ASSERT_TRUE(fabs(d - 3.14159) < TEST_DOUBLE_EPS);
}

/* §3 GetString: 完整拷贝返回 0; 截断拷贝前 size-1 字节加 '\0' 返回 -7; size == 0 返回 -7 */
static void test_get_string_buffer_contract(void)
{
    cJSONConfigur_Schema_t *s = cJSONConfigur_SchemaCreate();
    char out[TEST_STR_BUF_MAX];

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/k", "abcdef"));

    new_temp_path("get_strbuf");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));

    memset(out, 0, sizeof(out));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/k", out, 16));
    TEST_ASSERT_EQUAL_STRING("abcdef", out);

    /* 恰好 strlen + 1: 完整拷贝 */
    memset(out, 0, sizeof(out));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/k", out, 7));
    TEST_ASSERT_EQUAL_STRING("abcdef", out);

    /* 截断: 前 size-1 字节 + '\0' */
    memset(out, 'X', sizeof(out));
    TEST_ASSERT_EQUAL_INT(-7, cJSONConfigur_GetString("/k", out, 6));
    TEST_ASSERT_EQUAL_STRING_LEN("abcde", out, 5);
    TEST_ASSERT_EQUAL_CHAR('\0', out[5]);

    memset(out, 'X', sizeof(out));
    TEST_ASSERT_EQUAL_INT(-7, cJSONConfigur_GetString("/k", out, 3));
    TEST_ASSERT_EQUAL_STRING_LEN("ab", out, 2);
    TEST_ASSERT_EQUAL_CHAR('\0', out[2]);

    /* size == 0: -7 且不写 out */
    memset(out, 'X', sizeof(out));
    TEST_ASSERT_EQUAL_INT(-7, cJSONConfigur_GetString("/k", out, 0));
    TEST_ASSERT_EQUAL_CHAR('X', out[0]);

    /* size == 1: 仅写 '\0' */
    memset(out, 'X', sizeof(out));
    TEST_ASSERT_EQUAL_INT(-7, cJSONConfigur_GetString("/k", out, 1));
    TEST_ASSERT_EQUAL_CHAR('\0', out[0]);
}

/* §1 嵌套路径与 ~0 / ~1 转义成员名取值正确 */
static void test_get_nested_and_escaped(void)
{
    cJSONConfigur_Schema_t *s = cJSONConfigur_SchemaCreate();
    char out[TEST_STR_BUF_MAX];
    double d = 0.0;

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/a/b/c", "deep"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/m~1x", "slash"));   /* 成员名 "m/x" */
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareNumber(s, "/n~0x", 5.5));       /* 成员名 "n~x" */

    new_temp_path("get_nested");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/a/b/c", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("deep", out);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/m~1x", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("slash", out);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetDouble("/n~0x", &d));
    TEST_ASSERT_TRUE(fabs(d - 5.5) < TEST_DOUBLE_EPS);
}

/* §3 单例未初始化 Set* 返回 -2 */
static void test_set_before_init_minus2(void)
{
    TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_SetString("/any", "v"));
    TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_SetInt("/any", 1));
    TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_SetBool("/any", true));
    TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_SetDouble("/any", 1.0));
}

/* §3 同族标量替换值, Get 可见 */
static void test_set_string_replace(void)
{
    char out[TEST_STR_BUF_MAX];

    new_temp_path("set_str");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SetString("/server/host", "9.9.9.9"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("9.9.9.9", out);

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SetString("/log/level", "debug"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/log/level", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("debug", out);
}

/* §3 数据生命周期: Set 的值拷贝进 JSON 树 */
static void test_set_string_value_copy(void)
{
    char value[16];
    char out[TEST_STR_BUF_MAX];

    new_temp_path("set_copy");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));

    (void)snprintf(value, sizeof(value), "temp");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SetString("/log/level", value));

    memset(value, 'Z', sizeof(value));                   /* 破坏调用者缓冲 */

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/log/level", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("temp", out);
}

/* §3 SetInt / SetDouble 写 Number 项, 族内互通 */
static void test_set_number_interop(void)
{
    int v = 0;
    double d = 0.0;

    new_temp_path("set_num");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SetInt("/server/port", 1234));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetInt("/server/port", &v));
    TEST_ASSERT_EQUAL_INT(1234, v);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetDouble("/server/port", &d));
    TEST_ASSERT_TRUE(fabs(d - 1234.0) < TEST_DOUBLE_EPS);

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SetDouble("/server/port", 56.75));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetDouble("/server/port", &d));
    TEST_ASSERT_TRUE(fabs(d - 56.75) < TEST_DOUBLE_EPS);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetInt("/server/port", &v));
    TEST_ASSERT_EQUAL_INT(56, v);
}

/* §3 SetBool 同族替换 */
static void test_set_bool_replace(void)
{
    bool b = true;

    new_temp_path("set_bool");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SetBool("/log/enable", false));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetBool("/log/enable", &b));
    TEST_ASSERT_FALSE(b);

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SetBool("/log/enable", true));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetBool("/log/enable", &b));
    TEST_ASSERT_TRUE(b);
}

/* §3 路径不存在 (含中间节点缺失/为标量) 返回 -5, 不创建任何节点 */
static void test_set_missing_path_minus5(void)
{
    char out[TEST_STR_BUF_MAX];
    int iv = 0;
    cJSON *root = NULL;

    new_temp_path("set_missing");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));

    TEST_ASSERT_EQUAL_INT(-5, cJSONConfigur_SetString("/nope", "v"));
    TEST_ASSERT_EQUAL_INT(-5, cJSONConfigur_SetInt("/new/x", 1));
    TEST_ASSERT_EQUAL_INT(-5, cJSONConfigur_SetInt("/server/x/y", 1));
    TEST_ASSERT_EQUAL_INT(-5, cJSONConfigur_SetString("/server/host/deep", "v"));

    TEST_ASSERT_EQUAL_INT(-5, cJSONConfigur_GetString("/nope", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(-5, cJSONConfigur_GetInt("/new/x", &iv));

    /* Save 后文件中也不存在这些节点 */
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Save());
    root = parse_file(g_cfg_path);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NULL(cJSON_GetObjectItemCaseSensitive(root, "nope"));
    TEST_ASSERT_NULL(cJSON_GetObjectItemCaseSensitive(root, "new"));
    cJSON_Delete(root);
}

/* §3 目标为 object 或异族标量返回 -6, 原值不被破坏 */
static void test_set_type_mismatch_minus6(void)
{
    char out[TEST_STR_BUF_MAX];
    int v = 0;

    new_temp_path("set_mismatch");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));

    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SetInt("/server/host", 1));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SetDouble("/server/host", 1.0));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SetBool("/server/host", true));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SetString("/server/port", "x"));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SetBool("/server/port", true));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SetString("/log/enable", "x"));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SetInt("/log/enable", 1));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SetDouble("/log/enable", 1.0));

    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SetString("/server", "x"));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SetInt("/server", 1));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SetBool("/log", false));
    TEST_ASSERT_EQUAL_INT(-6, cJSONConfigur_SetDouble("/log", 1.0));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("127.0.0.1", out);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetInt("/server/port", &v));
    TEST_ASSERT_EQUAL_INT(8080, v);
}

/* §3 单例未初始化 Save 返回 -2 */
static void test_save_before_init_minus2(void)
{
    TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_Save());
}

/* §3 单例未初始化 Deinit 返回 -2 */
static void test_deinit_before_init_minus2(void)
{
    TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_Deinit());
}

/* §3 Save: 写回合法 JSON 与内存一致, *.tmp 不残留, 重新 Init 读回一致 */
static void test_save_roundtrip_no_tmp(void)
{
    char tmp_path[TEST_PATH_MAX + 8];
    char out[TEST_STR_BUF_MAX];
    int port = 0;
    bool enable = false;
    FILE *fp = NULL;
    cJSON *root = NULL;
    cJSON *host = NULL;
    cJSON *level = NULL;

    new_temp_path("save_round");
    TEST_ASSERT_TRUE(write_file(g_cfg_path,
        "{\"server\": {\"host\": \"10.0.0.1\", \"port\": 9090}}"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SetString("/log/level", "warn"));

    (void)snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_cfg_path);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Save());

    fp = fopen(tmp_path, "rb");
    TEST_ASSERT_NULL(fp);                    /* *.tmp 不残留 */

    root = parse_file(g_cfg_path);
    TEST_ASSERT_NOT_NULL(root);
    host = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root, "server"), "host");
    TEST_ASSERT_TRUE(cJSON_IsString(host));
    TEST_ASSERT_EQUAL_STRING("10.0.0.1", host->valuestring);
    level = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root, "log"), "level");
    TEST_ASSERT_TRUE(cJSON_IsString(level));
    TEST_ASSERT_EQUAL_STRING("warn", level->valuestring);
    cJSON_Delete(root);

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Deinit());
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("10.0.0.1", out);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetInt("/server/port", &port));
    TEST_ASSERT_EQUAL_INT(9090, port);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/log/level", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("warn", out);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetBool("/log/enable", &enable));
    TEST_ASSERT_TRUE(enable);
}

/* §3 Save: 临时文件创建失败返回 -4, 目标文件保持完整旧内容 */
static void test_save_tmp_create_fail_minus4(void)
{
    const char *dir = NULL;
    const char *name = NULL;
    cJSONConfigur_Schema_t *s = NULL;
    char target[TEST_PATH_MAX];
    char tmp_path[TEST_PATH_MAX + 8];
    char out[TEST_STR_BUF_MAX];
    FILE *fp = NULL;
    cJSON *root = NULL;
    cJSON *k = NULL;

    if (geteuid() == 0)
    {
        TEST_IGNORE_MESSAGE("root bypasses file permissions; injection unavailable");
    }

    dir = new_temp_dir("save_rofail");
    name = strrchr(dir, '/') + 1;
    (void)snprintf(target, sizeof(target), "%s/%s", dir, name);
    TEST_ASSERT_TRUE(write_file(target, "{\"k\": \"stable\"}"));
    TEST_ASSERT_EQUAL_INT(0, chmod(dir, 0555));          /* 目录只读: *.tmp 无法创建 */

    s = cJSONConfigur_SchemaCreate();
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/k", "old"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, target));   /* 目标可读: Init 正常 */

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/k", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("stable", out);

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SetString("/k", "changed"));
    TEST_ASSERT_EQUAL_INT(-4, cJSONConfigur_Save());

    (void)snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", target);
    fp = fopen(tmp_path, "rb");
    TEST_ASSERT_NULL(fp);                    /* 失败路径不残留临时文件 */

    root = parse_file(target);
    TEST_ASSERT_NOT_NULL(root);
    k = cJSON_GetObjectItemCaseSensitive(root, "k");
    TEST_ASSERT_TRUE(cJSON_IsString(k));
    TEST_ASSERT_EQUAL_STRING("stable", k->valuestring);   /* 目标文件未被破坏 */
    cJSON_Delete(root);
}

/* §3 Save: 写入失败返回 -4, 临时文件删除, 目标文件保持完整旧内容 */
static void test_save_write_fail_minus4(void)
{
    struct rlimit old_limit;
    struct rlimit tiny_limit;
    cJSONConfigur_Schema_t *s = NULL;
    char tmp_path[TEST_PATH_MAX + 8];
    char out[TEST_STR_BUF_MAX];
    char long_value[TEST_STR_BUF_MAX];
    FILE *fp = NULL;
    cJSON *root = NULL;
    cJSON *k = NULL;
    int save_rc = 0;

    memset(long_value, 'x', sizeof(long_value) - 1);
    long_value[sizeof(long_value) - 1] = '\0';

    new_temp_path("save_wrfail");
    TEST_ASSERT_TRUE(write_file(g_cfg_path, "{\"k\": \"stable\"}"));
    s = cJSONConfigur_SchemaCreate();
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/k", "old"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SetString("/k", long_value));   /* 序列化长度超过写入上限 */

    TEST_ASSERT_EQUAL_INT(0, getrlimit(RLIMIT_FSIZE, &old_limit));
    tiny_limit = old_limit;
    tiny_limit.rlim_cur = 16;                /* 只降软限, 硬限不动, 事后可完整恢复 */
    TEST_ASSERT_EQUAL_INT(0, setrlimit(RLIMIT_FSIZE, &tiny_limit));
    (void)signal(SIGXFSZ, SIG_IGN);          /* 超限以 EFBIG 失败而非终止进程 */
    save_rc = cJSONConfigur_Save();
    (void)signal(SIGXFSZ, SIG_DFL);
    TEST_ASSERT_EQUAL_INT(0, setrlimit(RLIMIT_FSIZE, &old_limit));

    TEST_ASSERT_EQUAL_INT(-4, save_rc);

    (void)snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_cfg_path);
    fp = fopen(tmp_path, "rb");
    TEST_ASSERT_NULL(fp);                    /* 失败路径不残留临时文件 */

    root = parse_file(g_cfg_path);
    TEST_ASSERT_NOT_NULL(root);
    k = cJSON_GetObjectItemCaseSensitive(root, "k");
    TEST_ASSERT_TRUE(cJSON_IsString(k));
    TEST_ASSERT_EQUAL_STRING("stable", k->valuestring);   /* 目标文件未被破坏 */
    cJSON_Delete(root);

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/k", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(long_value, out);            /* 内存配置不受 Save 失败影响 */
}

/* §3 Save: rename 失败返回 -4, 临时文件删除, 内存配置不变 */
static void test_save_rename_fail_minus4(void)
{
    const char *dir = NULL;
    const char *name = NULL;
    cJSONConfigur_Schema_t *s = NULL;
    char target[TEST_PATH_MAX];
    char tmp_path[TEST_PATH_MAX + 8];
    char out[TEST_STR_BUF_MAX];
    FILE *fp = NULL;

    dir = new_temp_dir("save_rnmfail");
    name = strrchr(dir, '/') + 1;
    (void)snprintf(target, sizeof(target), "%s/%s", dir, name);

    s = cJSONConfigur_SchemaCreate();
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/k", "old"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, target));   /* 文件不存在: 保留默认值 */
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Save());            /* 目标文件已生成 */

    TEST_ASSERT_EQUAL_INT(0, remove(target));                  /* 目标文件替换为同名空目录, rename(file→dir) 失败 */
    TEST_ASSERT_EQUAL_INT(0, mkdir(target, 0755));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SetString("/k", "changed"));
    TEST_ASSERT_EQUAL_INT(-4, cJSONConfigur_Save());

    (void)snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", target);
    fp = fopen(tmp_path, "rb");
    TEST_ASSERT_NULL(fp);                    /* 失败路径不残留临时文件 */

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/k", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("changed", out);   /* 内存配置不受 Save 失败影响 */
}

/* §3 破坏性契约: Save 以内存树整体覆写, 文件中未声明内容被清除 */
static void test_save_destructive_overwrite(void)
{
    cJSONConfigur_Schema_t *s = cJSONConfigur_SchemaCreate();
    cJSON *root = NULL;
    cJSON *server = NULL;
    cJSON *port = NULL;

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareNumber(s, "/server/port", 1.0));

    new_temp_path("save_destr");
    TEST_ASSERT_TRUE(write_file(g_cfg_path,
        "{\"server\": {\"port\": 2, \"gone\": \"x\"}, \"gone_too\": [1]}"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Save());

    root = parse_file(g_cfg_path);
    TEST_ASSERT_NOT_NULL(root);
    server = cJSON_GetObjectItemCaseSensitive(root, "server");
    TEST_ASSERT_TRUE(cJSON_IsObject(server));
    TEST_ASSERT_NULL(cJSON_GetObjectItemCaseSensitive(root, "gone_too"));
    TEST_ASSERT_NULL(cJSON_GetObjectItemCaseSensitive(server, "gone"));
    port = cJSON_GetObjectItemCaseSensitive(server, "port");
    TEST_ASSERT_TRUE(cJSON_IsNumber(port));
    TEST_ASSERT_EQUAL_INT(2, port->valueint);
    cJSON_Delete(root);
}

/* §3 Deinit 不自动保存: Set 后不 Save 直接 Deinit, 文件保持旧值 */
static void test_deinit_no_autosave(void)
{
    cJSONConfigur_Schema_t *s = cJSONConfigur_SchemaCreate();
    char out[TEST_STR_BUF_MAX];
    cJSON *root = NULL;
    cJSON *k = NULL;

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/k", "old"));

    new_temp_path("deinit_no_save");
    TEST_ASSERT_TRUE(write_file(g_cfg_path, "{\"k\": \"old\"}"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SetString("/k", "new"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/k", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("new", out);

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Deinit());

    root = parse_file(g_cfg_path);
    TEST_ASSERT_NOT_NULL(root);
    k = cJSON_GetObjectItemCaseSensitive(root, "k");
    TEST_ASSERT_TRUE(cJSON_IsString(k));
    TEST_ASSERT_EQUAL_STRING("old", k->valuestring);
    cJSON_Delete(root);

    /* 重新 Init 读回旧值 */
    s = cJSONConfigur_SchemaCreate();
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_SchemaDeclareString(s, "/k", "old"));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(s, g_cfg_path));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/k", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("old", out);
}

/* §3 Deinit: 单例回到未初始化, 可重新 Init */
static void test_deinit_then_reinit(void)
{
    char out[TEST_STR_BUF_MAX];

    new_temp_path("deinit_reinit");
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Deinit());

    TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(-2, cJSONConfigur_Save());

    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Init(new_default_schema(), g_cfg_path));
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_GetString("/server/host", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("127.0.0.1", out);
    TEST_ASSERT_EQUAL_INT(0, cJSONConfigur_Deinit());
}

/* Entry point ---------------------------------------------------------------*/
int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_schema_create_destroy);
    RUN_TEST(test_schema_declare_null_args);
    RUN_TEST(test_pointer_format_declare_invalid);
    RUN_TEST(test_schema_declare_string_deep_copy);
    RUN_TEST(test_schema_declare_new_paths);
    RUN_TEST(test_schema_declare_idempotent_same_family);
    RUN_TEST(test_schema_declare_conflict_family);
    RUN_TEST(test_init_empty_schema_empty_object);
    RUN_TEST(test_init_null_schema_and_path);
    RUN_TEST(test_init_twice_minus3);
    RUN_TEST(test_init_missing_file_keeps_defaults);
    RUN_TEST(test_init_blank_file_keeps_defaults);
    RUN_TEST(test_init_file_open_error_minus4);
    RUN_TEST(test_init_bad_json_rollback);
    RUN_TEST(test_init_top_level_non_object);
    RUN_TEST(test_init_type_mismatch_rejects);
    RUN_TEST(test_init_file_null_keeps_default);
    RUN_TEST(test_init_ignores_undeclared);
    RUN_TEST(test_init_intermediate_array_keeps_default);
    RUN_TEST(test_init_file_values_override);
    RUN_TEST(test_init_path_copied_by_library);
    RUN_TEST(test_get_before_init_minus2);
    RUN_TEST(test_get_set_pointer_format_minus1);
    RUN_TEST(test_get_null_out_minus1);
    RUN_TEST(test_get_missing_path_minus5);
    RUN_TEST(test_get_family_mismatch_minus6);
    RUN_TEST(test_get_number_interop_truncation);
    RUN_TEST(test_get_bool_double_values);
    RUN_TEST(test_get_string_buffer_contract);
    RUN_TEST(test_get_nested_and_escaped);
    RUN_TEST(test_set_before_init_minus2);
    RUN_TEST(test_set_string_replace);
    RUN_TEST(test_set_string_value_copy);
    RUN_TEST(test_set_number_interop);
    RUN_TEST(test_set_bool_replace);
    RUN_TEST(test_set_missing_path_minus5);
    RUN_TEST(test_set_type_mismatch_minus6);
    RUN_TEST(test_save_before_init_minus2);
    RUN_TEST(test_deinit_before_init_minus2);
    RUN_TEST(test_save_roundtrip_no_tmp);
    RUN_TEST(test_save_tmp_create_fail_minus4);
    RUN_TEST(test_save_write_fail_minus4);
    RUN_TEST(test_save_rename_fail_minus4);
    RUN_TEST(test_save_destructive_overwrite);
    RUN_TEST(test_deinit_no_autosave);
    RUN_TEST(test_deinit_then_reinit);

    return UNITY_END();
}


#ifdef __cplusplus
}
#endif /* __cplusplus */
