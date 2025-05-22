#!/bin/bash

# libopencm3内の他のMCUファミリに関連する可能性のあるディレクトリを削除
echo "Deleting MCU family specific directories in libopencm3/include/libopencm3/..."
rm -rf libopencm3/include/libopencm3/efm32/
rm -rf libopencm3/include/libopencm3/lm3s/
rm -rf libopencm3/include/libopencm3/lm4f/
rm -rf libopencm3/include/libopencm3/lpc13xx/
rm -rf libopencm3/include/libopencm3/lpc17xx/
rm -rf libopencm3/include/libopencm3/lpc43xx/
rm -rf libopencm3/include/libopencm3/msp432/
rm -rf libopencm3/include/libopencm3/pac55xx/
rm -rf libopencm3/include/libopencm3/sam/
rm -rf libopencm3/include/libopencm3/swm050/
rm -rf libopencm3/include/libopencm3/vf6xx/

echo "Deleting MCU family specific directories in libopencm3/lib/..."
rm -rf libopencm3/lib/efm32/
rm -rf libopencm3/lib/lm3s/
rm -rf libopencm3/lib/lm4f/
rm -rf libopencm3/lib/lpc13xx/
rm -rf libopencm3/lib/lpc17xx/
rm -rf libopencm3/lib/lpc43xx/
rm -rf libopencm3/lib/msp432/
rm -rf libopencm3/lib/pac55xx/
rm -rf libopencm3/lib/sam/
rm -rf libopencm3/lib/swm050/
rm -rf libopencm3/lib/vf6xx/

# libopencm3/tests 内の特定のボード向けテストファイルを削除 (コメントアウトされているものは削除されません)
# 必要に応じてコメントを解除し、ファイル名を指定してください。
echo "Deleting specific test files in libopencm3/tests/gadget-zero/ (if uncommented)..."
# rm -rf libopencm3/tests/gadget-zero/main-efm32hg309-generic.c
# rm -rf libopencm3/tests/gadget-zero/openocd.efm32hg309-generic.cfg
# (他の main-*.c や openocd.*.cfg も同様に追加またはコメント解除してください)

# python スクリプトやドキュメント類を削除
echo "Deleting python scripts and documentation..."
rm -rf python/
rm -rf libopencm3/doc/
# rm -rf libopencm3/scripts/data/ # (genlink.py などが使用する可能性あり注意のためコメントアウト)
echo "Note: libopencm3/scripts/data/ was not deleted due to potential use by genlink.py. Uncomment if sure."

# .d (依存関係ファイル) を削除
echo "Deleting .d (dependency) files..."
find . -name "*.d" -type f -delete

echo "Cleanup complete."