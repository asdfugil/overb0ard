# overb0ard

**iOS and macOS share a program: jetsam.** The tl;dr is that jetsam monitors memory usage, asks for memory when there isn't much free, and kills other programs that don't give back as much as was requested. This tool--a continuation from conradev's jetsamctl--serves to override the strict limits that jetsam sets on iOS devices with a simple command. All you need is a device with iOS 6 or later.

### Installation

The latest Debian package can be downloaded from my [Cydia Repository](https://doregon.github.io/cydia) in the System section.

### Usage

You can use `jetsamctl` to set the per process memory limit or Jetsam priority for a process.

Limit the Phone app to 48 MB of memory:

```
# jetsamctl -l 48 MobilePhone
```

Set the priority of the Photos app to that of an iOS keyboard extension:

```
# jetsamctl -p 8 MobileSlideShow
```

### Priorities

Here is a table of all of the priorities and their numerical values:

| Priority | Value | Examples |
|:--|:--:|:--:|
| Idle | 0 | |
| Idle (Deferred) | 1 | |
| Background (Opportunistic) | 2 | |
| Background | 3 | |
| Mail | 4 | Apple Mail |
| Phone | 5 | Phone app |
| UI Support | 8 | Keyboard extension |
| Foreground Support | 9 | Share extension |
| Foreground | 10 | Foreground application |
| Audio and Accessory | 12 | |
| Conductor | 13 | |
| Home | 16 | SpringBoard |
| Executive | 17 | |
| Important | 18 | |
| Critical | 19 | |


## License

`jetsamctl` is available under the MIT license. See the LICENSE file for more info.
