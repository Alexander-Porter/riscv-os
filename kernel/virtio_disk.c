#include "types.h"
#include "memlayout.h"
#include "include/riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "buf.h"
#include "virtio.h"
#include "param.h"
#include "global_func.h"

#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

static struct disk {
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    struct virtq_used *used;

    char free[NUM];
    uint16 used_idx;

    struct {
        struct buf *b;
        char status;
    } info[NUM];

    struct virtio_blk_req ops[NUM];

    struct spinlock vdisk_lock;
} disk;

static int alloc_desc(void)
{
    for (int i = 0; i < NUM; i++)
    {
        if (disk.free[i])
        {
            disk.free[i] = 0;
            return i;
        }
    }
    return -1;
}

static void free_desc(int i)
{
    if (i >= NUM)
        panic("free_desc");
    if (disk.free[i])
        panic("free_desc 2");
    disk.desc[i].addr = 0;
    disk.desc[i].len = 0;
    disk.desc[i].flags = 0;
    disk.desc[i].next = 0;
    disk.free[i] = 1;
    wakeup(&disk.free[0]);
}

static void free_chain(int i)
{
    while (1)
    {
        int flag = disk.desc[i].flags;
        int next = disk.desc[i].next;
        free_desc(i);
        if (flag & VRING_DESC_F_NEXT)
            i = next;
        else
            break;
    }
}

static int alloc3_desc(int *idx)
{
    for (int i = 0; i < 3; i++)
    {
        idx[i] = alloc_desc();
        if (idx[i] < 0)
        {
            for (int j = 0; j < i; j++)
                free_desc(idx[j]);
            return -1;
        }
    }
    return 0;
}

void virtio_disk_init(void)
{
    initlock(&disk.vdisk_lock, "virtio_disk");

    uint32 status = 0;

    if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 || *R(VIRTIO_MMIO_VERSION) != 2 || *R(VIRTIO_MMIO_DEVICE_ID) != 2 || *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551)
        panic("virtio disk not found");

    *R(VIRTIO_MMIO_STATUS) = status;
    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    *R(VIRTIO_MMIO_STATUS) = status;
    status |= VIRTIO_CONFIG_S_DRIVER;
    *R(VIRTIO_MMIO_STATUS) = status;

    uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
    features &= ~(1 << 5);  // VIRTIO_BLK_F_RO
    features &= ~(1 << 7);  // VIRTIO_BLK_F_SCSI
    features &= ~(1 << 6);  // VIRTIO_BLK_F_CONFIG_WCE
    features &= ~(1 << 12); // VIRTIO_BLK_F_MQ
    features &= ~(1 << 27); // VIRTIO_F_ANY_LAYOUT
    features &= ~(1 << 29); // VIRTIO_RING_F_EVENT_IDX
    features &= ~(1 << 28); // VIRTIO_RING_F_INDIRECT_DESC
    *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

    status |= VIRTIO_CONFIG_S_FEATURES_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    status = *R(VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_CONFIG_S_FEATURES_OK))
        panic("virtio disk FEATURES_OK unset");

    *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
    if (*R(VIRTIO_MMIO_QUEUE_READY))
        panic("virtio queue should not be ready");

    uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max == 0)
        panic("virtio queue num max 0");
    if (max < NUM)
        panic("virtio queue too short");

    disk.desc = (struct virtq_desc *)alloc_page();
    disk.avail = (struct virtq_avail *)alloc_page();
    disk.used = (struct virtq_used *)alloc_page();
    if (!disk.desc || !disk.avail || !disk.used)
        panic("virtio kalloc");
    memset(disk.desc, 0, PGSIZE);
    memset(disk.avail, 0, PGSIZE);
    memset(disk.used, 0, PGSIZE);

    *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;
    *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)disk.desc;
    *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)disk.desc >> 32;
    *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64)disk.avail;
    *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)disk.avail >> 32;
    *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64)disk.used;
    *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)disk.used >> 32;

    *R(VIRTIO_MMIO_QUEUE_READY) = 1;

    for (int i = 0; i < NUM; i++)
        disk.free[i] = 1;

    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    *R(VIRTIO_MMIO_STATUS) = status;
}

void virtio_disk_rw(struct buf *b, int write)
{
    uint64 sector = b->blockno * (BSIZE / 512);

    acquire(&disk.vdisk_lock);

    int idx[3];
    while (alloc3_desc(idx) < 0)
        sleep(&disk.free[0], &disk.vdisk_lock);

    struct virtio_blk_req *req = &disk.ops[idx[0]];
    req->type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    req->reserved = 0;
    req->sector = sector;

    disk.desc[idx[0]].addr = (uint64)req;
    disk.desc[idx[0]].len = sizeof(*req);
    disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
    disk.desc[idx[0]].next = idx[1];

    disk.desc[idx[1]].addr = (uint64)b->data;
    disk.desc[idx[1]].len = BSIZE;
    disk.desc[idx[1]].flags = write ? 0 : VRING_DESC_F_WRITE;
    disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
    disk.desc[idx[1]].next = idx[2];

    disk.info[idx[0]].status = 0xff;
    disk.desc[idx[2]].addr = (uint64)&disk.info[idx[0]].status;
    disk.desc[idx[2]].len = 1;
    disk.desc[idx[2]].flags = VRING_DESC_F_WRITE;
    disk.desc[idx[2]].next = 0;

    b->disk = 1;
    disk.info[idx[0]].b = b;

    disk.avail->ring[disk.avail->idx % NUM] = idx[0];
    __sync_synchronize();
    disk.avail->idx++;
    __sync_synchronize();

    *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;

    while (b->disk == 1)
        sleep(b, &disk.vdisk_lock);

    disk.info[idx[0]].b = 0;
    free_chain(idx[0]);

    release(&disk.vdisk_lock);
}

void virtio_disk_intr(void)
{
    acquire(&disk.vdisk_lock);

    *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;
    __sync_synchronize();

    while (disk.used_idx != disk.used->idx)
    {
        __sync_synchronize();
        int id = disk.used->ring[disk.used_idx % NUM].id;
        if (disk.info[id].status != 0)
            panic("virtio status");
        struct buf *b = disk.info[id].b;
        b->disk = 0;
        wakeup(b);
        disk.used_idx++;
    }

    release(&disk.vdisk_lock);
}
