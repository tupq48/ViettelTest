# ViettelTest - 5G Network Simulation

Dự án mô phỏng mạng 5G cơ bản với các thành phần: AMF (Access and Mobility Management Function), gNodeB (Next Generation Node B), và UE (User Equipment). Sử dụng C để implement các chức năng paging, MIB broadcast, và đồng bộ SFN.

## Yêu cầu hệ thống

- CMake
- GCC hoặc compiler C tương thích
- Linux

## Build

```bash
mkdir build
cd build
cmake ..
make
```

## Chạy

Mở 3 terminal riêng biệt và chạy theo thứ tự sau:

### Terminal 1: gNodeB
```bash
cd build
./gnodeb
```
Khởi động gNodeB server: broadcast MIB và lắng nghe paging request từ AMF qua TCP.

### Terminal 2: UE
```bash
cd build
./ue
```
Khởi động UE: đồng bộ MIB với gNodeB và giả lập DRX wakeup để nhận paging.

### Terminal 3: AMF
```bash
cd build
./amf
```
Giả lập AMF: gửi paging message đến gNodeB cho UE.

## Kiến trúc

- **AMF**: Gửi paging qua TCP đến gNodeB.
- **gNodeB**: Nhận paging, gửi RRC paging, MIB qua UDP đến UE.
- **UE**: Nhận MIB để sync SFN, wakeup theo DRX để nhận paging.

## Cấu hình

Các tham số như port, cycle, UE ID được định nghĩa trong `include/config/common.h`.

## Lưu ý

- Vì gNodeB là server host TCP, nên AMF chạy sau gNodeB.
- Sử dụng Ctrl+C để dừng các process.

