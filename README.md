# iScale

[简体中文说明](README.zh.md)

A LeviLamina mod for scaling player size.

`iScale` lets you change player size on the server. If the client side is also installed, the camera view will scale with the player; otherwise the view will stay unchanged. If only the server side is installed, collision boxes and some behaviors may be a bit off, including unexpected sneaking or actions being performed from the original position instead of the scaled one.

> [!WARNING]
> For full consistency, install both server and client versions.
> - Missing the client side: the camera view will not scale
> - Missing the server side: collision boxes and some behaviors may be incorrect
> - Example: arrows may fire from the original position instead of the scaled position

## Features

- Set or query a player's scale
- Supports both server and client builds
- Built for LeviLamina `26.51.x`

## Usage

Command: `/scale`

### Command

```text
/scale <target> [scale]
```

### Command Notes

- `target` accepts player targets
- If `scale` is provided, the selected players will be resized
- If `scale` is omitted, the current scale will be reported

## Installation

> [!CAUTION]
> This installation guide is for reference only. Please follow your actual server/client environment and package source.

### Server

#### Using Lip (Recommended)

1. Navigate to your Minecraft server directory
2. Execute the following command:
   ```bash
   lip install github.com/MiracleForest/iScale
   ```

#### Manual Installation

1. Download `iScale-server-windows-x64.zip` from the [Releases page](https://github.com/MiracleForest/iScale/releases)
2. Extract the `iScale` folder into your server's `./plugins` directory

### Client

#### Using Lip (Recommended)

1. Navigate to your Minecraft client directory
2. Execute the following command:
   ```bash
   lip install github.com/MiracleForest/iScale#client
   ```

#### Manual Installation

1. Download `iScale-client-windows-x64.zip` from the [Releases page](https://github.com/MiracleForest/iScale/releases)
2. Extract the `iScale` folder into your client's `./mods` directory
