# <project-name>

TODO: description / 项目描述。

## Build 构建

```sh
make          # compile all, output build/demo
make clean    # remove build/
```

## Run 运行

```sh
./build/demo
```

## Layout 结构

```
main.c           application entry / 程序入口
template.c/.h    module scaffold / 模块脚手架
libs/queue/      thread-safe queue / 线程安全队列
libs/log.c/      logging library / 日志库
libs/unity/      unit test framework / 单元测试框架
```
