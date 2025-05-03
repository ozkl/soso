# Toolchain
To build userspace executables, we can use Clang cross compiler with sysroot config.
Lets create our toolchain in /opt/soso-toolchain

    mkdir -p /opt/soso-toolchain/bin
    mkdir -p /opt/soso-toolchain/i386-soso/sys-root/

## soso-clang
This is a wrapper script to clang with correct settings.
Copy soso-clang into /opt/soso-toolchain/bin/

    chmod +x /opt/soso-toolchain/bin/soso-clang

Copy soso.ld into /opt/soso-toolchain/i386-soso/sys-root/

Add our binaries to the path!

    export PATH=/opt/soso-toolchain/bin:$PATH

The static lib that includes low level builtins libclang_rt.builtins-i386.a will be needed in our toolchain.
We can build llvm to generate it or we can use the one in our linux system.


    mkdir -p /opt/soso-toolchain/lib/clang/19/lib/linux
    cp /usr/lib/llvm-19/lib/clang/19/lib/linux/libclang_rt.builtins-i386.a /opt/soso-toolchain/lib/clang/19/lib/linux/

## binutils
Soso requires userspace processes to be loaded after virtual memory location 0x40000000.

So we either need to use a linker script or a cross linker to link executables.
Binutils is patched with TEXT_START_ADDR=0x40000000 to achieve that.

Download and build binutils 2.34
    
    wget https://ftp.gnu.org/gnu/binutils/binutils-2.34.tar.xz
    tar xf binutils-2.34.tar.xz
    cd binutils-2.34
    git apply ../git/soso/toolchain/soso-binutils.patch

    ./configure \
      --target=i386-soso \
      --prefix=/opt/soso-toolchain \
      --with-sysroot=/opt/soso-toolchain/i386-soso/sys-root \
      --disable-nls --disable-werror

For building binutils, makeinfo (ubuntu package: texinfo) is needed.

    make
    make install


## musl

    ./configure \
      CC=soso-clang \
      LIBCC=compiler-rt \
      CROSS_COMPILE=i386-soso- \
      --target=i386-soso \
      --prefix=/opt/soso-toolchain/i386-soso/sys-root \
      --disable-shared

    make
    make install

After building musl, we are able to build userspace binaries for soso!
