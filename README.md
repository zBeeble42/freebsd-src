Notes

-Codes in this repository are WIP codes, they contain printfs for debugging purposes etc.
-Some issues related to copyrights need to be solved.
-Codes by JS are beginner's work. Criticism is welcome.
-For JH7100 specifically this contains now clk drivers and reset.
-Also contains some drivers for JH7100, they need further developing.
-Does not boot.

How to test

-Download FreeBSD's generic SD image dated 2023-07-27 and copy it to your microSD card
https://download.freebsd.org/snapshots/riscv/riscv64/ISO-IMAGES/14.0/FreeBSD-14.0-CURRENT-riscv-riscv64-GENERICSD-20230727-474708c334a7-264358.img.xz
-Clone this repository and build the kernel
-Copy the kernel to SD image's UFS partition's file: /boot/kernel/kernel
-Copy a device tree file jh7110-visionfive-v2.dtb (placed to the root of this repository) to SD image's EFI partition's file: /dtb/jh7110-visionfive-v2.dtb

-One of booting methods is described here (tested without updating firmware):
1) QSPI boot mode (board's physical boot switches must point to the right)
2) after switching power on, stop autoboot by pressing any key when a counter appears
3) then type commands
load mmc 1:1 0x48000000 dtb/jh7110-visionfive-v2.dtb
load mmc 1:1 0x44000000 EFI/BOOT/bootriscv64.efi
bootefi 0x44000000 0x48000000

