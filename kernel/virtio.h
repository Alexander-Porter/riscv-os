#ifndef __VIRTIO_H
#define __VIRTIO_H

#include "types.h"
#include "memlayout.h"

// virtio 寄存器偏移
#define VIRTIO_MMIO_MAGIC_VALUE   0x000
#define VIRTIO_MMIO_VERSION       0x004
#define VIRTIO_MMIO_DEVICE_ID     0x008
#define VIRTIO_MMIO_VENDOR_ID     0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_QUEUE_SEL     0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034
#define VIRTIO_MMIO_QUEUE_NUM     0x038
#define VIRTIO_MMIO_QUEUE_READY   0x044
#define VIRTIO_MMIO_QUEUE_DESC_LOW 0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_DRIVER_DESC_LOW 0x090
#define VIRTIO_MMIO_DRIVER_DESC_HIGH 0x094
#define VIRTIO_MMIO_DEVICE_DESC_LOW 0x0a0
#define VIRTIO_MMIO_DEVICE_DESC_HIGH 0x0a4
#define VIRTIO_MMIO_QUEUE_NOTIFY  0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK 0x064
#define VIRTIO_MMIO_STATUS        0x070

// 状态位
#define VIRTIO_CONFIG_S_ACKNOWLEDGE 1
#define VIRTIO_CONFIG_S_DRIVER      2
#define VIRTIO_CONFIG_S_FEATURES_OK 8
#define VIRTIO_CONFIG_S_DRIVER_OK   4

// 队列大小
#define NUM 8

struct virtq_desc {
    uint64 addr;
    uint32 len;
    uint16 flags;
    uint16 next;
};

#define VRING_DESC_F_NEXT 1
#define VRING_DESC_F_WRITE 2

struct virtq_avail {
    uint16 flags;
    uint16 idx;
    uint16 ring[NUM];
    uint16 unused;
};

struct virtq_used_elem {
    uint32 id;
    uint32 len;
};

struct virtq_used {
    uint16 flags;
    uint16 idx;
    struct virtq_used_elem ring[NUM];
};

#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1

struct virtio_blk_req {
    uint32 type;
    uint32 reserved;
    uint64 sector;
};

void virtio_disk_init(void);
void virtio_disk_rw(struct buf *b, int write);
// 新增：非阻塞提交与等待，用于批量 I/O
void virtio_disk_submit(struct buf *b, int write);
void virtio_disk_wait(struct buf *b);
void virtio_disk_intr(void);

#endif
