#include <inttypes.h>

#define u32 uint32_t
#define u16 uint16_t
#define u8 uint8_t

#ifdef TEST_MODE
#define PAGE_SIZE 256
#else
#define PAGE_SIZE 8196
#endif

#define PAGE_HDR_SIZE 8
#define PAGE_SLOT_SIZE 8

typedef struct Header {
    u32 page_id;
    u16 slot_count;
    u16 freespace_end;
} Header;

typedef struct Slot {
    u16 key_size;
    u16 data_size;
    u16 offset;
    u8 deleted;
    int : 8;
} Slot;

typedef struct Page {
    Header* hdr;
    Slot* slots;
    char* raw_page;
} Page;

typedef struct Record {
    u32 page_id;
    u16 slot_id;
    char* key;
    u16 key_size;
    char* data;
    u16 data_size;
    u16 record_size;
} Record;

typedef struct Result {
    int code;
    u16 slot_id;
} Result;

typedef enum ReturnCode {
    Ok,
    NotEnoughSpace,
    NotFound
} ReturnCode;

Page* page_new(u32 page_id);
void page_destroy(Page* page);
Record* page_get(Page* page, u16 slot_id);
Result page_insert(Page* page, char* key, u32 key_size, char* data, u32 data_size);
int page_update(Page* page, u16 slot_id, char* key, u32 key_size, char* data, u32 data_size);
int page_delete(Page* page, u16 slot_id);

// extra
u16 page_get_freespace(Page* page);
Slot* page_get_slot(Page* page, u16 slot_id);
