#include "../lib/unity/unity.h"
#include "page.h"
#include <stdlib.h>

char* record1_key = "key1";
u32 record1_key_size = 5;
char* record1_data = "1111111111";
u32 record1_data_size = 11;
u32 record1_size = 16;

char* record2_key = "key2";
u32 record2_key_size = 5;
char* record2_data = "222222222222";
u32 record2_data_size = 13;
u32 record2_size = 18;

////////////////
// TEST PAGE 1
////////////////
//
//    8      8       8        198           18        16    
// ----------------------------------------------------------
// | hdr | slot1 | slot2 |  free space | record2  | record1 |
// ----------------------------------------------------------
// 0     8      16       24           222        240       256
//
Page* setUpTestPage1() {
    Page* page = page_new(0);
    u16 freespace = page_get_freespace(page);
    Result result = page_insert(page, record1_key, record1_key_size, record1_data, record1_data_size);
    TEST_ASSERT_EQUAL_INT(Ok, result.code);
    TEST_ASSERT_EQUAL_INT(0, result.slot_id);
    TEST_ASSERT_EQUAL_INT(1, page->hdr->slot_count);
    TEST_ASSERT_EQUAL_INT(240, page->hdr->freespace_end);
    TEST_ASSERT_EQUAL_INT(224, page_get_freespace(page));

    Result result2 = page_insert(page, record2_key, record2_key_size, record2_data, record2_data_size);
    TEST_ASSERT_EQUAL_INT(Ok, result2.code);
    TEST_ASSERT_EQUAL_INT(1, result2.slot_id);
    TEST_ASSERT_EQUAL_INT(2, page->hdr->slot_count);
    TEST_ASSERT_EQUAL_INT(222, page->hdr->freespace_end);
    TEST_ASSERT_EQUAL_INT(198, page_get_freespace(page));

    Record* record1 = page_get(page, result.slot_id);
    TEST_ASSERT_EQUAL_STRING(record1_key, record1->key);
    TEST_ASSERT_EQUAL_INT(record1_key_size, record1->key_size);
    TEST_ASSERT_EQUAL_STRING(record1_data, record1->data);
    TEST_ASSERT_EQUAL_INT(record1_data_size, record1->data_size);
    free(record1);

    Record* record2 = page_get(page, result2.slot_id);
    TEST_ASSERT_EQUAL_STRING(record2_key, record2->key);
    TEST_ASSERT_EQUAL_INT(record2_key_size, record2->key_size);
    TEST_ASSERT_EQUAL_STRING(record2_data, record2->data);
    TEST_ASSERT_EQUAL_INT(record2_data_size, record2->data_size);
    free(record2);

    return page;
}

void test_createAndDestroy() {
    u32 page_id = 123;
    Page* page = page_new(page_id);
    TEST_ASSERT_EQUAL_INT(page_id, page->hdr->page_id);
    TEST_ASSERT_EQUAL_INT(PAGE_SIZE, page->hdr->freespace_end);
    TEST_ASSERT_EQUAL_INT(0, page->hdr->slot_count);
    TEST_ASSERT_EQUAL_INT(PAGE_SIZE - PAGE_HDR_SIZE, page_get_freespace(page));
    page_destroy(page);
}

void test_insertTooLarge() {
    Page* page = setUpTestPage1();
    char* key = "key1key1key1key1key1";
    u32 key_size = 21;
    char* data =
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111";
    u32 data_size = 181;

    Result res = page_insert(page, key, key_size, data, data_size);
    TEST_ASSERT_EQUAL_INT(NotEnoughSpace, res.code);

    page_destroy(page);
}

void test_insertReuseDeletedSlot() {
    Page* page = setUpTestPage1();
    int rc = page_delete(page, 0);
    TEST_ASSERT_EQUAL_INT(Ok, rc);
    // fse = 238
    // fs = 214

    char* key = "key123";
    u32 key_size = 7;
    char* data = "777777777777";
    u32 data_size = 13;
    u32 size = 20;

    Result result = page_insert(page, key, key_size, data, data_size);
    TEST_ASSERT_EQUAL_INT(Ok, result.code);
    TEST_ASSERT_EQUAL_INT(0, result.slot_id);
    TEST_ASSERT_EQUAL_INT(2, page->hdr->slot_count);
    TEST_ASSERT_EQUAL_INT(218, page->hdr->freespace_end);
    TEST_ASSERT_EQUAL_INT(194, page_get_freespace(page));

    page_destroy(page);
}

