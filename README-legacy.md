# Legacy

This document describes some legacy features.

__Note: please do not do this, this is only kept to assist debugging.__

Legacy mode is activated when no option starting with "--" is given. In this case hwinfo
works as follows:

    hwinfo [debug=deb_flag] [log=log_file] [list[+]=hw_item] [[+|-]probe_option1] [[+|-]probe_option2] ...

Examples:

* `hwinfo` - probes for nearly everything
* `hwinfo +all` - probes for everything
* `hwinfo log=hw_log` - default probing, output is written to hw_log. Please
  don't use `hwinfo >some_log 2>&1` to store the output into a log file!
* `hwinfo -all +pci +int` - probe for pci devices

Note that `int` should almost always be active.

Some probing flags do not stand for complete modules but enable additional
features; e.g. `bios.vesa` or `block.cdrom`.

Example:

    hwinfo -all +block +int

gives a list of all block devs

    hwinfo -all +block.cdrom +int

additionally reads the iso9660 info.

The list of supported flags varies from version to version. To get a list of
the actual set of probing flags, call `hwinfo -all` and look at the top of
the log.
