# ライブラリへのパス
MCULIB         ?= mculib
OPENCM3_DIR    ?= libopencm3
BOOTLOAD_PORT  ?= /dev/ttyACM0

# デバイス設定
BOARDNAME      ?= board_v2_plus4

# ★★★ デバイス定義をSTM32F103に変更 ★★★
# libopencm3が認識するSTM32F103の中密度デバイスを指定 (例: stm32f103c8)
# これにより、libopencm3のヘッダはSTM32F1として処理されることを期待
DEVICE         := stm32f103c8 # STM32F103C8T6 を想定

# プロジェクト固有の定義
# GD32関連の定義は削除し、mculibがSTM32F1を認識するようにする
# STM32F1系のファミリー定義 (例: STM32F10X_MD) は libopencm3 の DEFS から提供されることを期待
PROJECT_DEFINES := \
    -DMCULIB_DEVICE_STM32F1 \
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

# libopencm3 の設定ファイルをインクルード (これにより ARCH_FLAGS, DEFS などが定義される)
# これらは MCU 固有の -mcpu, -mthumb, ファミリー定義マクロを含むべき
include $(OPENCM3_DIR)/mk/genlink-config.mk # genlink_family などが設定される
include $(OPENCM3_DIR)/mk/gcc-config.mk     # ARCH_FLAGS, TOOLCHAIN_PREFIX, DEFS などが設定される

# CとC++共通のフラグ
# ARCH_FLAGSとDEFSはlibopencm3のmkファイルから来る (DEVICE=stm32f103c8 に基づくSTM32F1用になるはず)
# PROJECT_DEFINES と EXTRA_CFLAGS を追加
COMMON_FLAGS   = $(ARCH_FLAGS) $(DEFS) $(OPT) $(DEBUGINFO) \
                 -Wall -Wno-unused-function -Werror=implicit-fallthrough \
                 -ffunction-sections -fdata-sections \
                 -funsigned-char -fwrapv -fno-delete-null-pointer-checks -fno-strict-aliasing \
                 -D_XOPEN_SOURCE=600 \
                 $(PROJECT_DEFINES) $(EXTRA_CFLAGS) # EXTRA_CFLAGS はコマンドラインから

CFLAGS          = $(COMMON_FLAGS) $(INCLUDE_DIRS)
CXXFLAGS        = $(COMMON_FLAGS) $(INCLUDE_DIRS) --std=c++17 -fno-exceptions -fno-rtti

LDFLAGS        += -static -nostartfiles -Wl,--exclude-libs,libssp
LDFLAGS        += -Wl,--gc-sections $(ARCH_LDFLAGS) # ARCH_LDFLAGS を追加
LDLIBS         += -Wl,--start-group -lgcc -lnosys -Wl,--end-group -lm

GITVERSION_FILE = gitversion.hpp

# リンカスクリプトはGD32F303CC用を維持するが、メモリマップの整合性に注意が必要
# STM32F103としてコンパイルするが、物理デバイスはGD32F303なので、
# リンカスクリプトはGD32F303のメモリサイズに合わせる必要がある。
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
# ライブラリへのパス
MCULIB         ?= mculib
OPENCM3_DIR    ?= libopencm3
BOOTLOAD_PORT  ?= /dev/ttyACM0

# デバイス設定
BOARDNAME      ?= board_v2_plus4

# ★★★ デバイス定義をSTM32F103に変更 ★★★
# libopencm3が認識するSTM32F103の中密度デバイスを指定 (例: stm32f103c8)
# これにより、libopencm3のヘッダはSTM32F1として処理されることを期待
DEVICE         := stm32f103c8 # STM32F103C8T6 を想定

# プロジェクト固有の定義
# GD32関連の定義は削除し、mculibがSTM32F1を認識するようにする
# STM32F1系のファミリー定義 (例: STM32F10X_MD) は libopencm3 の DEFS から提供されることを期待
PROJECT_DEFINES := \
    -DMCULIB_DEVICE_STM32F1 \
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

# libopencm3 の設定ファイルをインクルード (これにより ARCH_FLAGS, DEFS などが定義される)
# これらは MCU 固有の -mcpu, -mthumb, ファミリー定義マクロを含むべき
include $(OPENCM3_DIR)/mk/genlink-config.mk # genlink_family などが設定される
include $(OPENCM3_DIR)/mk/gcc-config.mk     # ARCH_FLAGS, TOOLCHAIN_PREFIX, DEFS などが設定される

# CとC++共通のフラグ
# ARCH_FLAGSとDEFSはlibopencm3のmkファイルから来る (DEVICE=stm32f103c8 に基づくSTM32F1用になるはず)
# PROJECT_DEFINES と EXTRA_CFLAGS を追加
COMMON_FLAGS   = $(ARCH_FLAGS) $(DEFS) $(OPT) $(DEBUGINFO) \
                 -Wall -Wno-unused-function -Werror=implicit-fallthrough \
                 -ffunction-sections -fdata-sections \
                 -funsigned-char -fwrapv -fno-delete-null-pointer-checks -fno-strict-aliasing \
                 -D_XOPEN_SOURCE=600 \
                 $(PROJECT_DEFINES) $(EXTRA_CFLAGS) # EXTRA_CFLAGS はコマンドラインから

CFLAGS          = $(COMMON_FLAGS) $(INCLUDE_DIRS)
CXXFLAGS        = $(COMMON_FLAGS) $(INCLUDE_DIRS) --std=c++17 -fno-exceptions -fno-rtti

LDFLAGS        += -static -nostartfiles -Wl,--exclude-libs,libssp
LDFLAGS        += -Wl,--gc-sections $(ARCH_LDFLAGS) # ARCH_LDFLAGS を追加
LDLIBS         += -Wl,--start-group -lgcc -lnosys -Wl,--end-group -lm

GITVERSION_FILE = gitversion.hpp

# リンカスクリプトはGD32F303CC用を維持するが、メモリマップの整合性に注意が必要
# STM32F103としてコンパイルするが、物理デバイスはGD32F303なので、
# リンカスクリプトはGD32F303のメモリサイズに合わせる必要がある。
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
