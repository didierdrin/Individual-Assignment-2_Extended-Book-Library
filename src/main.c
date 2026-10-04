/* ============================================================
 *  File: src/main.c
 *  Extended Library Blockchain Tracker - CLI
 * ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crypto_utils.h"
#include "registry.h"
#include "blockchain.h"

#define BOOKS_FILE   "books.txt"
#define MEMBERS_FILE "members.txt"
#define CHAIN_FILE   "chain.dat"
#define KEY_DIR      "keys"

#define MAX_ACCOUNTS 256
#define MAX_UTXOS    1024

typedef enum {
    MODEL_UTXO = 1,
    MODEL_ACCOUNT = 2
} TxModel;

typedef struct {
    char member_id[20];
    int  balance;
    unsigned long nonce;
    int  tx_count;
} AccountEntry;

typedef struct {
    char member_id[20];
    int  amount;
    int  spent;
} UtxoEntry;

typedef struct TxLog {
    char sender[20];
    char recipient[20];
    int  amount;
    int  fee;
    unsigned long nonce;
    struct TxLog *next;
} TxLog;

static Chain g_chain;
static PendingPool g_pending;
static TxModel g_model = MODEL_UTXO;
static int g_difficulty = 2;
static AccountEntry g_accounts[MAX_ACCOUNTS];
static int g_account_count = 0;
static UtxoEntry g_utxos[MAX_UTXOS];
static int g_utxo_count = 0;
static TxLog *g_history[MAX_ACCOUNTS];

static int read_line(const char *prompt, char *buf, size_t n)
{
    size_t len;
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, (int)n, stdin)) { buf[0] = '\0'; return 0; }

    len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';
    return 1;
}

static int init_chain(void)
{
    int r;
    chain_init_struct(&g_chain);
    pending_pool_init(&g_pending);
    r = chain_load(&g_chain, CHAIN_FILE);

    if (r == 1) {
        if (chain_genesis(&g_chain) != 0) {
            fprintf(stderr, "FATAL: could not create genesis block\n");
            return -1;
        }
        if (chain_save(&g_chain, CHAIN_FILE) != 0) {
            fprintf(stderr, "FATAL: could not persist new chain\n");
            return -1;
        }
        printf("[chain]    created new chain with genesis block -> %s\n", CHAIN_FILE);
    } else if (r != 0) {
        fprintf(stderr, "FATAL: chain file '%s' could not be read\n", CHAIN_FILE);
        return -1;
    } else {
        printf("[chain]    loaded %d block(s) from %s\n", g_chain.count, CHAIN_FILE);
    }
    return 0;
}

static void persist_or_warn(void)
{
    if (chain_save(&g_chain, CHAIN_FILE) != 0)
        printf("WARNING: chain could not be written to disk.\n");
}

static void add_account_if_missing(const char *member_id);

static int equals_ignore_case(const char *a, const char *b)
{
    unsigned char ca, cb;
    while (*a && *b) {
        ca = (unsigned char)*a; cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == *b;
}

static void init_accounts(void)
{
    int i;
    for (i = 0; i < MAX_ACCOUNTS; i++) {
        g_accounts[i].member_id[0] = '\0';
        g_accounts[i].balance = 0;
        g_accounts[i].nonce = 0;
        g_accounts[i].tx_count = 0;
        g_history[i] = NULL;
    }
    g_account_count = 0;
    for (i = 0; i < MAX_UTXOS; i++) {
        g_utxos[i].member_id[0] = '\0';
        g_utxos[i].amount = 0;
        g_utxos[i].spent = 0;
    }
    g_utxo_count = 0;

    for (int j = 0; j < registry_member_count(); j++) {
        const Member *m = registry_member_at(j);
        if (m) add_account_if_missing(m->member_id);
    }
}

static AccountEntry *find_account(const char *id)
{
    for (int i = 0; i < g_account_count; i++) {
        if (strcmp(g_accounts[i].member_id, id) == 0)
            return &g_accounts[i];
    }
    return NULL;
}

static void add_account_if_missing(const char *member_id)
{
    if (find_account(member_id) != NULL) return;
    if (g_account_count >= MAX_ACCOUNTS) return;
    snprintf(g_accounts[g_account_count].member_id, sizeof(g_accounts[g_account_count].member_id), "%s", member_id);
    g_accounts[g_account_count].balance = 0;
    g_accounts[g_account_count].nonce = 0;
    g_accounts[g_account_count].tx_count = 0;
    g_history[g_account_count] = NULL;
    g_account_count++;
}

static int utxo_balance_for_member(const char *member_id)
{
    int total = 0;
    for (int i = 0; i < g_utxo_count; i++) {
        if (strcmp(g_utxos[i].member_id, member_id) == 0 && !g_utxos[i].spent)
            total += g_utxos[i].amount;
    }
    return total;
}

static void utxo_add_reward(const char *member_id, int amount)
{
    if (g_utxo_count >= MAX_UTXOS) {
        printf("WARNING: UTXO pool is full.\n");
        return;
    }
    snprintf(g_utxos[g_utxo_count].member_id, sizeof(g_utxos[g_utxo_count].member_id), "%s", member_id);
    g_utxos[g_utxo_count].amount = amount;
    g_utxos[g_utxo_count].spent = 0;
    g_utxo_count++;
}

static void account_credit(const char *member_id, int amount)
{
    AccountEntry *acc = find_account(member_id);
    if (!acc) {
        add_account_if_missing(member_id);
        acc = find_account(member_id);
    }
    if (!acc) return;
    acc->balance += amount;
}

static int account_debit(const char *member_id, int amount)
{
    AccountEntry *acc = find_account(member_id);
    if (!acc) return -1;
    if (acc->balance < amount) return -1;
    acc->balance -= amount;
    return 0;
}

static void record_history(const char *sender, const char *recipient, int amount, int fee, unsigned long nonce)
{
    for (int i = 0; i < g_account_count; i++) {
        if (strcmp(g_accounts[i].member_id, sender) == 0 || strcmp(g_accounts[i].member_id, recipient) == 0) {
            TxLog *node = malloc(sizeof(TxLog));
            if (!node) return;
            memset(node, 0, sizeof(TxLog));
            snprintf(node->sender, sizeof(node->sender), "%s", sender);
            snprintf(node->recipient, sizeof(node->recipient), "%s", recipient);
            node->amount = amount;
            node->fee = fee;
            node->nonce = nonce;
            node->next = g_history[i];
            g_history[i] = node;
        }
    }
}

static void print_member_history(const char *member_id)
{
    TxLog *cur = NULL;
    int found = 0;
    for (int i = 0; i < g_account_count; i++) {
        if (strcmp(g_accounts[i].member_id, member_id) != 0) continue;
        cur = g_history[i];
        break;
    }
    printf("\n--- TRANSACTION HISTORY FOR %s ---\n", member_id);
    if (!cur) {
        printf("No transactions recorded for this member.\n");
        return;
    }
    while (cur) {
        printf("sender=%s recipient=%s amount=%d fee=%d nonce=%lu\n",
               cur->sender, cur->recipient, cur->amount, cur->fee, cur->nonce);
        cur = cur->next;
        found = 1;
    }
    if (!found) printf("No transactions recorded for this member.\n");
}

static void print_balances(void)
{
    int total = 0;
    if (g_model == MODEL_UTXO) {
        printf("\n=== UTXO BALANCES ===\n");
        for (int i = 0; i < registry_member_count(); i++) {
            const Member *m = registry_member_at(i);
            if (!m) continue;
            printf("  %s -> %d\n", m->member_id, utxo_balance_for_member(m->member_id));
        }
        printf("UTXO set:\n");
        if (g_utxo_count == 0) {
            printf("No unspent outputs yet.\n");
            return;
        }
        for (int i = 0; i < g_utxo_count; i++) {
            if (!g_utxos[i].spent) {
                printf("  %s -> %d\n", g_utxos[i].member_id, g_utxos[i].amount);
                total += g_utxos[i].amount;
            }
        }
        printf("Total unspent output value: %d\n", total);
        return;
    }

    printf("\n=== ACCOUNT BALANCES ===\n");
    for (int i = 0; i < g_account_count; i++) {
        printf("  %s -> balance=%d, nonce=%lu\n",
               g_accounts[i].member_id, g_accounts[i].balance, g_accounts[i].nonce);
    }
}

static void update_model_balance(const char *member_id, int reward)
{
    if (g_model == MODEL_UTXO) {
        utxo_add_reward(member_id, reward);
    } else {
        account_credit(member_id, reward);
    }
}

static int member_balance(const char *member_id)
{
    if (g_model == MODEL_UTXO) return utxo_balance_for_member(member_id);
    AccountEntry *acc = find_account(member_id);
    return acc ? acc->balance : 0;
}

static void do_borrow(void)
{
    char bid[64], mid[64];
    const Book *b; const Member *m;

    printf("\n--- BORROW BOOK ---\n");
    if (!read_line("Book ID   : ", bid, sizeof bid)) return;
    if (!read_line("Member ID : ", mid, sizeof mid)) return;

    b = registry_find_book(bid);
    m = registry_find_member(mid);
    if (!b || !m) { printf("ERROR: Book or Member not found\n"); return; }
    if (chain_book_on_loan(&g_chain, b->book_id)) { printf("ERROR: \"%s\" (%s) is already on loan.\n", b->title, b->book_id); return; }

    if (chain_add(&g_chain, b->book_id, b->title, m->member_id, m->full_name, "BORROWED") != 0) {
        printf("ERROR: could not record the lending event.\n");
        return;
    }
    persist_or_warn();
    printf("\nSUCCESS: %s (%s) borrowed \"%s\".\n", m->full_name, m->member_id, b->title);
}

static void do_return(void)
{
    char bid[64], mid[64], answer[16];
    const Book *b; const Member *m; const Block *last;
    Block reward_block;
    char tx_string[256];
    char tx_hash[SHA256_HEX_LEN];
    int reward = 0;

    printf("\n--- RETURN BOOK ---\n");
    if (!read_line("Book ID   : ", bid, sizeof bid)) return;
    if (!read_line("Member ID : ", mid, sizeof mid)) return;
    if (!read_line("On time? [y/N]: ", answer, sizeof answer)) return;

    b = registry_find_book(bid); m = registry_find_member(mid);
    if (!b || !m) { printf("ERROR: Book or Member not found\n"); return; }

    last = chain_last_for_book(&g_chain, b->book_id);
    if (!last || (strcmp(last->action, "BORROWED") != 0 && strcmp(last->action, "OVERDUE") != 0)) {
        printf("ERROR: \"%s\" (%s) is not currently on loan.\n", b->title, b->book_id);
        return;
    }

    reward = (equals_ignore_case(answer, "y") || equals_ignore_case(answer, "yes")) ? 10 : 5;
    snprintf(tx_string, sizeof tx_string, "%s|%s|%d|%ld", m->member_id, b->book_id, reward, (long)time(NULL));
    sha256_hex(tx_string, strlen(tx_string), tx_hash);

    memset(&reward_block, 0, sizeof reward_block);
    reward_block.index = 0;
    reward_block.timestamp = time(NULL);
    snprintf(reward_block.book_id, sizeof reward_block.book_id, "%s", b->book_id);
    snprintf(reward_block.book_title, sizeof reward_block.book_title, "%s", b->title);
    snprintf(reward_block.member_id, sizeof reward_block.member_id, "%s", m->member_id);
    snprintf(reward_block.member_name, sizeof reward_block.member_name, "%s", m->full_name);
    snprintf(reward_block.action, sizeof reward_block.action, "RETURNED");
    reward_block.token_reward = reward;
    reward_block.nonce = 0;
    snprintf(reward_block.transaction_id, sizeof reward_block.transaction_id, "%s", tx_hash);
    snprintf(reward_block.previous_hash, sizeof reward_block.previous_hash, "%s", g_chain.blocks[g_chain.count - 1].hash);

    if (pending_pool_add(&g_pending, &reward_block) != 0) {
        printf("ERROR: could not add return reward to pending pool.\n");
        return;
    }

    printf("\nSUCCESS: \"%s\" returned by %s.\n", b->title, m->full_name);
    printf("Reward queued in pending pool: %d tokens (tx=%s)\n", reward, tx_hash);
    pending_pool_print(&g_pending);
}

static void do_overdue(void)
{
    char bid[64], mid[64]; const Book *b; const Member *m; const Block *last;
    printf("\n--- MARK BOOK OVERDUE ---\n");
    if (!read_line("Book ID   : ", bid, sizeof bid)) return;
    if (!read_line("Member ID : ", mid, sizeof mid)) return;
    b = registry_find_book(bid); m = registry_find_member(mid);
    if (!b || !m) { printf("ERROR: Book or Member not found\n"); return; }
    last = chain_last_for_book(&g_chain, b->book_id);
    if (!last || strcmp(last->action, "BORROWED") != 0) {
        printf("ERROR: \"%s\" is not currently on loan.\n", b->title);
        return;
    }
    if (chain_add(&g_chain, b->book_id, b->title, m->member_id, m->full_name, "OVERDUE") != 0) {
        printf("ERROR: could not record the overdue event.\n");
        return;
    }
    persist_or_warn();
    printf("\nSUCCESS: \"%s\" flagged OVERDUE.\n", b->title);
}

static void do_validate(void)
{
    int bad = -1;
    printf("\n--- CHAIN VALIDATION ---\n");
    if (chain_validate(&g_chain, &bad)) {
        printf("CHAIN VALID: all %d block(s) verified.\n", g_chain.count);
    } else {
        printf("CHAIN INVALID: tampering detected at block #%d\n", bad);
    }
}

static void do_audit_book(void)
{
    char bid[64]; const Book *b;
    if (!read_line("\nBook ID: ", bid, sizeof bid)) return;
    b = registry_find_book(bid);
    if (!b) { printf("ERROR: Book or Member not found\n"); return; }
    printf("\nHistory for \"%s\" (%s):\n", b->title, b->book_id);
    int found = 0;
    for (int i = 0; i < g_chain.count; i++) {
        const Block *blk = &g_chain.blocks[i];
        char tbuf[64]; struct tm *tm_info;
        if (strcmp(blk->book_id, b->book_id) != 0) continue;
        tm_info = localtime(&blk->timestamp); strftime(tbuf, sizeof tbuf, "%Y-%m-%d %H:%M:%S", tm_info);
        printf("  #%-3d %-9s %-12s %s\n", blk->index, blk->action, blk->member_id, tbuf);
        found = 1;
    }
    if (!found) printf("  (no lending events recorded)\n");
    printf("  Current status: %s\n", chain_book_on_loan(&g_chain, b->book_id) ? "ON LOAN" : "AVAILABLE");
}

static void do_mine_pending(void)
{
    int mined = 0, attempts = 0;
    printf("\n--- MINE PENDING POOL ---\n");
    if (g_pending.count == 0) {
        printf("No pending rewards to mine.\n");
        return;
    }

    for (int i = 0; i < g_pending.count; i++) {
        const Block *b = &g_pending.items[i];
        if (g_model == MODEL_UTXO) {
            utxo_add_reward(b->member_id, b->token_reward);
        } else {
            AccountEntry *acc = find_account(b->member_id);
            if (!acc) {
                add_account_if_missing(b->member_id);
                acc = find_account(b->member_id);
            }
            if (acc) {
                acc->balance += b->token_reward;
                acc->nonce++;
            }
        }
        record_history("SYSTEM", b->member_id, b->token_reward, 1, g_model == MODEL_ACCOUNT ? 0 : 0);
    }

    mined = chain_mine_pending(&g_chain, &g_pending, g_difficulty, &mined, &attempts);
    printf("Mining complete. Confirmed %d block(s). Total attempts: %d\n", mined, attempts);
    printf("Reward confirmations are being applied to member balances.\n");
    print_balances();
}

static void do_switch_model(void)
{
    char sel[16];
    printf("\n--- SWITCH TRANSACTION MODEL ---\n");
    printf("1. UTXO model\n2. Account-based model\n");
    if (!read_line("Choice > ", sel, sizeof sel)) return;
    if (strcmp(sel, "1") == 0) g_model = MODEL_UTXO;
    else if (strcmp(sel, "2") == 0) g_model = MODEL_ACCOUNT;
    else printf("Invalid selection.\n");
    printf("Current model: %s\n", g_model == MODEL_UTXO ? "UTXO" : "Account-based");
}

static void do_set_difficulty(void)
{
    char sel[16];
    printf("\n--- SET MINING DIFFICULTY ---\n");
    if (!read_line("Difficulty (1-4): ", sel, sizeof sel)) return;
    g_difficulty = atoi(sel);
    if (g_difficulty < 1 || g_difficulty > 4) {
        g_difficulty = 2;
        printf("Difficulty out of range; reset to default 2.\n");
    }
    printf("Mining difficulty is now %d.\n", g_difficulty);
}

static void do_member_history(void)
{
    char mid[64];
    if (!read_line("\nMember ID: ", mid, sizeof mid)) return;
    if (registry_find_member(mid) == NULL) {
        printf("ERROR: Member not found.\n");
        return;
    }
    print_member_history(mid);
}

static void print_menu(void)
{
    printf("\n==================================================\n");
    printf("  EXTENDED LIBRARY BLOCKCHAIN TRACKER\n");
    printf("==================================================\n");
    printf("  1. Borrow a book\n");
    printf("  2. Return a book (creates pending reward)\n");
    printf("  3. View lending records (full ledger)\n");
    printf("  4. Validate chain integrity\n");
    printf("  5. Tamper-detection demonstration\n");
    printf("  6. Show book registry\n");
    printf("  7. Show member registry\n");
    printf("  8. Audit a single book's history\n");
    printf("  9. Mark a book overdue\n");
    printf(" 10. Mine pending pool\n");
    printf(" 11. Switch transaction model\n");
    printf(" 12. Set mining difficulty\n");
    printf(" 13. Show balances\n");
    printf(" 14. Show member transaction history\n");
    printf("  0. Exit\n");
    printf("--------------------------------------------------\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("\n##################################################\n");
    printf("#  Blockchain-Based Library Book Lending Tracker #\n");
    printf("#  Proof-of-work + token rewards + mining       #\n");
    printf("##################################################\n\n");

    if (crypto_init(KEY_DIR) != 0) {
        fprintf(stderr, "FATAL: could not initialise cryptography.\n");
        return 1;
    }

    if (registry_load_books(BOOKS_FILE) != 0) {
        fprintf(stderr, "FATAL: book registry unavailable.\n");
        crypto_cleanup();
        return 1;
    }
    if (registry_load_members(MEMBERS_FILE) != 0) {
        fprintf(stderr, "FATAL: member registry unavailable.\n");
        crypto_cleanup();
        return 1;
    }

    if (init_chain() != 0) {
        crypto_cleanup();
        return 1;
    }

    init_accounts();

    for (;;) {
        char line[32]; int choice;
        print_menu();
        if (!read_line("Choice > ", line, sizeof line)) {
            printf("\nEnd of input - exiting.\n");
            break;
        }
        if (line[0] == '\0') continue;
        choice = atoi(line);

        switch (choice) {
            case 1: do_borrow(); break;
            case 2: do_return(); break;
            case 3: chain_print(&g_chain); break;
            case 4: do_validate(); break;
            case 5: chain_tamper_demo(&g_chain); break;
            case 6: registry_print_books(); break;
            case 7: registry_print_members(); break;
            case 8: do_audit_book(); break;
            case 9: do_overdue(); break;
            case 10: do_mine_pending(); break;
            case 11: do_switch_model(); break;
            case 12: do_set_difficulty(); break;
            case 13: print_balances(); break;
            case 14: do_member_history(); break;
            case 0:
                printf("\nSaving chain and shutting down...\n");
                persist_or_warn();
                pending_pool_free(&g_pending);
                chain_free(&g_chain);
                crypto_cleanup();
                printf("Goodbye.\n");
                return 0;
            default:
                printf("Invalid option. Please choose 0-14.\n");
        }
    }

    persist_or_warn();
    pending_pool_free(&g_pending);
    chain_free(&g_chain);
    crypto_cleanup();
    return 0;
}