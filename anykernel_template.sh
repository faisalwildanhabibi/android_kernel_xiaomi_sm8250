### AnyKernel3 Ramdisk Mod Script
### Poco F3 (alioth / aliothin) - Universal Pure Kernel Installer for All AOSP ROMs (PixelOS, crDroid, EvoX, etc.)

## AnyKernel setup
# begin properties
properties() { '
kernel.string=APTKernel Pure Universal for Poco F3 (alioth)
do.devicecheck=1
do.modules=0
do.systemless=0
do.cleanup=1
do.cleanuponabort=0
device.name1=alioth
device.name2=aliothin
device.name3=alioth_global
device.name4=M2012K11AG
device.name5=
supported.versions=
supported.patchlevels=
supported.vendorpatchlevels=
'; } # end properties

# shell variables
BLOCK=boot;
IS_SLOT_DEVICE=auto;
RAMDISK_COMPRESSION=auto;

block=boot;
is_slot_device=auto;
ramdisk_compression=auto;

## AnyKernel methods (DO NOT CHANGE)
# import patching functions/variables - see for reference
. tools/ak3-core.sh;

## AnyKernel install
# Zero-Ramdisk-Touch: Unpack boot image header only, replace kernel Image, and directly repack boot.img
# This preserves the ROM's generic ramdisk 100% untouched byte-for-byte, preserving SELinux contexts, xattrs, and capabilities!
split_boot;

flash_boot;
## end install
