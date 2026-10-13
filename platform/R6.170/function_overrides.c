/** \file
 * Function overrides needed for R6 1.7.0
 */
#include <dryos.h>
#include <property.h>
#include <bmp.h>
#include <config.h>
#include <consts.h>
#include <lens.h>
#include <edmac.h>

extern int uart_printf(const char * fmt, ...);

/*
 * Partition tables stuff. Got inlined in new generations.
 */
struct chs_entry
{
    uint8_t head;
    uint8_t sector; //sector + cyl_msb
    uint8_t cyl_lsb;
}__attribute__((packed));

struct partition
{
    uint8_t  state;
    struct   chs_entry start;
    uint8_t  type;
    struct   chs_entry end;
    uint32_t start_sector;
    uint32_t size;
}__attribute__((aligned,packed));

struct partition_table
{
    uint8_t  state; // 0x80 = bootable
    uint8_t  start_head;
    uint16_t start_cylinder_sector;
    uint8_t  type;
    uint8_t  end_head;
    uint16_t end_cylinder_sector;
    uint32_t sectors_before_partition;
    uint32_t sectors_in_partition;
}__attribute__((packed));

void fsuDecodePartitionTable(void * partIn, struct partition_table * pTable)
{
    struct partition * part = (struct partition *) partIn;
    pTable->state      = part->state;
    pTable->type       = part->type;
    pTable->start_head = part->start.head;
    pTable->end_head   = part->end.head;
    pTable->sectors_before_partition = part->start_sector;
    pTable->sectors_in_partition     = part->size;
    pTable->start_cylinder_sector = 0;
    pTable->end_cylinder_sector   = 0;
}

void platform_post_init(void)
{
}

int winsys_bmp_dirty_bit_neg = 0;

void gui_init_end(void)
{
}

extern void gui_enqueue_message(uint32_t, uint32_t, uint32_t, uint32_t);
void GUI_Control(int bgmt_code, int obj, int arg, int unknown)
{
    gui_enqueue_message(0, bgmt_code, obj, arg);
}

void LoadCalendarFromRTC(struct tm *tm)
{
    _LoadCalendarFromRTC(tm, 0, 0, 16);
}

void _EngDrvOut(uint32_t reg, uint32_t value)
{
}

uint32_t shamem_read(uint32_t addr)
{
    return 0;
}

void _engio_write(uint32_t* reg_list)
{
}

extern void* _AllocateMemory(size_t size);
extern void _FreeMemory(void* ptr);

void* _alloc_dma_memory(size_t size)
{
    void* raw = _AllocateMemory(size + 64);
    if (!raw) return NULL;
    uintptr_t ptr = ((uintptr_t)raw + 32) & ~31;
    *(void**)(ptr - 4) = raw;
    return (void*)(ptr | 0x40000000);
}

void _free_dma_memory(void* ptr)
{
    if (!ptr) return;
    uintptr_t p = ((uintptr_t)ptr) & ~0x40000000;
    void* raw = *(void**)(p - 4);
    _FreeMemory(raw);
}

int get_task_info_by_id(int unknown_flag, int task_id, void *task_attr)
{
    return -1;
}

extern int _FIO_GetFileSize64(const char *, void *);
int _FIO_GetFileSize(const char * filename, uint32_t * size)
{
    uint32_t size64[2];
    int code = _FIO_GetFileSize64(filename, &size64);
    *size = size64[0];
    return code;
}
