/* ============================================================
 *  File: include/registry.h
 * ============================================================ */
#ifndef REGISTRY_H
#define REGISTRY_H

#define MAX_BOOKS   256
#define MAX_MEMBERS 256

/* ---- Book registry ---------------------------------------- */
typedef struct {
    char book_id[20];
    char title[80];
    char author[50];
} Book;

/* ---- Member registry -------------------------------------- */
typedef struct {
    char member_id[20];
    char full_name[50];
    char course_code[10];
} Member;

int  registry_load_books(const char *path);
int  registry_load_members(const char *path);

const Book   *registry_find_book(const char *book_id);
const Book   *registry_book_at(int index);
const Member *registry_find_member(const char *member_id);
const Member *registry_member_at(int index);

int  registry_book_count(void);
int  registry_member_count(void);

void registry_print_books(void);
void registry_print_members(void);

#endif /* REGISTRY_H */