/* ============================================================
 *  File: src/blockchain.c
 * ============================================================ */
#include "blockchain.h"
#include "registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 16
#define CHAIN_MAGIC      "LBCT1"
#define CHAIN_MAGIC_LEN  5

static int block_hash_matches_difficulty(const char *h, int difficulty)
{
    int i;
    if (difficulty < 0) return 0;
    for (i = 0; i < difficulty; i++) {
        if (h[i] != '0') return 0;
    }
    return 1;
}

static void block_payload(const Block *b, char *out, size_t outsz)
{
    snprintf(out, outsz,
             "%d|%ld|%s|%s|%s|%s|%s|%d|%lu|%s|%s",
             b->index, (long)b->timestamp,
             b->book_id, b->book_title,
             b->member_id, b->member_name,
             b->action, b->token_reward,
             b->nonce, b->transaction_id,
             b->previous_hash);
}

static void block_compute_hash(const Block *b, char out[HASH_HEX_LEN])
{
    char payload[1024];
    char sig_hex[2 * BLOCK_SIG_LEN + 1];
    char full[1400];
    size_t slen;

    block_payload(b, payload, sizeof payload);
    slen = der_sig_len(b->signature);
    to_hex(b->signature, slen, sig_hex);

    snprintf(full, sizeof full, "%s|%s", payload, sig_hex);
    sha256_hex(full, strlen(full), out);
}

static int block_seal(Block *b, int difficulty, int *hash_attempts)
{
    char payload[1024];
    unsigned int sig_len = 0;
    int attempts = 0;

    if (difficulty < 0) difficulty = 0;

    do {
        b->nonce++;
        block_payload(b, payload, sizeof payload);
        if (ecdsa_sign((unsigned char *)payload, strlen(payload),
                       b->signature, &sig_len) != 0)
            return -1;
        block_compute_hash(b, b->hash);
        attempts++;
    } while (!block_hash_matches_difficulty(b->hash, difficulty));

    if (hash_attempts) *hash_attempts = attempts;
    return 0;
}

/* ============================================================
 *  Chain lifecycle
 * ============================================================ */
void chain_init_struct(Chain *c)
{
    c->blocks   = NULL;
    c->count    = 0;
    c->capacity = 0;
}

static int chain_reserve(Chain *c, int need)
{
    if (need <= c->capacity) return 0;

    int    newcap = c->capacity ? c->capacity : INITIAL_CAPACITY;
    while (newcap < need) newcap *= 2;

    Block *tmp = realloc(c->blocks, sizeof(Block) * (size_t)newcap);
    if (!tmp) { fprintf(stderr, "ERROR: out of memory\n"); return -1; }

    c->blocks   = tmp;
    c->capacity = newcap;
    return 0;
}

int chain_genesis(Chain *c)
{
    Block g;

    if (chain_reserve(c, 1) != 0) return -1;

    memset(&g, 0, sizeof g);
    g.index     = 0;
    g.timestamp = time(NULL);
    g.token_reward = 0;
    g.nonce = 0;

    snprintf(g.book_id,     sizeof g.book_id,     "GENESIS");
    snprintf(g.book_title,  sizeof g.book_title,  "Genesis Block");
    snprintf(g.member_id,   sizeof g.member_id,   "SYSTEM");
    snprintf(g.member_name, sizeof g.member_name, "Library System");
    snprintf(g.action,      sizeof g.action,      "GENESIS");
    snprintf(g.transaction_id, sizeof g.transaction_id, "GENESIS");
    snprintf(g.previous_hash, sizeof g.previous_hash, "%s", ZERO_HASH);

    if (block_seal(&g, 0, NULL) != 0) return -1;

    c->blocks[c->count++] = g;
    return 0;
}

int chain_add(Chain *c, const char *book_id,   const char *book_title,
                          const char *member_id, const char *member_name,
                          const char *action)
{
    return chain_add_rewarded(c, book_id, book_title, member_id, member_name,
                             action, 0, "NONE", 0, NULL);
}

