#! /bin/sh
#
# gen-hwcfg-disk.sh
#
# Generates hwcfg file for all configured disks
#

if [ -x /sbin/ata_identify ]; then
    ATA_ID=/sbin/ata_identify
elif [ -x /lib/klibc/bin/ata_identify ]; then
    ATA_ID=/lib/klibc/bin/ata_identify
else
    echo "ata_identify not found, please install udev"
    exit 1
fi

hwcfg=/etc/sysconfig/hardware

if [ ! -d "$hwcfg" ]; then
    echo "No hardware configuration directory found"
    exit 1
fi

# IDE disks first
for ifname in /sys/block/hd*; do
    id=$($ATA_ID /dev/${ifname##*/} 2> /dev/null)
    if [ $?  -eq 0 ]; then
	filename="SATA_$id"
	echo "Generate hwcfg file for $filename"
	echo "DEVICE=${ifname##*/}" > ${hwcfg}/hwcfg-disk-id-${filename}
    fi
done

# SCSI disks next
for ifname in /sys/block/sd*; do
    if [ -d $ifname/device ]; then
	read vendor < $ifname/device/vendor
	if [ "$vendor" = "ATA" ]; then
	    # We need page 0x80 to get the serial number
	    page="-p 0x80"
	else
	    page=
	fi
	scsi_id -g $page -s ${ifname#/sys} 2> /dev/null | while read vendor model serial; do
	    filename="${vendor}_${model}_${serial}"
	    echo "Generate hwcfg file for $filename"
	    echo "DEVICE=${ifname##*/}" > ${hwcfg}/hwcfg-disk-id-${filename}
	done
    fi
done
