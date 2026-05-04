#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper #Deep cleans the kernel and remove .config file
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig #CONFIGURE for our virtual arm dev board we will simulate in QEMU
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all #Build the kernel image for booting in QEMU
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules #Build kernel modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs #Build the devicetree
    cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}
fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir $OUTDIR/rootfs && cd $OUTDIR/rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var init
cd $OUTDIR/rootfs/usr
mkdir -p usr/bin usr/lib usr/sbin
cd $OUTDIR/rootfs/usr
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=$OUTDIR/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
make CONFIG_PREFIX=/home/rkhoury/courses/advanced_embedded_linux/rootfs ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a /bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a /bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
cp /usr/aarch64-linux-gnu/lib/ld-linux-aarch64.so.1 $OUTDIR/rootfs/lib64/
cp /usr/aarch64-linux-gnu/lib/libm.so.6 $OUTDIR/rootfs/lib64/
cp /usr/aarch64-linux-gnu/lib/libresolv.so.2 $OUTDIR/rootfs/lib64/
cp /usr/aarch64-linux-gnu/lib/libc.so.6 $OUTDIR/rootfs/lib64/

cp /usr/aarch64-linux-gnu/lib/ld-linux-aarch64.so.1 $OUTDIR/rootfs/lib/
cp /usr/aarch64-linux-gnu/lib/libm.so.6 $OUTDIR/rootfs/lib/
cp /usr/aarch64-linux-gnu/lib/libresolv.so.2 $OUTDIR/rootfs/lib/
cp /usr/aarch64-linux-gnu/lib/libc.so.6 $OUTDIR/rootfs/lib/

# TODO: Make device nodes
cd "$OUTDIR/rootfs"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=aarch64-none-linux-gnu-

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp ./finder.sh $OUTDIR/rootfs/home/
cp ./finder-test.sh $OUTDIR/rootfs/home/
cp ./autorun-qemu.sh $OUTDIR/rootfs/home/
cp -r ../conf $OUTDIR/rootfs/home/conf
cp ./Makefile $OUTDIR/rootfs/home/
cp ./writer $OUTDIR/rootfs/home/

# TODO: Chown the root directory
sudo chown -R root:root $OUTDIR/rootfs

# TODO: Create initramfs.cpio.gz
cd "$OUTDIR/rootfs"
find . | cpio -H newc -ov --owner root:root > "$OUTDIR/initramfs.cpio"
gzip -f $OUTDIR/initramfs.cpio