void test_updateMiddleWithSameSize() {
    Page* page = setUpTestPage1();
    char* key = "key123";
    u32 key_size = 7;
    char* data = "77777777";
    u32 data_size = 9;
    u32 size = 16;

    int rc = page_update(page, 0, key, key_size, data, data_size);
    TEST_ASSERT_EQUAL_INT(Ok, rc);
    TEST_ASSERT_EQUAL_INT(2, page->hdr->slot_count);
    TEST_ASSERT_EQUAL_INT(222, page->hdr->freespace_end);
    TEST_ASSERT_EQUAL_INT(198, page_get_freespace(page));

    Record* record = page_get(page, 0);
    TEST_ASSERT_EQUAL_STRING(key, record->key);
    TEST_ASSERT_EQUAL_INT(key_size, record->key_size);
    TEST_ASSERT_EQUAL_STRING(data, record->data);
    TEST_ASSERT_EQUAL_INT(data_size, record->data_size);
    free(record);

    page_destroy(page);
}

void test_updateMiddleWithSmallerSize() {
    Page* page = setUpTestPage1();
    char* key = "key1";
    u32 key_size = 5;
    char* data = "1111";
    u32 data_size = 5;
    u32 size = 10;

    int rc = page_update(page, 0, key, key_size, data, data_size);
    TEST_ASSERT_EQUAL_INT(Ok, rc);
    TEST_ASSERT_EQUAL_INT(2, page->hdr->slot_count);
    TEST_ASSERT_EQUAL_INT(228, page->hdr->freespace_end);
    TEST_ASSERT_EQUAL_INT(204, page_get_freespace(page));

    Record* record = page_get(page, 0);
    TEST_ASSERT_EQUAL_STRING(key, record->key);
    TEST_ASSERT_EQUAL_INT(key_size, record->key_size);
    TEST_ASSERT_EQUAL_STRING(data, record->data);
    TEST_ASSERT_EQUAL_INT(data_size, record->data_size);
    free(record);

    Record* record2 = page_get(page, 1);
    TEST_ASSERT_EQUAL_STRING(record2_key, record2->key);
    TEST_ASSERT_EQUAL_INT(record2_key_size, record2->key_size);
    TEST_ASSERT_EQUAL_STRING(record2_data, record2->data);
    TEST_ASSERT_EQUAL_INT(record2_data_size, record2->data_size);
    free(record2);

    page_destroy(page);
}

void test_updateMiddleWithLargerSize() {
    Page* page = setUpTestPage1();
    char* key = "key1";
    u32 key_size = 5;
    char* data = "1111111111111111";
    u32 data_size = 17;
    u32 size = 22;

    int rc = page_update(page, 0, key, key_size, data, data_size);
    TEST_ASSERT_EQUAL_INT(Ok, rc);
    TEST_ASSERT_EQUAL_INT(2, page->hdr->slot_count);
    TEST_ASSERT_EQUAL_INT(216, page->hdr->freespace_end);
    TEST_ASSERT_EQUAL_INT(192, page_get_freespace(page));

    Record* record = page_get(page, 0);
    TEST_ASSERT_EQUAL_STRING(key, record->key);
    TEST_ASSERT_EQUAL_INT(key_size, record->key_size);
    TEST_ASSERT_EQUAL_STRING(data, record->data);
    TEST_ASSERT_EQUAL_INT(data_size, record->data_size);
    free(record);

    Record* record2 = page_get(page, 1);
    TEST_ASSERT_EQUAL_STRING(record2_key, record2->key);
    TEST_ASSERT_EQUAL_INT(record2_key_size, record2->key_size);
    TEST_ASSERT_EQUAL_STRING(record2_data, record2->data);
    TEST_ASSERT_EQUAL_INT(record2_data_size, record2->data_size);
    free(record2);

    page_destroy(page);
}

