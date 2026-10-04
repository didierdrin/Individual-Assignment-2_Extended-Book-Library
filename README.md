# Assignment 2: Extending the Blockchain-Based Library Book Lending Tracker

This project extends the earlier blockchain-based library system so that book returns generate token rewards, those rewards are confirmed through mining before they are credited, and both a UTXO model and an account-based model are supported for member balances.

---

## 1. Project Goal

The library system tracks lending activity as blockchain records. In this extension:

- a book return triggers a token reward transaction;
- the reward is not immediately accepted into the chain;
- the transaction is placed in a pending pool;
- a mining process confirms the block before the member balance is updated;
- the project demonstrates both UTXO and account-balance transaction logic.

This assignment shows how blockchain transactions, pending validation, and mining work together in a real ledger-based system.

---

## 2. Updated Functional Requirements

### Lending events and reward rules

- Return on time: +10 coins to the member
- Late return: +5 coins to the member
- No return / still overdue: no token reward
- A lending record remains pending until mined
- Only confirmed blocks update the member balance

### Updated block fields

Each block should include:

- token reward value
- transaction ID (SHA-256 hash of the reward transaction)
- all original fields from the first assignment

### Transaction models

The project must support two methods for managing token balances:

1. UTXO model
   - transactions contain inputs and outputs;
   - member balances are derived from unspent outputs;
   - double spending is prevented;
   - transaction fees and change outputs are handled.

2. Account-based model
   - each member has a token balance and a nonce;
   - outgoing transactions debit the sender and credit the recipient;
   - invalid or reused nonces are rejected;
   - transaction history is stored in a linked list.

### Mining simulation

The project must simulate three mining methods:

- Solo mining
- Pool mining
- Cloud mining

All mining operates on the same pending pool of lending blocks.

---

## 3. Dependencies

The project requires:

- GCC or Clang with C11 support
- OpenSSL development libraries for SHA-256 and cryptographic support
- Standard POSIX / Linux environment

Install dependencies:

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install build-essential libssl-dev

# Fedora / RHEL
sudo dnf install gcc openssl-devel

# macOS with Homebrew
brew install openssl
export CFLAGS="-I$(brew --prefix openssl)/include"
export LDFLAGS="-L$(brew --prefix openssl)/lib"
```

---

## 4. Build and Run

Compile the project:

```bash
gcc -std=c11 -Wall -Wextra -Iinclude src/*.c -o library_tracker -lssl -lcrypto
```

Run the application:

```bash
./library_tracker
```

To start from a clean state:

```bash
rm -f chain.dat
./library_tracker
```

---

## 5. Switching Between Transaction Models

The program should allow the user to select a model from a menu or via a compile-time setting.

### Option A: compile-time selection

```c
#define TX_MODEL_UTXO 1
// or
#define TX_MODEL_ACCOUNT 1
```

### Option B: menu selection

The menu should include:

- 1. Use UTXO model
- 2. Use account-based model
- 3. Start mining simulation

The chosen model applies to all token transactions during the session.

---

## 6. Setting Mining Difficulty

Mining difficulty is configured using a variable or command-line argument. The default is usually 2 leading zeroes.

Example:

```bash
./library_tracker --difficulty 2
```

The difficulty range is expected to be from 1 to 4. A higher value means more hash attempts are required before a valid block hash is found.

---

## 7. Testing and Demo Workflow

### Typical test flow

1. Load the library registry.
2. Borrow a book.
3. Return the book.
4. Check pending pool status.
5. Mine the pending blocks.
6. View updated balances and confirmed records.
7. Run the UTXO and account-based model tests.
8. Run solo, pool, and cloud mining simulations.

### Example scenarios to test

- On-time return creates a reward of 10 coins
- Late return creates a reward of 5 coins
- Overdue book with no return produces no reward
- Insufficient member balance is rejected
- Invalid nonce is rejected in account-based model
- Cloud mining warns if fees exceed rewards

---

## 8. Edge-Case Validation Checklist

The system should be tested for:

- absent student generates no transaction;
- insufficient balance stops a transfer;
- incorrect nonce fails validation;
- unprofitable cloud rental displays a warning;
- pending blocks remain unconfirmed until mined;
- reward generation only occurs after a valid return;
- mining difficulty can be increased and decreased without breaking the logic.

---

## 9. Project Files

```text
Assignment-2/
├── README.md
├── report.md
├── voiceover_script.md
├── include/
│   ├── blockchain.h
│   ├── crypto_utils.h
│   └── registry.h
├── src/
│   ├── blockchain.c
│   ├── crypto_utils.c
│   ├── main.c
│   └── registry.c
├── members.txt
├── books.txt
└── chain.dat
```

---

## 10. Additional Notes

This assignment extends the earlier library blockchain tracker by adding the missing blockchain economics and confirmation layer. It demonstrates how token rewards can be tied to library events, how mining acts as a trust and confirmation step, and how different transaction models affect ledger management.

The final submission should include:

- updated source code;
- a technical report in PDF/Word format;
- a complete README file;
- a demo video showing the full workflow.

---

## 11. Submission Checklist

Before submission, confirm:

- [ ] rewards are created on return events;
- [ ] pending blocks are mined before confirmation;
- [ ] UTXO model works correctly;
- [ ] account-based model works correctly;
- [ ] mining difficulty is configurable;
- [ ] solo, pool, and cloud mining are implemented;
- [ ] README is accurate and complete;
- [ ] technical report includes screenshots and test results;
- [ ] demo video is recorded and narrated clearly.
