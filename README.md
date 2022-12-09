# libimobiledevice-glue

Library with common code used by the libraries and tools around the
**libimobiledevice** project.

![](https://github.com/libimobiledevice/libimobiledevice-glue/workflows/build/badge.svg)

## Features

The main functionality provided by this library are **socket** helper
functions and a platform independent **thread/mutex** implementation.
Besides that it comes with a number of string, file, and plist helper
functions, as well as some other commonly used code that was originally
duplicated in the dedicated projects.

Test on Linux, macOS, Windows.

## Projects using this library

- [libusbmuxd](https://github.com/libimobiledevice/libusbmuxd)
- [libimobiledevice](https://github.com/libimobiledevice/libimobiledevice)
- [usbmuxd](https://github.com/libimobiledevice/usbmuxd)
- [libirecovery](https://github.com/libimobiledevice/libirecovery)
- [idevicerestore](https://github.com/libimobiledevice/idevicerestore)

## Installation / Getting started

### Debian / Ubuntu Linux

First install all required dependencies and build tools:
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

Then clone the actual project repository:
```shell
git clone https://github.com/libimobiledevice/libimobiledevice-glue.git
cd libimobiledevice-glue
```

Now you can build and install it:
```shell
./autogen.sh
make
sudo make install
```

If you require a custom prefix or other option being passed to `./configure`
you can pass them directly to `./autogen.sh` like this:
```bash
./autogen.sh --prefix=/opt/local
make
sudo make install
```

## Usage

This library is directly used by libusbmuxd, libimobiledevice, etc., so there
is no need to do anything in particular.

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

This library and utilities are licensed under the [GNU Lesser General Public License v2.1](https://www.gnu.org/licenses/lgpl-2.1.en.html),
also included in the repository in the `COPYING` file.

## Credits

Apple, iPhone, iPad, iPod, iPod Touch, Apple TV, Apple Watch, Mac, iOS,
iPadOS, tvOS, watchOS, and macOS are trademarks of Apple Inc.

This project is an independent software and has not been authorized, sponsored,
or otherwise approved by Apple Inc.

README Updated on: 2022-04-04
