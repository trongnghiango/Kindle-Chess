#!/bin/bash
set -euo pipefail

KINDLE_DIR="/run/media/ka/Kindle"
EXT_DIR="$KINDLE_DIR/extensions"
TARGET_DIR="$EXT_DIR/kindlechess"
ZIP_FILE="/home/ka/Downloads/kindlechess-kual.zip"

echo "=== Cài đặt KindleChess mới lên máy Kindle ==="

if [ ! -d "$KINDLE_DIR" ]; then
    echo "❌ Lỗi: Không tìm thấy ổ đĩa Kindle tại $KINDLE_DIR."
    echo "Vui lòng cắm cáp USB và mở khóa Kindle."
    exit 1
fi

if [ ! -w "$KINDLE_DIR" ]; then
    echo "⚠️ Ổ đĩa Kindle đang ở chế độ Read-Only (do lần trước rút cáp chưa Eject an toàn)."
    echo "Đang thử sửa lỗi hệ thống file bằng fsck..."
    udisksctl unmount -b /dev/sda1 2>/dev/null || true
    sudo fsck.vfat -a /dev/sda1
    udisksctl mount -b /dev/sda1
fi

if [ ! -w "$KINDLE_DIR" ]; then
    echo "❌ Lỗi: Ổ đĩa vẫn ở chế độ Read-Only."
    exit 1
fi

echo "1. Đang sao lưu dữ liệu ván cờ cũ (save.txt, games, slots)..."
BACKUP_DIR="/tmp/kindlechess_backup"
mkdir -p "$BACKUP_DIR"
if [ -d "$TARGET_DIR/data" ]; then
    cp -r "$TARGET_DIR/data" "$BACKUP_DIR/" 2>/dev/null || true
    echo "-> Đã sao lưu dữ liệu vào $BACKUP_DIR"
fi

echo "2. Đang xóa bản KindleChess cũ..."
rm -rf "$TARGET_DIR"

echo "3. Đang giải nén bản mới từ $ZIP_FILE..."
unzip -q "$ZIP_FILE" -d "$EXT_DIR/"

echo "4. Đang khôi phục dữ liệu ván cờ và cài đặt..."
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

echo "5. Cấp quyền thực thi cho các file nhị phân..."
chmod +x "$TARGET_DIR/bin/kindlechess"
chmod +x "$TARGET_DIR/bin/start.sh"
[ -f "$TARGET_DIR/bin/stockfish" ] && chmod +x "$TARGET_DIR/bin/stockfish"

echo "6. Đồng bộ dữ liệu vào chip nhớ Kindle (sync)..."
sync

echo ""
echo "✅ HOÀN TẤT CÀI ĐẶT THÀNH CÔNG!"
echo "Đầy đủ 12 quân cờ, Stockfish 18 NEON, giao diện PW3 độ phân giải cao và tính năng chống tắt màn hình."
echo ""
echo "Bạn có thể ngắt kết nối an toàn bằng lệnh:"
echo "  udisksctl unmount -b /dev/sda1"
echo "Sau đó rút cáp USB và mở KUAL trên Kindle để chơi!"
