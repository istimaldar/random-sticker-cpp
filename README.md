# Random sticker sender

Application to send random sticker to specified user in Telegram.

## Features

* Persistent login sessions
* Can send one or more random stickers

## Usage

```bash
Sends random sticker to specified user
Usage: random_sticker_sender [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -l,--login TEXT REQUIRED    Login of user to whom you want to send sticker
  -e,--encryption_key TEXT REQUIRED
                              Key to encrypt session database
  -a,--amount INT             Amount of random stickers to send

```

## Dependencies

* C++14 compatible compiler (Clang 3.4+, GCC 4.9+, MSVC 19.0+ (Visual Studio 2015+), Intel C++ Compiler 17+)
* OpenSSL
* zlib
* gperf (build only)
* CMake (3.1+, build only)

## Build

### Linux

```bash
    mkdir -p build && cd build;
    cmake ..;
    cmake --build .;
```