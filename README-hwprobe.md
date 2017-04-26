# hwprobe environment variable/kernel cmdline parameter

This document describes the `hwprobe` environment variable/kernel cmdline
parameter.

You can control the hardware probing using the environment variable
`hwprobe` and the kernel cmdline parameter `hwprobe`.

If `hwprobe` is set on the kernel cmdline, the environment variable
`hwprobe` is ignored. Otherwise, the meaning of both is exactly the same.

## Controlling probing flags

`hwprobe` controls which probing flags should *always* be set/cleared (These
settings *cannot* be overridden by command line switches). Examples:

* `hwprobe=-isapnp` - *never* do any isapnp probing
* `hwprobe=-braille,-modem` - don't look for braille displays & modems

The list of supported flags varies from version to version. To get a list of
the actual set of probing flags, call `hwinfo -all` (**Not** `--all`!) and look at the top of
the log (it lists all probing flags with their respective status there).

## Adding/removing hardware from results

`hwprobe` allows you to add and remove hardware from the probing results. In
this case the syntax is (-: remove, +: add at end of list, `<nothing>`: add at
begin of list):

    hwprobe=[+-]<device_class>:<vendor_id>:<device_id>[:<unix_device_file>]

`<device_class>`, `<vendor_id>` and `<device_id>` are device ids as used by
libhd. See the output of `hwinfo` for examples. In connection with `-`, you can
use `*` as a placeholder that matches every id.

Note: `<unix_device_file>` is optional.

Note2: you cannot usefully *add* hardware that needs more info than that
given by the `hwprobe` entry. Disks & floppies are notable examples.
(But you can *remove* them.)

Here is a typical `hwinfo` output for a mouse, with the relevant ids
underlined (`<device_class>` is the combined `base_class` & `sub_class`),
(see 1st example below):

        14: PS/2 00.0: 10500 PS/2 Mouse
                       ^^^^^ -->	<device_class>
          [Created at mouse.110]
          Vendor: s0200 "Unknown"
                  ^^^^^  -->		<vendor_id>
          Model: 0002 "Generic PS/2 Mouse"
                 ^^^^  -->		<device_id>
          Device File: /dev/psaux
                       ^^^^^^^^^^ -->	<unix_device_file>
          Driver Info #0:
            XFree86 Protocol: ps/2
            GPM Protocol: ps2
          Attached to: #8 (PS/2 Controller)

Examples:

        hwprobe=+10500:s200:2:/dev/psaux
          o add a ps/2 mouse [at the end of the hardware list]

        hwprobe=10500:s200:2:/dev/psaux
          o add a ps/2 mouse [at the start of the hardware list, so it
            is our default mouse]

        hwprobe=+10b00:s5001:0:/dev/ttyS0
          o add a braille display connected to /dev/ttyS0

        hwprobe=-10500:s200:2:/dev/psaux
          o remove ps/2 mice attached to /dev/psaux

        hwprobe=-10500:s200:2
          o remove all ps/2 mice

        hwprobe=-10500:*:*
          o remove all ps/2 mice

        hwprobe=-*:*:*:/dev/hdc
          o remove /dev/hdc

        hwprobe=+401:1274:5000
          o add an ensoniq sound card

Graphics cards are slightly trickier:

        hwprobe=+300:1014:b7
          o add a Fire GL1 card
            Note: this way you'll get a multihead config. You'll probably
            rather want the following example.

        hwprobe=-300:*:*,+300:1014:b7
          o remove all graphics cards; then add a Fire GL1 card

        hwprobe=+400:121a:1
          o add a 3fx voodoo card (Note the class "400", not "300"!)

For more ids, see `src/ids/names.*` and `src/ids/drivers.*`.
