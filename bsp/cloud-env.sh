# Source this file in the prepared cloud SDK environment; does not install.
BSP_SDK_ROOT=${BSP_SDK_ROOT:-/workspace/bsp-sdk/root}
export PATH="$BSP_SDK_ROOT/usr/bin:$PATH"
export LD_LIBRARY_PATH="$BSP_SDK_ROOT/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export BISON_PKGDATADIR="$BSP_SDK_ROOT/usr/share/bison"
export M4="$BSP_SDK_ROOT/usr/bin/m4"
export BSP_CC="$BSP_SDK_ROOT/usr/bin/aarch64-linux-gnu-gcc-12"
export BSP_HOSTCFLAGS="-O2 -I$BSP_SDK_ROOT/usr/include"
export BSP_HOSTLDFLAGS="-L$BSP_SDK_ROOT/usr/lib/x86_64-linux-gnu"
