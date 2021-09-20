# hwinfo

## Overview

hwinfo/libhd are used to probe for the hardware present in the system. It can be
used to generate a system overview log which can be later used for support.

This project provides a hardware probing library `libhd.so` and a command line tool `hwinfo` using it.
A major project using this library is [YaST](https://yast.github.io), the SUSE installation tool.

To give you an idea what kind of information it provides, here's the output it gives when asked about the graphics card:

```sh
# hwinfo --gfxcard
27: PCI 200.0: 0300 VGA compatible controller (VGA)
  [Created at pci.378]
  Unique ID: B35A.G9ppNwS+xM4
  Parent ID: _Znp.nMBktMhAWbC
  SysFS ID: /devices/pci0000:00/0000:00:02.0/0000:02:00.0
  SysFS BusID: 0000:02:00.0
  Hardware Class: graphics card
  Model: "nVidia GF119 [NVS 310]"
  Vendor: pci 0x10de "nVidia Corporation"
  Device: pci 0x107d "GF119 [NVS 310]"
  SubVendor: pci 0x10de "nVidia Corporation"
  SubDevice: pci 0x094e
  Revision: 0xa1
  Driver: "nvidia"
  Driver Modules: "nvidia"
  Memory Range: 0xfa000000-0xfaffffff (rw,non-prefetchable)
  Memory Range: 0xf0000000-0xf7ffffff (ro,non-prefetchable)
  Memory Range: 0xf8000000-0xf9ffffff (ro,non-prefetchable)
  I/O Ports: 0xe000-0xefff (rw)
  Memory Range: 0xfb000000-0xfb07ffff (ro,non-prefetchable,disabled)
  IRQ: 82 (3241635 events)
  I/O Ports: 0x3c0-0x3df (rw)
  Module Alias: "pci:v000010DEd0000107Dsv000010DEsd0000094Ebc03sc00i00"
  Driver Info #0:
    Driver Status: nouveau is not active
    Driver Activation Cmd: "modprobe nouveau"
  Driver Info #1:
    Driver Status: nvidia is active
    Driver Activation Cmd: "modprobe nvidia"
  Config Status: cfg=new, avail=yes, need=no, active=unknown
  Attached to: #9 (PCI bridge)

Primary display adapter: #27
```

If that's a bit too much information, you can ask it also for an abbreviated form. For example:

```sh
# hwinfo --short --disk --cdrom
disk:
  /dev/sda             WDC WD10EARS-00Y
  /dev/sdb             ST2000DM001-1CH1
cdrom:
  /dev/sr0             PLDS DVD+-RW DS-8ABSH
```

You can influence libhd via the `hwprobe` environment variable resp. the `hwprobe` boot option.
This includes turning on or off
probing modules and also manually adding hardware devices (to some degree).

For example

```sh
export hwprobe=-bios
```
will turn off the `bios` probing module.

For details about `hwprobe` look [here](README-hwprobe.md).

For general usage instructions, see `hwinfo` manual page.

> ### Note
>
>
> `hwinfo` has a legacy interface, accepting `hwprobe`-like options as command argument (For example
> `hwinfo -bios` - note the single '`-`'). Please don't do this. If you are interested, you can
> read about it [here](README-legacy.md).

## Technical documentation

The hardware detection library makes use of a number of technical specifications.

[Here](specifications.md) is a compilation of external links to technical standards relevant to `libhd`.

## openSUSE Development

To build the library, simply run `make`. Install with `make install`.

Basically every new commit into the master branch of the repository will be auto-submitted
to all current SUSE products. No further action is needed except accepting the pull request.

Submissions are managed by a SUSE internal [jenkins](https://jenkins.io) node in the InstallTools tab.

Each time a new commit is integrated into the master branch of the repository,
a new submit request is created to the openSUSE Build Service. The devel project
is [system:install:head](https://build.opensuse.org/package/show/system:install:head/hwinfo).

For maintained branches, the package is submitted to a devel project but the final submission
must be triggered manually.

`*.changes` and version numbers are auto-generated from git commits, you don't have to worry about this.

The spec file is maintained in the Build Service only. If you need to change it for the `master` branch,
submit to the
[devel project](https://build.opensuse.org/package/show/system:install:head/hwinfo)
in the build service directly.

The current names of the devel projects for other branches can be seen in the jenkins logs.

Development happens mainly in the `master` branch. The branch is used for all current products.

In rare cases branching was unavoidable:

* branch `sl_11.1`: SLE 11 SP4
* branch `sle12`: SLE 12 (**not** SPx)

You can find more information about the changes auto-generation and the
tools used for jenkis submissions in the [linuxrc-devtools
documentation](https://github.com/openSUSE/linuxrc-devtools#opensuse-development).

## Updating Database for Pci and Usb Ids

For details about updating pci and usb ids look [here](src/ids/README.md).