int chain_add_rewarded(Chain *c, const char *book_id, const char *book_title,
                       const char *member_id, const char *member_name,
                       const char *action, int reward, const char *tx_id,
                       int difficulty, int *hash_attempts)
{
    Block b;

    if (c->count == 0) {
        fprintf(stderr, "ERROR: chain has no genesis block\n");
        return -1;
    }
    if (chain_reserve(c, c->count + 1) != 0) return -1;

    memset(&b, 0, sizeof b);
    b.index     = c->count;
    b.timestamp = time(NULL);
    b.token_reward = reward;
    b.nonce = 0;

    snprintf(b.book_id,     sizeof b.book_id,     "%s", book_id);
    snprintf(b.book_title,  sizeof b.book_title,  "%s", book_title);
    snprintf(b.member_id,   sizeof b.member_id,   "%s", member_id);
    snprintf(b.member_name, sizeof b.member_name, "%s", member_name);
    snprintf(b.action,      sizeof b.action,      "%s", action);
    snprintf(b.transaction_id, sizeof b.transaction_id, "%s", tx_id && tx_id[0] ? tx_id : "NONE");
    snprintf(b.previous_hash, sizeof b.previous_hash, "%s",
             c->blocks[c->count - 1].hash);

    if (block_seal(&b, difficulty, hash_attempts) != 0) {
        fprintf(stderr, "ERROR: could not sign or mine block\n");
        return -1;
    }

    c->blocks[c->count++] = b;
    return 0;
}

void chain_free(Chain *c)
{
    free(c->blocks);
    c->blocks   = NULL;
    c->count    = 0;
    c->capacity = 0;
}

void pending_pool_init(PendingPool *p)
{
    p->items = NULL;
    p->count = 0;
    p->capacity = 0;
}

void pending_pool_free(PendingPool *p)
{
    free(p->items);
    p->items = NULL;
    p->count = 0;
    p->capacity = 0;
}

int pending_pool_add(PendingPool *p, const Block *block)
{
    Block *tmp;
    if (p->count >= MAX_PENDING) {
        fprintf(stderr, "ERROR: pending pool is full\n");
        return -1;
    }
    if (p->count >= p->capacity) {
        int newcap = p->capacity ? p->capacity * 2 : 8;
        tmp = realloc(p->items, sizeof(Block) * (size_t)newcap);
        if (!tmp) return -1;
        p->items = tmp;
        p->capacity = newcap;
    }
    p->items[p->count++] = *block;
    return 0;
}

void pending_pool_print(const PendingPool *p)
{
    printf("\n=== PENDING POOL ===\n");
    if (p->count == 0) {
        printf("No unconfirmed lending events waiting to be mined.\n");
        return;
    }
    for (int i = 0; i < p->count; i++) {
        const Block *b = &p->items[i];
        printf("[%d] %s | %s | %s | reward=%d | tx=%s\n",
               i + 1, b->member_name, b->book_title, b->action,
               b->token_reward, b->transaction_id);
    }
}

int chain_mine_pending(Chain *c, PendingPool *pending, int difficulty,
                        int *mined_count, int *total_hash_attempts)
{
    int total = 0;
    int mined = 0;

    if (mined_count) *mined_count = 0;
    if (total_hash_attempts) *total_hash_attempts = 0;

    if (!pending || pending->count == 0) return 0;

    for (int i = 0; i < pending->count; i++) {
        Block b = pending->items[i];
        int attempts = 0;

        b.index = c->count;
        b.timestamp = time(NULL);
        snprintf(b.previous_hash, sizeof b.previous_hash, "%s",
                 c->blocks[c->count - 1].hash);
        b.nonce = 0;

        if (chain_add_rewarded(c, b.book_id, b.book_title, b.member_id,
                               b.member_name, b.action, b.token_reward,
                               b.transaction_id, difficulty, &attempts) != 0) {
            continue;
        }
        mined++;
        total += attempts;
        printf("[mine] confirmed block #%d with %d hash attempts\n",
               c->blocks[c->count - 1].index, attempts);
    }

    pending->count = 0;
    if (mined_count) *mined_count = mined;
    if (total_hash_attempts) *total_hash_attempts = total;
    return mined;
}

