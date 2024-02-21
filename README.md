# libimobiledevice-glue

Library with common code used by the libraries and tools around the
**libimobiledevice** project.

![](https://github.com/libimobiledevice/libimobiledevice-glue/workflows/build/badge.svg)

## Table of Contents
- [Features](#features)
- [Building](#building)
  - [Prerequisites](#prerequisites)
    - [Linux (Debian/Ubuntu based)](#linux-debianubuntu-based)
    - [macOS](#macos)
    - [Windows](#windows)
  - [Configuring the source tree](#configuring-the-source-tree)
  - [Building and installation](#building-and-installation)
- [Usage](#usage)
- [Contributing](#contributing)
- [Links](#links)
- [License](#license)
- [Credits](#credits)

## Features

The main functionality provided by this library are **socket** helper
functions and a platform independent **thread/mutex** implementation.
Besides that it comes with a number of string, file, and plist helper
functions, as well as some other commonly used code that was originally
duplicated in the dedicated projects.

Tested on Linux, macOS, and Windows.

## Projects using this library

- [libusbmuxd](https://github.com/libimobiledevice/libusbmuxd)
- [libimobiledevice](https://github.com/libimobiledevice/libimobiledevice)
- [usbmuxd](https://github.com/libimobiledevice/usbmuxd)
- [libirecovery](https://github.com/libimobiledevice/libirecovery)
- [idevicerestore](https://github.com/libimobiledevice/idevicerestore)

## Building

### Prerequisites

You need to have a working compiler (gcc/clang) and development environent
available. This project uses autotools for the build process, allowing to
have common build steps across different platforms.
Only the prerequisites differ and they are described in this section.

libimobiledevice-glue requires [libplist](https://github.com/libimobiledevice/libplist).
Check the [Building](https://github.com/libimobiledevice/libplist?tab=readme-ov-file#building)
section of the README on how to build it. Note that some platforms might have it as a package.

#### Linux (Debian/Ubuntu based)

* Install all required dependencies and build tools:
  ```shell
  sudo apt-get install \
  	build-essential \
  	pkg-config \
  	checkinstall \
  	git \
  	autoconf \
  	automake \
  	libtool-bin \
  	libplist-dev
  ```

  In case libplist-dev is not available, you can manually build and install it. See note above.

#### macOS

* Make sure the Xcode command line tools are installed. Then, use either [MacPorts](https://www.macports.org/)
  or [Homebrew](https://brew.sh/) to install `automake`, `autoconf`, `libtool`, etc.

  Using MacPorts:
  ```shell
  sudo port install libtool autoconf automake pkgconfig
  ```

  Using Homebrew:
  ```shell
  brew install libtool autoconf automake pkg-config
  ```

#### Windows

* Using [MSYS2](https://www.msys2.org/) is the official way of compiling this project on Windows. Download the MSYS2 installer
  and follow the installation steps.

  It is recommended to use the _MSYS2 MinGW 64-bit_ shell. Run it and make sure the required dependencies are installed:

  ```shell
  pacman -S base-devel \
  	git \
  	mingw-w64-x86_64-gcc \
  	make \
  	libtool \
  	autoconf \
  	automake-wrapper \
  	pkg-config
  ```
  NOTE: You can use a different shell and different compiler according to your needs. Adapt the above command accordingly.

### Configuring the source tree

You can build the source code from a git checkout, or from a `.tar.bz2` release tarball from [Releases](https://github.com/libimobiledevice/libimobiledevice-glue/releases).
Before we can build it, the source tree has to be configured for building. The steps depend on where you got the source from.

Since libimobiledevice-glue depends on other packages, you should set the pkg-config environment variable `PKG_CONFIG_PATH`
accordingly. Make sure to use a path with the same prefix as the dependencies. If they are installed in `/usr/local` you would do

```shell
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
```

* **From git**

  If you haven't done already, clone the actual project repository and change into the directory.
  ```shell
  git clone https://github.com/libimobiledevice/libimobiledevice-glue
  cd libimobiledevice-glue
  ```

  Configure the source tree for building:
  ```shell
  ./autogen.sh
  ```

* **From release tarball (.tar.bz2)**

  When using an official [release tarball](https://github.com/libimobiledevice/libimobiledevice-glue/releases) (`libimobiledevice-glue-x.y.z.tar.bz2`)
  the procedure is slightly different.

  Extract the tarball:
  ```shell
  tar xjf libimobiledevice-glue-x.y.z.tar.bz2
  cd libimobiledevice-glue-x.y.z
  ```

  Configure the source tree for building:
  ```shell
  ./configure
  ```

Both `./configure` and `./autogen.sh` (which generates and calls `configure`) accept a few options, for example `--prefix` to allow
building for a different target folder. You can simply pass them like this:

```shell
./autogen.sh --prefix=/usr/local
```
or
```shell
./configure --prefix=/usr/local
```

Once the command is successful, the last few lines of output will look like this:
```
[...]
config.status: creating config.h
config.status: executing depfiles commands
config.status: executing libtool commands

Configuration for libimobiledevice-glue 1.0.1:
-------------------------------------------

  Install prefix: .........: /usr/local

  Now type 'make' to build libimobiledevice-glue 1.0.1,
  and then 'make install' for installation.
```

### Building and installation

If you followed all the steps successfully, and `autogen.sh` or `configure` did not print any errors,
you are ready to build the project. This is simply done with

```shell
make
```

If no errors are emitted you are ready for installation. Depending on whether
the current user has permissions to write to the destination directory or not,
you would either run
```shell
make install
```
_OR_
```shell
sudo make install
```

If you are on Linux, you want to run `sudo ldconfig` after installation to
make sure the installed libraries are made available.

## Usage

This library is directly used by libusbmuxd, libimobiledevice, etc., so there
is no need to do anything in particular. Feel free to use it in your project,
check the header files for available functions. A documentation might be added
later.

## Contributing

We welcome contributions from anyone and are grateful for every pull request!

If you'd like to contribute, please fork the `master` branch, change, commit and
send a pull request for review. Once approved it can be merged into the main
code base.

If you plan to contribute larger changes or a major refactoring, please create a
ticket first to discuss the idea upfront to ensure less effort for everyone.

Please make sure your contribution adheres to:
* Try to follow the code style of the project
* Commit messages should describe the change well without being too short
* Try to split larger changes into individual commits of a common domain

## Links

* Homepage: https://libimobiledevice.org/
* Repository: https://git.libimobiledevice.org/libimobiledevice-glue.git
* Repository (Mirror): https://github.com/libimobiledevice/libimobiledevice-glue.git
* Issue Tracker: https://github.com/libimobiledevice/libimobiledevice-glue/issues
* Mailing List: https://lists.libimobiledevice.org/mailman/listinfo/libimobiledevice-devel
* Twitter: https://twitter.com/libimobiledev

## License

This library is licensed under the [GNU Lesser General Public License v2.1](https://www.gnu.org/licenses/lgpl-2.1.en.html),
also included in the repository in the `COPYING` file.

## Credits

Apple, iPhone, iPad, iPod, iPod Touch, Apple TV, Apple Watch, Mac, iOS,
iPadOS, tvOS, watchOS, and macOS are trademarks of Apple Inc.

This project is an independent software library and has not been authorized,
sponsored, or otherwise approved by Apple Inc.

README Updated on: 2024-02-21

