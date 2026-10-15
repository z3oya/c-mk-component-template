/**
  ******************************************************************************
  * File Name          : cjson_configur.h
  * Description        : Header for cjson_configur
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
#ifndef USERAPP_CJSON_CONFIGUR_H
#define USERAPP_CJSON_CONFIGUR_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include <stddef.h>
/* Private includes ----------------------------------------------------------*/
/* Public define -------------------------------------------------------------*/
/* Public macro --------------------------------------------------------------*/
/* Public typedef ------------------------------------------------------------*/
typedef struct cJSONConfigur_Schema cJSONConfigur_Schema_t;
/* Public variables ----------------------------------------------------------*/
/* Public define (typed) -----------------------------------------------------*/
/* Public macro (typed) ------------------------------------------------------*/
/* Public variables (typed) --------------------------------------------------*/
/* Public inline functions ---------------------------------------------------*/
/* Public function prototypes ------------------------------------------------*/
// 创建空声明表 (失败返回 NULL)
cJSONConfigur_Schema_t *cJSONConfigur_SchemaCreate(void);
// 声明字符串配置项 (0 成功, -1 参数非法, -6 类型不匹配, -8 内存分配失败)
int cJSONConfigur_SchemaDeclareString(cJSONConfigur_Schema_t *schema, const char *pointer, const char *default_value);
// 声明数值配置项 (0 成功, -1 参数非法, -6 类型不匹配, -8 内存分配失败)
int cJSONConfigur_SchemaDeclareNumber(cJSONConfigur_Schema_t *schema, const char *pointer, double default_value);
// 声明布尔配置项 (0 成功, -1 参数非法, -6 类型不匹配, -8 内存分配失败)
int cJSONConfigur_SchemaDeclareBool(cJSONConfigur_Schema_t *schema, const char *pointer, bool default_value);
// 释放声明表 (接受 NULL, 为空操作)
void cJSONConfigur_SchemaDestroy(cJSONConfigur_Schema_t *schema);
// 由声明建树、读取配置文件取值 (0 成功, -1 参数非法, -3 已初始化, -4 配置文件无效, -8 内存分配失败)
int cJSONConfigur_Init(cJSONConfigur_Schema_t *schema, const char *path);
// 原子写回 Init 的文件 (0 成功, -2 未初始化, -4 读写失败, -8 内存分配失败)
int cJSONConfigur_Save(void);
// 释放单例 (0 成功, -2 未初始化)
int cJSONConfigur_Deinit(void);
// 按路径读字符串 (0 成功, -1 参数非法, -2 未初始化, -5 路径不存在, -6 类型不匹配, -7 输出缓冲区不足)
int cJSONConfigur_GetString(const char *pointer, char *out, size_t size);
// 按路径读整数 (0 成功, -1 参数非法, -2 未初始化, -5 路径不存在, -6 类型不匹配)
int cJSONConfigur_GetInt(const char *pointer, int *out);
// 按路径读布尔 (0 成功, -1 参数非法, -2 未初始化, -5 路径不存在, -6 类型不匹配)
int cJSONConfigur_GetBool(const char *pointer, bool *out);
// 按路径读浮点 (0 成功, -1 参数非法, -2 未初始化, -5 路径不存在, -6 类型不匹配)
int cJSONConfigur_GetDouble(const char *pointer, double *out);
// 按路径写字符串 (0 成功, -1 参数非法, -2 未初始化, -5 路径不存在, -6 类型不匹配, -8 内存分配失败)
int cJSONConfigur_SetString(const char *pointer, const char *value);
// 按路径写整数 (0 成功, -1 参数非法, -2 未初始化, -5 路径不存在, -6 类型不匹配)
int cJSONConfigur_SetInt(const char *pointer, int value);
// 按路径写布尔 (0 成功, -1 参数非法, -2 未初始化, -5 路径不存在, -6 类型不匹配)
int cJSONConfigur_SetBool(const char *pointer, bool value);
// 按路径写浮点 (0 成功, -1 参数非法, -2 未初始化, -5 路径不存在, -6 类型不匹配)
int cJSONConfigur_SetDouble(const char *pointer, double value);
/* Public inline functions (auxiliary) ---------------------------------------*/
/* Public define (auxiliary) -------------------------------------------------*/
/* Public macro (auxiliary) --------------------------------------------------*/


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* USERAPP_CJSON_CONFIGUR_H */
