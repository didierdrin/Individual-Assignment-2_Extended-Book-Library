/* ============================================================
 *  File: include/blockchain.h
 * ============================================================ */
#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <time.h>
#include "crypto_utils.h"

#define HASH_HEX_LEN   65
#define BLOCK_SIG_LEN  72
#define MAX_PENDING    256

/* 64 zeros - the previous_hash of the genesis block. */
#define ZERO_HASH \
    "0000000000000000000000000000000000000000000000000000000000000000"

/* ------------------------------------------------------------
 *  One block = one lending event (or the genesis block).
 * ------------------------------------------------------------ */
typedef struct {
    int           index;                        /* 0 = genesis            */
    time_t        timestamp;                    /* creation time          */
    char          book_id[20];                  /* FK -> book registry    */
    char          book_title[80];               /* snapshot at txn time   */
    char          member_id[20];                /* FK -> member registry  */
    char          member_name[50];              /* snapshot at txn time   */
    char          action[10];                   /* BORROWED/RETURNED/...  */
    int           token_reward;                 /* reward earned          */
    unsigned long nonce;                         /* mining nonce          */
    char          transaction_id[65];            /* SHA-256 of reward tx  */
    char          previous_hash[HASH_HEX_LEN];  /* hash of block i-1      */
    unsigned char signature[BLOCK_SIG_LEN];     /* ECDSA (DER) signature  */
    char          hash[HASH_HEX_LEN];           /* SHA-256 of all above   */
} Block;

typedef struct {
    Block *items;
    int    count;
    int    capacity;
} PendingPool;

typedef struct {
    Block *blocks;
    int    count;
    int    capacity;
} Chain;

/* --- lifecycle --------------------------------------------- */
void chain_init_struct(Chain *c);
int  chain_load(Chain *c, const char *path);   /*  0 ok, 1 no file, -1 bad */
int  chain_save(const Chain *c, const char *path);
int  chain_genesis(Chain *c);
void chain_free(Chain *c);

/* --- pending pool ------------------------------------------ */
void pending_pool_init(PendingPool *p);
void pending_pool_free(PendingPool *p);
int  pending_pool_add(PendingPool *p, const Block *block);
void pending_pool_print(const PendingPool *p);

/* --- mutations --------------------------------------------- */
int  chain_add(Chain *c, const char *book_id,   const char *book_title,
                          const char *member_id, const char *member_name,
                          const char *action);
int  chain_add_rewarded(Chain *c, const char *book_id, const char *book_title,
                       const char *member_id, const char *member_name,
                       const char *action, int reward, const char *tx_id,
                       int difficulty, int *hash_attempts);
int  chain_mine_pending(Chain *c, PendingPool *pending, int difficulty,
                        int *mined_count, int *total_hash_attempts);

/* --- queries ----------------------------------------------- */
int  chain_validate(const Chain *c, int *first_bad_index); /* 1 = valid */
int  chain_book_on_loan(const Chain *c, const char *book_id);
const Block *chain_last_for_book(const Chain *c, const char *book_id);
void chain_print(const Chain *c);
void chain_tamper_demo(const Chain *c);

#endif /* BLOCKCHAIN_H */