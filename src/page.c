#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "page.h"

Page* page_new(u32 page_id) {
    char* raw_page = calloc(1, PAGE_SIZE);
    Page* page = calloc(1, sizeof(Page));
    page->hdr = (Header*)raw_page;
    page->hdr->page_id = page_id;
    page->hdr->slot_count = 0;
    page->hdr->freespace_end = PAGE_SIZE;
    page->slots = (Slot*)(raw_page + PAGE_HDR_SIZE);
    page->raw_page = raw_page;
    return page;
}

void page_destroy(Page* page) {
    free(page->raw_page);
    free(page);
}

Slot* page_get_slot(Page* page, u16 slot_id) {
    if (slot_id < page->hdr->slot_count) {
        return (Slot*)(page->slots + slot_id);
    }
    return NULL;
}

u16 page_get_freespace(Page* page) {
    int freespace_start = PAGE_HDR_SIZE + page->hdr->slot_count * PAGE_SLOT_SIZE;
    return page->hdr->freespace_end - freespace_start;
}

int page_find_deleted_slot(Page* page, Slot** result) {
    for (int i = 0; i < page->hdr->slot_count; i++) {
        Slot* slot = page_get_slot(page, i);
        if (slot->deleted != 0) {
            *result = slot;
            return i;
        }
    }
    return -1;
}

Record* page_get(Page* page, u16 slot_id) {
    Slot* slot = page_get_slot(page, slot_id);
    if (slot->deleted == 1) {
        return NULL;
    } else {
        Record* record = malloc(sizeof(Record));
        record->page_id = page->hdr->page_id;
        record->slot_id = slot_id;
        record->key = page->raw_page + slot->offset;
        record->key_size = slot->key_size;
        record->data = page->raw_page + slot->offset + slot->key_size;
        record->data_size = slot->data_size;
        return record;
    }
}

Result page_insert(Page* page, char* key, u32 key_size, char* data, u32 data_size) {
    u16 freespace = page_get_freespace(page);
    u32 payload_size = key_size + data_size;
    if (payload_size > freespace) {
        return (Result) { .code = NotEnoughSpace };
    }

    Slot* slot = NULL;
    int index = page_find_deleted_slot(page, &slot);
    if (index == -1) {
        u32 required_space = payload_size + PAGE_SLOT_SIZE;
        if (required_space > freespace) {
            return (Result) { .code = NotEnoughSpace };
        }

        index = page->hdr->slot_count++;
        slot = page_get_slot(page, index);
    }

    // add slot
    u16 offset = page->hdr->freespace_end - payload_size;
    slot->offset = offset;
    slot->key_size = key_size;
    slot->data_size = data_size;

    // write data
    memcpy(page->raw_page + offset, key, key_size);
    memcpy(page->raw_page + offset + key_size, data, data_size);
    page->hdr->freespace_end -= payload_size;

    return (Result) { .code = Ok, .slot_id = index };
}

int page_delete(Page* page, u16 slot_id) {
    Slot* slot = page_get_slot(page, slot_id);
    if (slot == NULL || slot->deleted != 0) {
        return NotFound;
    }

    u16 payload_size = slot->key_size + slot->data_size;
    if (slot->offset != page->hdr->freespace_end) {
        // move data
        u16 source_offset = page->hdr->freespace_end;
        u16 dest_offset = source_offset + payload_size;
        u16 nbytes = slot->offset - source_offset;
        memmove(
            page->raw_page + dest_offset,
            page->raw_page + source_offset,
            nbytes
        );

        for (int i = 0; i < page->hdr->slot_count; i++) {
            Slot* curr_slot = page_get_slot(page, i);
            if (curr_slot->offset < slot->offset) {
                curr_slot->offset += payload_size;
            }
        }
    }

    // update slots
    slot->deleted = 1;
    page->hdr->freespace_end += payload_size;
    return Ok;
}

int page_update(Page* page, u16 slot_id, char* key, u32 key_size, char* data, u32 data_size) {
    Slot* slot = page_get_slot(page, slot_id);
    if (slot->deleted != 0) {
        return NotFound;
    }

    u16 curr_size = slot->key_size + slot->data_size;
    u32 new_size = key_size + data_size;
    if (new_size == curr_size) {
        // overwrite current data
        slot->key_size = key_size;
        slot->data_size = data_size;
        memcpy(page->raw_page + slot->offset, key, key_size);
        memcpy(page->raw_page + slot->offset + key_size, data, data_size);

        return Ok;
    } else if (new_size < curr_size) {
        // new data 'fits' within curr data
        // write the data and then move data before it for N bytes to the right
        int n = curr_size - new_size;
        u16 new_offset = slot->offset + n;
        memcpy(page->raw_page + new_offset, key, key_size);
        memcpy(page->raw_page + new_offset + key_size, data, data_size);

        // move data unless data was writtin in last entry
        if (slot->offset > page->hdr->freespace_end) {
            u16 source_offset = page->hdr->freespace_end;
            u16 dest_offset = source_offset + n;
            u16 bytes_to_move = slot->offset - source_offset;
            memmove(page->raw_page + dest_offset, page->raw_page + source_offset, bytes_to_move);

            // update slots
            for (int i = 0; i < page->hdr->slot_count; i++) {
                Slot* curr_slot = page_get_slot(page, i);
                if (curr_slot->offset < slot->offset) {
                    curr_slot->offset += n;
                }
            }
        }

        // update header and slots
        page->hdr->freespace_end += n;
        slot->offset = new_offset;
        slot->key_size = key_size;
        slot->data_size = data_size;
        return Ok;
    } else {
        int extra_bytes = new_size - curr_size;
        if (extra_bytes > page_get_freespace(page)) {
            return NotEnoughSpace;
        }

        // new data can't 'fit' within curr data
        // delete curr data and then insert new data
        int rc1 = page_delete(page, slot_id);
        assert(rc1 == Ok);
        assert(slot->deleted == 1);

        // add slot
        u16 offset = page->hdr->freespace_end - new_size;
        slot->offset = offset;
        slot->key_size = key_size;
        slot->data_size = data_size;
        slot->deleted = 0;
        page->hdr->freespace_end -= new_size;

        // write data
        memcpy(page->raw_page + offset, key, key_size);
        memcpy(page->raw_page + offset + key_size, data, data_size);
        return Ok;
    }
}
