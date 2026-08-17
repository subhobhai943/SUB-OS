#ifndef _DRIVERS_NVME_H
#define _DRIVERS_NVME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define NVME_PCI_CLASS    0x01
#define NVME_PCI_SUBCLASS 0x08
#define NVME_PCI_PROGIF   0x02

#define NVME_REG_CAP   0x0000 // Controller Capabilities
#define NVME_REG_VS    0x0008 // Version
#define NVME_REG_INTMS 0x000C // Interrupt Mask Set
#define NVME_REG_INTMC 0x0010 // Interrupt Mask Clear
#define NVME_REG_CC    0x0014 // Controller Configuration
#define NVME_REG_CSTS  0x001C // Controller Status
#define NVME_REG_AQA   0x0024 // Admin Queue Attributes
#define NVME_REG_ASQ   0x0028 // Admin Submission Queue Base
#define NVME_REG_ACQ   0x0030 // Admin Completion Queue Base

#define NVME_OP_ADMIN_DELETE_SQ 0x00
#define NVME_OP_ADMIN_CREATE_SQ 0x01
#define NVME_OP_ADMIN_GET_LOG   0x02
#define NVME_OP_ADMIN_DELETE_CQ 0x04
#define NVME_OP_ADMIN_CREATE_CQ 0x05
#define NVME_OP_ADMIN_IDENTIFY  0x06

#define NVME_OP_NVM_FLUSH 0x00
#define NVME_OP_NVM_WRITE 0x01
#define NVME_OP_NVM_READ  0x02

typedef struct {
    uint8_t  opcode;
    uint8_t  flags;
    uint16_t cid;
    uint32_t nsid;
    uint64_t reserved;
    uint64_t mptr;
    uint64_t dptr_prp1;
    uint64_t dptr_prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed)) nvme_sqe_t;

typedef struct {
    uint32_t cdw0;
    uint32_t reserved;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t cid;
    uint16_t status;
} __attribute__((packed)) nvme_cqe_t;

typedef struct {
    char     model[40];
    char     serial[20];
    char     firmware[8];
    uint64_t total_capacity_bytes;
    uint64_t total_sectors;
    uint32_t sector_size;
    uint32_t max_transfer_sectors;
    uint32_t queue_depth;
    bool     initialized;
} nvme_device_info_t;

bool nvme_init(void);
bool nvme_is_detected(void);
const nvme_device_info_t* nvme_get_info(void);
int  nvme_read_blocks(uint64_t lba, uint32_t count, void* buffer);
int  nvme_write_blocks(uint64_t lba, uint32_t count, const void* buffer);

#endif // _DRIVERS_NVME_H