/* ============================================================
 *  Persistence (binary snapshot)
 * ============================================================ */
int chain_save(const Chain *c, const char *path)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) { perror("chain_save"); return -1; }

    fwrite(CHAIN_MAGIC, 1, CHAIN_MAGIC_LEN, fp);
    fwrite(&c->count, sizeof(int), 1, fp);
    for (int i = 0; i < c->count; i++)
        fwrite(&c->blocks[i], sizeof(Block), 1, fp);

    fclose(fp);
    return 0;
}

int chain_load(Chain *c, const char *path)
{
    FILE *fp = fopen(path, "rb");
    char  magic[CHAIN_MAGIC_LEN];
    int   count;

    if (!fp) return 1;

    if (fread(magic, 1, CHAIN_MAGIC_LEN, fp) != CHAIN_MAGIC_LEN ||
        memcmp(magic, CHAIN_MAGIC, CHAIN_MAGIC_LEN) != 0) {
        fclose(fp);
        fprintf(stderr, "ERROR: '%s' is not a valid chain file\n", path);
        return -1;
    }
    if (fread(&count, sizeof(int), 1, fp) != 1 ||
        count < 0 || count > 1000000) {
        fclose(fp);
        fprintf(stderr, "ERROR: '%s' has an invalid block count\n", path);
        return -1;
    }
    if (chain_reserve(c, count ? count : 1) != 0) { fclose(fp); return -1; }

    for (int i = 0; i < count; i++) {
        if (fread(&c->blocks[i], sizeof(Block), 1, fp) != 1) {
            fclose(fp);
            fprintf(stderr, "ERROR: '%s' is truncated\n", path);
            return -1;
        }
    }
    c->count = count;
    fclose(fp);
    return 0;
}

/* ============================================================
 *  Validation
 * ============================================================ */
int chain_validate(const Chain *c, int *first_bad_index)
{
    int ok = 1;

    if (first_bad_index) *first_bad_index = -1;

    if (c->count == 0) {
        if (first_bad_index) *first_bad_index = 0;
        return 0;
    }

    for (int i = 0; i < c->count; i++) {
        const Block *b = &c->blocks[i];
        char payload[1024];
        char expect[HASH_HEX_LEN];

        if (i == 0) {
            if (strcmp(b->previous_hash, ZERO_HASH) != 0) {
                if (ok && first_bad_index) *first_bad_index = i;
                ok = 0;
            }
        } else {
            if (strcmp(b->previous_hash, c->blocks[i - 1].hash) != 0) {
                if (ok && first_bad_index) *first_bad_index = i;
                ok = 0;
            }
        }

        block_payload(b, payload, sizeof payload);
        if (!ecdsa_verify((unsigned char *)payload, strlen(payload),
                          b->signature)) {
            if (ok && first_bad_index) *first_bad_index = i;
            ok = 0;
        }

        block_compute_hash(b, expect);
        if (strcmp(expect, b->hash) != 0) {
            if (ok && first_bad_index) *first_bad_index = i;
            ok = 0;
        }
    }
    return ok;
}

/* ============================================================
 *  Queries
 * ============================================================ */
const Block *chain_last_for_book(const Chain *c, const char *book_id)
{
    for (int i = c->count - 1; i >= 0; i--)
        if (strcmp(c->blocks[i].book_id, book_id) == 0)
            return &c->blocks[i];
    return NULL;
}

int chain_book_on_loan(const Chain *c, const char *book_id)
{
    const Block *b = chain_last_for_book(c, book_id);
    if (!b) return 0;
    return strcmp(b->action, "BORROWED") == 0 ||
           strcmp(b->action, "OVERDUE")  == 0;
}

/* ============================================================
 *  Display
 * ============================================================ */