void test_updateDeleted() {
    Page* page = setUpTestPage1();
    int rc = page_delete(page, 0);
    TEST_ASSERT_EQUAL_INT(Ok, rc);

    int rc2 = page_update(page, 0, record1_key, record1_key_size, record1_data, record1_data_size);
    TEST_ASSERT_EQUAL_INT(NotFound, rc2);

    page_destroy(page);
}

void test_updateTooLarge() {
    Page* page = setUpTestPage1();
    char* key = "key1key1key1key1key1";
    u32 key_size = 21;
    char* data =
        "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111";
    u32 data_size = 379;

    int rc = page_update(page, 0, key, key_size, data, data_size);
    TEST_ASSERT_EQUAL_INT(NotEnoughSpace, rc);

    page_destroy(page);
}

void test_deleteMiddle() {
    Page* page = setUpTestPage1();
    int rc = page_delete(page, 0);
    TEST_ASSERT_EQUAL_INT(Ok, rc);
    TEST_ASSERT_EQUAL_INT(214, page_get_freespace(page));
    TEST_ASSERT_EQUAL_INT(238, page->hdr->freespace_end);
    TEST_ASSERT_EQUAL_INT(2, page->hdr->slot_count);

    Slot* deleted_slot = page_get_slot(page, 0);
    TEST_ASSERT_EQUAL_INT(1, deleted_slot->deleted);

    Slot* moved_slot = page_get_slot(page, 1);
    TEST_ASSERT_EQUAL_INT(0, moved_slot->deleted);
    TEST_ASSERT_EQUAL_INT(238, moved_slot->offset);
    TEST_ASSERT_EQUAL_INT(record2_key_size, moved_slot->key_size);
    TEST_ASSERT_EQUAL_INT(record2_data_size, moved_slot->data_size);

    TEST_ASSERT_NULL(page_get(page, 0));

    Record* record = page_get(page, 1);
    TEST_ASSERT_EQUAL_STRING(record2_key, record->key);
    TEST_ASSERT_EQUAL_STRING(record2_data, record->data);
    free(record);

    page_destroy(page);
}

void test_deleteLeftmost() {
    Page* page = setUpTestPage1();
    int rc = page_delete(page, 1);
    TEST_ASSERT_EQUAL_INT(Ok, rc);
    TEST_ASSERT_EQUAL_INT(216, page_get_freespace(page));
    TEST_ASSERT_EQUAL_INT(240, page->hdr->freespace_end);
    TEST_ASSERT_EQUAL_INT(2, page->hdr->slot_count);

    Slot* deleted_slot = page_get_slot(page, 1);
    TEST_ASSERT_EQUAL_INT(1, deleted_slot->deleted);

    Slot* other_slot = page_get_slot(page, 0);
    TEST_ASSERT_EQUAL_INT(0, other_slot->deleted);
    TEST_ASSERT_EQUAL_INT(240, other_slot->offset);
    TEST_ASSERT_EQUAL_INT(record1_key_size, other_slot->key_size);
    TEST_ASSERT_EQUAL_INT(record1_data_size, other_slot->data_size);

    Record* record = page_get(page, 0);
    TEST_ASSERT_EQUAL_STRING(record1_key, record->key);
    TEST_ASSERT_EQUAL_STRING(record1_data, record->data);
    free(record);

    page_destroy(page);
}

void test_deleteNonexistent() {
    Page* page = setUpTestPage1();
    int rc = page_delete(page, 10);
    TEST_ASSERT_EQUAL_INT(NotFound, rc);
    page_destroy(page);
}

void setUp(void) {
}

void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_createAndDestroy);
    RUN_TEST(test_insertTooLarge);
    RUN_TEST(test_insertReuseDeletedSlot);
    RUN_TEST(test_updateMiddleWithSameSize);
    RUN_TEST(test_updateMiddleWithSmallerSize);
    RUN_TEST(test_updateMiddleWithLargerSize);
    RUN_TEST(test_updateDeleted);
    RUN_TEST(test_updateTooLarge);
    RUN_TEST(test_deleteMiddle);
    RUN_TEST(test_deleteLeftmost);
    RUN_TEST(test_deleteNonexistent);
    return UNITY_END();
}
