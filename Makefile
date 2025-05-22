# ライブラリへのパス
MCULIB         ?= mculib
OPENCM3_DIR    ?= libopencm3
BOOTLOAD_PORT  ?= /dev/ttyACM0

# デバイス設定
BOARDNAME      ?= board_v2_plus4

# DEVICE は libopencm3 のビルドシステムで使用されます
# これに基づいて TARGET_ARCH_CFLAGS や TARGET_ARCH_CXXFLAGS に適切なファミリー定義が設定されるはずです。
DEVICE         := gd32f303cc_nofpu # GD32F303CC (Cortex-M4, FPUなし)

# プロジェクト固有の定義 (コマンドラインからの EXTRA_CFLAGS で上書き可能)
# ★★★ GD32ファミリー定義とmculibデバイス定義をここに集約 ★★★
# libopencm3のgcc-config.mkが設定するDEFSにもファミリー定義が含まれるはずだが、念のため明示。
# お使いのlibopencm3/mculibが期待する正確なマクロ名に合わせて調整が必要な場合があります。
PROJECT_DEFINES := \
    -DGD32F30X_HD \
    -DMCULIB_DEVICE_GD32F3 \
    -DSWEEP_POINTS_MAX=201 \
    -DSAVEAREA_MAX=7 \
    -DDISPLAY_ST7796

# ソースファイル一覧
OBJS += \
    $(BOARDNAME)/board.o \
    Font5x7.o \
    Font7x13b.o \
    command_parser.o \
    common.o \
    fft.o \
    flash.o \
    gain_cal.o \
    globals.o \
    ili9341.o \
    main2.o \
    numfont20x22.o \
    plot.o \
    sin_rom.o \
    stream_fifo.o \
    synthesizers.o \
    ui.o \
    uihw.o \
    vna_measurement.o \
    xpt2046.o \
    spi_slave.o

OBJS    += \
    $(MCULIB)/dma_adc.o \
    $(MCULIB)/dma_driver.o \
    $(MCULIB)/fastwiring.o \
    $(MCULIB)/message_log.o \
    $(MCULIB)/printf.o \
    $(MCULIB)/si5351.o \
    $(MCULIB)/usbserial.o

# 基本的な最適化とデバッグ情報
OPT             = -O2
DEBUGINFO       = -g

# インクルードパス
INCLUDE_DIRS   += -I$(BOARDNAME) -I$(MCULIB)/include -I$(OPENCM3_DIR)/include

# CとC++共通のフラグ (プロジェクト定義とコマンドラインからのEXTRA_CFLAGSを含む)
# libopencm3 の gcc-config.mk が ARCH_FLAGS と DEFS (MCUファミリー定義を含むべき) を設定
include $(OPENCM3_DIR)/mk/genlink-config.mk # genlink_family などが設定される
include $(OPENCM3_DIR)/mk/gcc-config.mk     # ARCH_FLAGS, TOOLCHAIN_PREFIX, DEFS などが設定される

# 共通フラグに ARCH_FLAGS と DEFS を含める
# さらにプロジェクト固有の定義とコマンドラインからの EXTRA_CFLAGS を追加
BASE_COMMON_FLAGS = $(ARCH_FLAGS) $(DEFS) $(OPT) $(DEBUGINFO) \
                    -Wall -Wno-unused-function -Werror=implicit-fallthrough \
                    -ffunction-sections -fdata-sections \
                    -funsigned-char -fwrapv -fno-delete-null-pointer-checks -fno-strict-aliasing \
                    -D_XOPEN_SOURCE=600

# CFLAGS と CXXFLAGS を構成
# DEFS には libopencm3 の gcc-config.mk が設定するデバイスファミリー定義が含まれるはず
# COMMON_FLAGS には PROJECT_DEFINES (GD32F30X_HD, MCULIB_DEVICE_GD32F3 など) と EXTRA_CFLAGS が含まれる
CFLAGS          = $(BASE_COMMON_FLAGS) $(PROJECT_DEFINES) $(EXTRA_CFLAGS) $(INCLUDE_DIRS)
CXXFLAGS        = $(BASE_COMMON_FLAGS) $(PROJECT_DEFINES) $(EXTRA_CFLAGS) $(INCLUDE_DIRS) --std=c++17 -fno-exceptions -fno-rtti

LDFLAGS        += -static -nostartfiles -Wl,--exclude-libs,libssp
LDFLAGS        += -Wl,--gc-sections $(ARCH_LDFLAGS) # ARCH_LDFLAGS を追加
LDLIBS         += -Wl,--start-group -lgcc -lnosys -Wl,--end-group -lm

GITVERSION_FILE = gitversion.hpp

# リンカスクリプト (コマンドラインから LDSCRIPT で上書き可能)
LDSCRIPT ?= ./gd32f303cc_with_bootloader_plus4.ld

.PHONY: all clean dist-clean flash bootload_firmware dfu

all: $(GITVERSION_FILE) $(OPENCM3_LIB) binary.elf binary.hex binary.bin

# libopencm3 ライブラリのビルド
# DEVICE 変数がこの makefile インスタンスからサブの make に渡されるようにする
$(OPENCM3_LIB):
	$(MAKE) -C $(OPENCM3_DIR) DEVICE=$(DEVICE)

$(GITVERSION_FILE): .git/HEAD .git/index
	@echo "#define GITVERSION \"$(shell git log -n 1 --pretty=format:"git-%ad%h" --date=format:"%Y%m%d-")\"" > $@
	@echo "#define GITURL \"$(shell git config --get remote.origin.url)\"" >> $@

# クリーンルール修正
clean:
	$(Q)$(RM) -f binary.elf binary.hex binary.bin binary.map \
	             $(OBJS) $(GITVERSION_FILE) \
	             $(BOARDNAME)/*.o $(MCULIB)/*.o *.o

dist-clean: clean
	$(MAKE) -C $(OPENCM3_DIR) clean

flash: binary.hex
	./st-flash --reset --format ihex write binary.hex

bootload_firmware dfu: binary.bin
	python3 bootload_firmware.py --file $< --serial $(BOOTLOAD_PORT)

# libopencm3 のルールファイルをインクルード
# これらが .c.o や .cpp.o の汎用ルール、および .elf のリンクステップルールを提供する
include $(OPENCM3_DIR)/mk/gcc-rules.mk # .c.o, .S.o などのコンパイルルール
include $(OPENCM3_DIR)/mk/genlink-rules.mk # .elf, .bin, .hex などの生成ルール
