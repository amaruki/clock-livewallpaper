# Desktop Clock Overlay

A lightweight, resource-efficient Windows desktop clock that integrates seamlessly behind your desktop icons. Built with the Win32 API and Direct2D for high-performance rendering.

![Desktop Clock Preview](image/image.png)

## Features

- **Seamless Desktop Integration**: Transitions behind desktop icons using the `WorkerW` technique. It remains visible even when using "Show Desktop" (`Win+D`).
- **High-Performance Rendering**: Utilizes **Direct2D** and **DirectWrite** for hardware-accelerated, crisp text rendering.
- **Multi-Monitor Support**: Automatically detects all connected monitors and displays a clock on each.
- **Low Resource Usage**: Updates only once per second and utilizes modern Windows APIs to minimize CPU/GPU impact.
- **System Tray Integration**: Easily toggle the clock or exit the application via a right-click menu in the system tray.
- **Customizable**: Configuration is stored in a simple `.ini` file.

## Requirements

- **OS**: Windows 10 or Windows 11.
- **Compiler**: MinGW-w64 (GCC) is recommended for building using the provided script.

## Building and Running

### 1. Build
Ensure `g++` is in your system PATH. Run the provided build script:
```powershell
.\build.bat
```
This will compile `main.cpp` and generate `DesktopClock.exe`.

### 2. Run
Simply execute the generated file:
```powershell
.\DesktopClock.exe
```

## Configuration

The application creates a configuration file at:
`%APPDATA%\DesktopClock.ini`

You can manually edit this file to change:
- `ShowOnStartup`: (1/0) Whether to enable the clock on launch.
- `StartupDelay`: Delay in milliseconds before showing (useful for system startup).
- `FontSize`: Primary font size for the time.
- `SubFontSize`: Secondary font size for the date.

## Usage

- **Toggle Clock**: Right-click the tray icon and select **"Aktifkan Clock"**.
- **Exit**: Right-click the tray icon and select **"Keluar"**.

## License

This project is provided "as-is" for personal use and customization.
