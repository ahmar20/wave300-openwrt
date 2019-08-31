MODULE_FILENAME		=	wave300_driver

OPENWRT_DIR			=	/home/ahmar/build-system/openwrt
# These two lines are not really needed
#BUILD_DIR			=	/home/ahmar/build-system/wave300-dev/build
#TEMP_DIR			=	${BUILD_DIR}/tmp

obj-m 				+=	$(MODULE_FILENAME).o
KO_FILE 			=	$(MODULE_FILENAME).ko

VERBOSITY_LEVEL		=	0


# export CrossCompile variables

export 	TOOLCHAIN_DIR		=	${OPENWRT_DIR}/staging_dir/toolchain-mips_24kc_gcc-7.3.0_musl
export 	PATH				:=	${TOOLCHAIN_DIR}/bin:${PATH}
export 	KERNEL_ROOT			=	${OPENWRT_DIR}/build_dir/target-mips_24kc_musl/linux-lantiq_xrx200/linux-4.9.184
export 	KBUILD_OUTPUT		=	${KERNEL_ROOT}

export 	CROSS_COMPILE		=	mips-openwrt-linux-
export 	CC					=	mips-openwrt-linux-gcc
export 	HOST				=	mips-openwrt-linux
export 	ARCH				=	mips
export 	OS					=	${HOST}


module:
	@$(MAKE) -C $(KERNEL_ROOT) M=$(PWD) $(KO_FILE) V=$(VERBOSITY_LEVEL)

clean: 
	rm -rf *.o *.ko *.mod.c *.symvers