static void fmt_time(time_t t, char *out, size_t n)
{
    struct tm *tm_info = localtime(&t);
    if (!tm_info) { snprintf(out, n, "(invalid)"); return; }
    strftime(out, n, "%Y-%m-%d %H:%M:%S", tm_info);
}

void chain_print(const Chain *c)
{
    char tbuf[64];

    printf("\n================= LIBRARY LENDING LEDGER =================\n");
    printf("Blocks in chain: %d\n", c->count);

    for (int i = 0; i < c->count; i++) {
        const Block *b = &c->blocks[i];
        char payload[1024];
        int  sig_ok;

        fmt_time(b->timestamp, tbuf, sizeof tbuf);
        block_payload(b, payload, sizeof payload);
        sig_ok = ecdsa_verify((unsigned char *)payload, strlen(payload),
                              b->signature);

        printf("\n----------------------------------------------------------\n");
        printf("Block #%d   [%s]\n", b->index, b->action);
        printf("  Time          : %s\n", tbuf);
        printf("  Book          : %s - \"%s\"\n", b->book_id, b->book_title);
        printf("  Member        : %s - %s\n", b->member_id, b->member_name);
        printf("  Reward        : %d tokens\n", b->token_reward);
        printf("  Tx ID         : %s\n", b->transaction_id);
        printf("  Previous hash : %.16s...\n", b->previous_hash);
        printf("  Hash          : %.16s...\n", b->hash);
        printf("  Signature     : %s (DER, %zu bytes)\n",
               sig_ok ? "VALID" : "INVALID", der_sig_len(b->signature));
    }
    printf("\n----------------------------------------------------------\n");
}

void chain_tamper_demo(const Chain *c)
{
    Chain copy;
    int   target = 1;
    int   bad    = -1;
    char  before[HASH_HEX_LEN];
    char  payload[1024];

    printf("\n=============== TAMPER DETECTION DEMO ===============\n");

    if (c->count < 2) {
        printf("Need at least 2 blocks (genesis + 1 transaction).\n");
        printf("Borrow a book first, then run this demo again.\n");
        return;
    }

    chain_init_struct(&copy);
    copy.count    = c->count;
    copy.capacity = c->count;
    copy.blocks   = malloc(sizeof(Block) * (size_t)c->count);
    if (!copy.blocks) { printf("Out of memory.\n"); return; }
    memcpy(copy.blocks, c->blocks, sizeof(Block) * (size_t)c->count);

    printf("Simulating a dishonest librarian editing the ledger...\n\n");
    printf("BEFORE  block #%d : action=\"%s\"  book=\"%s\"\n",
           target, copy.blocks[target].action, copy.blocks[target].book_title);
    snprintf(before, sizeof before, "%s", copy.blocks[target].hash);

    snprintf(copy.blocks[target].action,
             sizeof copy.blocks[target].action, "RETURNED");

    printf("AFTER   block #%d : action=\"%s\"  book=\"%s\"\n\n",
           target, copy.blocks[target].action, copy.blocks[target].book_title);

    printf("Stored hash  (unchanged) : %.32s...\n", before);
    printf("Recomputed hash          : ");

    {
        char expect[HASH_HEX_LEN];
        char full[1400];
        char sig_hex[2 * BLOCK_SIG_LEN + 1];

        block_payload(&copy.blocks[target], payload, sizeof payload);
        to_hex(copy.blocks[target].signature,
               der_sig_len(copy.blocks[target].signature), sig_hex);
        snprintf(full, sizeof full, "%s|%s", payload, sig_hex);
        sha256_hex(full, strlen(full), expect);
        printf("%.32s...\n", expect);
    }

    if (chain_validate(&copy, &bad)) {
        printf("RESULT: UNEXPECTED - chain still reports VALID.\n");
    } else {
        printf("RESULT: CHAIN INVALID - tampering detected at block #%d\n", bad);
    }

    printf("\nThe live in-memory chain and '%s' were NOT modified.\n",
           "chain.dat");
    printf("=====================================================\n");

    chain_free(&copy);
}