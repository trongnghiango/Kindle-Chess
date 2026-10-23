#!/bin/bash
set -euo pipefail

KINDLE_DIR="/run/media/ka/Kindle"
EXT_DIR="$KINDLE_DIR/extensions"
TARGET_DIR="$EXT_DIR/kindlechess"
ZIP_FILE="/home/ka/Downloads/kindlechess-kual.zip"

echo "========================================================"
echo "    KINDLECHESS AUTOMATED INSTALLER FOR KINDLE PW2/3    "
echo "========================================================"

if [ ! -f "$ZIP_FILE" ]; then
    echo "❌ Không tìm thấy file zip tại: $ZIP_FILE"
    exit 1
fi

echo "1. Kiểm tra kết nối Kindle..."
if ! lsblk | grep -q sda1; then
    echo "❌ Không tìm thấy ổ đĩa Kindle (sda1). Vui lòng cắm cáp USB vào máy."
    exit 1
fi

echo "2. Đảm bảo phân vùng Kindle ở chế độ ghi (Read-Write)..."
# Tự động sửa lỗi cờ dirty của FAT32 qua UDisks2 (không cần quyền root)
udisksctl unmount -b /dev/sda1 2>/dev/null || true
gdbus call --system --dest org.freedesktop.UDisks2 --object-path /org/freedesktop/UDisks2/block_devices/sda1 --method org.freedesktop.UDisks2.Filesystem.Repair "{}" >/dev/null 2>&1 || true
udisksctl mount -b /dev/sda1 >/dev/null 2>&1

if [ ! -d "$KINDLE_DIR" ]; then
    echo "❌ Thư mục $KINDLE_DIR không tồn tại sau khi mount."
    exit 1
fi

if [ ! -w "$KINDLE_DIR" ]; then
    echo "⚠️ Ổ đĩa vẫn ở chế độ Read-Only, đang thử sửa bằng fsck..."
    udisksctl unmount -b /dev/sda1 2>/dev/null || true
    sudo fsck.vfat -a /dev/sda1
    udisksctl mount -b /dev/sda1
fi

echo "3. Sao lưu dữ liệu ván cờ cũ (save.txt, games, slots)..."
BACKUP_DIR="/tmp/kindlechess_backup_$$"
mkdir -p "$BACKUP_DIR"
if [ -d "$TARGET_DIR/data" ]; then
    cp -r "$TARGET_DIR/data" "$BACKUP_DIR/" 2>/dev/null || true
    echo "-> Đã sao lưu dữ liệu cũ vào $BACKUP_DIR"
fi

echo "4. Gỡ bỏ bản KindleChess cũ khỏi Kindle..."
rm -rf "$TARGET_DIR"

echo "5. Giải nén gói cài đặt mới..."
TEMP_UNPACK="/tmp/kc_unpack_$$"
mkdir -p "$TEMP_UNPACK"
unzip -q "$ZIP_FILE" -d "$TEMP_UNPACK/"

# Xử lý trường hợp GitHub Actions bọc zip lồng zip (nested zip)
ACTUAL_ZIP="$ZIP_FILE"
if [ -f "$TEMP_UNPACK/kindlechess-kual.zip" ]; then
    ACTUAL_ZIP="$TEMP_UNPACK/kindlechess-kual.zip"
fi

mkdir -p "$EXT_DIR"
unzip -q -o "$ACTUAL_ZIP" -d "$EXT_DIR/"
rm -rf "$TEMP_UNPACK"

echo "6. Khôi phục dữ liệu ván cờ và cài đặt của bạn..."
if [ -d "$BACKUP_DIR/data" ]; then
    mkdir -p "$TARGET_DIR/data"
    [ -f "$BACKUP_DIR/data/save.txt" ] && cp "$BACKUP_DIR/data/save.txt" "$TARGET_DIR/data/" || true
    if [ -d "$BACKUP_DIR/data/games" ]; then
        mkdir -p "$TARGET_DIR/data/games"
        cp -r "$BACKUP_DIR/data/games/." "$TARGET_DIR/data/games/" 2>/dev/null || true
    fi
    if [ -d "$BACKUP_DIR/data/slots" ]; then
        mkdir -p "$TARGET_DIR/data/slots"
        cp -r "$BACKUP_DIR/data/slots/." "$TARGET_DIR/data/slots/" 2>/dev/null || true
    fi
    rm -rf "$BACKUP_DIR"
fi

echo "7. Cấp quyền thực thi cho các file nhị phân..."
chmod +x "$TARGET_DIR/bin/kindlechess"
chmod +x "$TARGET_DIR/bin/start.sh"
[ -f "$TARGET_DIR/bin/stockfish" ] && chmod +x "$TARGET_DIR/bin/stockfish"

echo "8. Đồng bộ dữ liệu vào chip nhớ Kindle (sync)..."
sync

echo ""
echo "========================================================"
echo "    ✅ CÀI ĐẶT THÀNH CÔNG KINDLECHESS MỚI LÊN KINDLE!   "
echo "========================================================"
echo "Bản mới gồm có:"
echo "  - Dãy số (1-8) và chữ (a-h) nằm ngoài ô cờ gọn gàng."
echo "  - Đầy đủ 12 quân cờ PNG sắc nét."
echo "  - Stockfish 18 NEON tăng tốc phần cứng Cortex-A9."
echo "  - Nhận diện UCI Engine không bị lỗi FAT32."
echo "  - Tích hợp sẵn libatomic trong thư mục lib/."
echo "  - Chống tắt màn hình tự động khi đang suy nghĩ nước cờ."
echo ""
read -p "Bạn có muốn ngắt kết nối an toàn (Eject) Kindle luôn không? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    udisksctl unmount -b /dev/sda1
    echo "-> Đã ngắt kết nối an toàn! Bạn có thể rút cáp USB và mở KUAL."
else
    echo "-> Khi rút cáp, vui lòng nhớ chạy: udisksctl unmount -b /dev/sda1"
fi
