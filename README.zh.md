# iScale - 缩放

[English README](README.md)

一个 LeviLamina 玩家缩放插件。

`iScale` 可以调整玩家大小。客户端安装后，视野会跟着缩放；如果客户端不装，视野不会改变。如果只装服务端，碰撞箱以及一些行为可能会有点问题，甚至可能出现莫名其妙自动潜行、动作还是从原本位置而不是缩小后位置触发之类的情况。

> [!WARNING]
> 想要效果一致，请同时安装服务端和客户端版本。
> - 没装客户端：视野不会跟着缩放
> - 没装服务端：碰撞箱以及一些行为可能不正确
> - 例如弓箭可能还是从原本位置发射，而不是从缩小后的位点发射

## 功能

- 调整或查询玩家缩放
- 支持服务端和客户端双端安装
- 适配 LeviLamina `26.40x`

## 用法

命令：`/scale`

### 命令

```text
/scale <target> [scale]
```

### 命令说明

- `target` 支持玩家目标
- 提供 `scale` 时会修改目标玩家大小
- 不提供 `scale` 时会查询当前缩放值

## 安装

> [!CAUTION]
> 以下安装说明仅供参考，请以你的实际客户端/服务端环境和发布包来源为准。

### 服务端

#### 使用 Lip（推荐）

1. 进入你的 Minecraft 服务端目录
2. 执行以下命令：
   ```bash
   lip install github.com/MiracleForest/iScale
   ```

#### 手动安装

1. 从 [Releases 页面](https://github.com/MiracleForest/iScale/releases) 下载 `iScale-server-windows-x64.zip`
2. 将压缩包中的 `iScale` 文件夹解压到服务端的 `./plugins` 目录

### 客户端

#### 使用 Lip（推荐）

1. 进入你的 Minecraft 客户端目录
2. 执行以下命令：
   ```bash
   lip install github.com/MiracleForest/iScale#client
   ```

#### 手动安装

1. 从 [Releases 页面](https://github.com/MiracleForest/iScale/releases) 下载 `iScale-client-windows-x64.zip`
2. 将压缩包中的 `iScale` 文件夹解压到客户端的 `./mods` 目录
