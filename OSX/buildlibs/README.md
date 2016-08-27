# Requirements

Install homebrew:

(homebrew is a package management tool for osx)

```sh
$ /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```
Install via homebrew:

```sh
$ brew install pkg-config autoconf automake libtool cmake curl
```

Install Xcode and command line tools. Configure xcode select

```sh
$ sudo xcode-select --reset
```

# Building

```sh
$ make install
```