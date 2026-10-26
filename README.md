# RK3576 experimental build download

This branch contains split download assets for `car-bsp-artifacts-with-initrd.tar.gz`.
Kernel: `6.1.99-rk3576-car-bsp`. These artifacts have not yet been boot-tested.
Concatenate part01 through part04 in order, then verify the whole-file SHA256 in manifest.json.
Do not extract directly over a running system root or boot partition.

Build source: https://github.com/LubanCat/kernel/tree/b73c060dbe3576f056c197d7f2f7cc11f65e2aec
Project integration and deployment source: https://github.com/saberDDX/car/tree/73bae00
Compiler: GCC 12.2 AArch64, binutils 2.40. Package includes Image, matching modules, DTB and a separately named initrd.
Build provenance and checksums are included in the package.
