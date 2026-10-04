/* ============================================================
 *  File: src/registry.c
 * ============================================================ */
#include "registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static Book   g_books[MAX_BOOKS];
static int    g_book_count   = 0;

static Member g_members[MAX_MEMBERS];
static int    g_member_count = 0;

/* ------------------------------------------------------------ */
static void trim(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i])) i++;
    if (i) memmove(s, s + i, strlen(s + i) + 1);
}

/* Split a CSV line in place; returns number of fields found. */
static int split_csv(char *line, char *fields[], int max)
{
    int   n = 0;
    char *p = line;

    while (n < max) {
        fields[n++] = p;
        char *comma = strchr(p, ',');
        if (!comma) break;
        *comma = '\0';
        p = comma + 1;
    }
    return n;
}

/* ------------------------------------------------------------ */
int registry_load_books(const char *path)
{
    FILE *fp;
    char  line[512];

    g_book_count = 0;
    fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "ERROR: cannot open book registry '%s'\n", path);
        return -1;
    }

    while (fgets(line, sizeof line, fp)) {
        char  copy[512];
        char *f[3];
        int   n;

        snprintf(copy, sizeof copy, "%s", line);
        trim(copy);
        if (copy[0] == '\0' || copy[0] == '#') continue;   /* skip blanks/comments */

        n = split_csv(copy, f, 3);
        if (n != 3) {
            fprintf(stderr, "WARNING: malformed book line skipped: \"%s\"\n", copy);
            continue;
        }
        for (int i = 0; i < 3; i++) trim(f[i]);

        if (g_book_count >= MAX_BOOKS) {
            fprintf(stderr, "WARNING: book registry full (%d max)\n", MAX_BOOKS);
            break;
        }

        Book *b = &g_books[g_book_count];
        memset(b, 0, sizeof *b);
        snprintf(b->book_id, sizeof b->book_id, "%s", f[0]);
        snprintf(b->title,   sizeof b->title,   "%s", f[1]);
        snprintf(b->author,  sizeof b->author,  "%s", f[2]);
        g_book_count++;
    }
    fclose(fp);

    if (g_book_count == 0) {
        fprintf(stderr, "ERROR: book registry '%s' is empty\n", path);
        return -1;
    }
    printf("[registry] loaded %d book(s)   from %s\n", g_book_count, path);
    return 0;
}

int registry_load_members(const char *path)
{
    FILE *fp;
    char  line[512];

    g_member_count = 0;
    fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "ERROR: cannot open member registry '%s'\n", path);
        return -1;
    }

    while (fgets(line, sizeof line, fp)) {
        char  copy[512];
        char *f[3];
        int   n;

        snprintf(copy, sizeof copy, "%s", line);
        trim(copy);
        if (copy[0] == '\0' || copy[0] == '#') continue;

        n = split_csv(copy, f, 3);
        if (n != 3) {
            fprintf(stderr, "WARNING: malformed member line skipped: \"%s\"\n", copy);
            continue;
        }
        for (int i = 0; i < 3; i++) trim(f[i]);

        if (g_member_count >= MAX_MEMBERS) {
            fprintf(stderr, "WARNING: member registry full (%d max)\n", MAX_MEMBERS);
            break;
        }

        Member *m = &g_members[g_member_count];
        memset(m, 0, sizeof *m);
        snprintf(m->member_id,   sizeof m->member_id,   "%s", f[0]);
        snprintf(m->full_name,   sizeof m->full_name,   "%s", f[1]);
        snprintf(m->course_code, sizeof m->course_code, "%s", f[2]);
        g_member_count++;
    }
    fclose(fp);

    if (g_member_count == 0) {
        fprintf(stderr, "ERROR: member registry '%s' is empty\n", path);
        return -1;
    }
    printf("[registry] loaded %d member(s) from %s\n", g_member_count, path);
    return 0;
}

/* ------------------------------------------------------------ */
const Book *registry_find_book(const char *book_id)
{
    for (int i = 0; i < g_book_count; i++)
        if (strcmp(g_books[i].book_id, book_id) == 0) return &g_books[i];
    return NULL;
}

const Book *registry_book_at(int index)
{
    if (index < 0 || index >= g_book_count) return NULL;
    return &g_books[index];
}

const Member *registry_find_member(const char *member_id)
{
    for (int i = 0; i < g_member_count; i++)
        if (strcmp(g_members[i].member_id, member_id) == 0) return &g_members[i];
    return NULL;
}

const Member *registry_member_at(int index)
{
    if (index < 0 || index >= g_member_count) return NULL;
    return &g_members[index];
}

int registry_book_count(void)   { return g_book_count;   }
int registry_member_count(void) { return g_member_count; }

/* ------------------------------------------------------------ */
void registry_print_books(void)
{
    printf("\n--- BOOK REGISTRY (%d) ---\n", g_book_count);
    printf("%-8s %-40s %s\n", "ID", "TITLE", "AUTHOR");
    for (int i = 0; i < g_book_count; i++)
        printf("%-8s %-40s %s\n",
               g_books[i].book_id, g_books[i].title, g_books[i].author);
}

void registry_print_members(void)
{
    printf("\n--- MEMBER REGISTRY (%d) ---\n", g_member_count);
    printf("%-8s %-28s %s\n", "ID", "FULL NAME", "COURSE");
    for (int i = 0; i < g_member_count; i++)
        printf("%-8s %-28s %s\n",
               g_members[i].member_id, g_members[i].full_name,
               g_members[i].course_code);
}