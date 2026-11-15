# Movie Ticket Booking & Seat Management System

A **C++11 console application** using **SQLite** for data persistence and **OpenSSL** for salted password hashing. Designed for a Linux environment (including GitHub Codespaces).

## Roles

| Role | Access |
| --- | --- |
| **Platform Admin (one account)** | First-launch setup only; bulk add, update and deactivate movies; approve/disable theatre owners; manage customers; view platform bookings, payments/refunds and reports; cancel bookings |
| **Theatre Owner (multiple accounts)** | Sign up, await approval, add **their own** theatres/screens/shows, cancel or replace their cancelled shows, set prices, view only their theatres' bookings, payments and reports |
| **Customer (multiple accounts)** | Sign up; browse/search movies; view details, shows and seats; book seats; simulate payment; view or cancel **own** bookings; see history; update profile |

**How the one-admin rule works:** If no admin exists in the database, the program asks for administrator registration. After setup, public sign-up offers **Customer** and **Theatre Owner** only. The SQLite database also has a unique index preventing more than one `ADMIN` row. Admin credentials are not included in the code.

**Theatre owners do not need to contact the admin through a message system:** Their account appears in the admin's approval list. Once approved, they log in and manage only their theatres.

## Requirements

- Ubuntu/Linux with `g++`, `make`, `libsqlite3-dev`, `libssl-dev`
- Python 3 for running automated tests (optional)

In GitHub Codespaces (Ubuntu):

```bash
sudo apt update
sudo apt install -y g++ make sqlite3 libsqlite3-dev libssl-dev python3
```

## Compile and run

Open the terminal **inside this project folder**, where `Makefile` is located:

```bash
mkdir -p data
make
./movie_booking
```

Or:

```bash
./run.sh
```

`data/cinema.db` is created on first launch. Run `make test` to run the integration tests. Run `make clean` to remove only the compiled executable.

**If Codespaces shows `No rule to make target src/main.cpp`:** the folder hierarchy was flattened during upload. Preserve `src/`, `include/`, and `tests/` when extracting the ZIP. Don't move all code files into the repository root.

**If you see `unable to open database file`:** enter `mkdir -p data`, then run the app again.

## First-time demonstration workflow

1. Launch `./movie_booking`, create **one** Platform Admin (username/password of your choice).
2. Sign up one or more **Theatre Owner** accounts. They are pending until approved.
3. Sign up **Customer** accounts (no approval needed).
4. Log in as **Admin** and approve theatre owners using the Owner Management menu. Add a movie.
5. Log in as an approved **Theatre Owner**, add a theatre, add its screen, add a show for the movie, and set a price.
6. Log in as a **Customer**, browse shows, view seats, book 1–6 seats, and select **Make Payment** to simulate a successful payment.
7. View booking and payment history; cancel a paid booking to generate a simulated refund and release its seats.
8. Log in as a different customer to verify seat reuse after cancellation. Log in as another owner to verify they cannot modify the first owner's show.
9. Log in as **Admin** to display platform-wide reports.

See `docs/DEMO_GUIDE.md` for suggested inputs and menu choices.

## Data model

SQLite tables: `accounts`, `movies`, `theatres`, `screens`, `shows`, `bookings`, `booking_seats`, `occupied_seats`, `payments`, `refunds`.

- Passwords use randomized salts and PBKDF2-HMAC-SHA256 (not plaintext).
- Username uniqueness, single-admin limit, foreign keys, show/screen uniqueness, and reserved-seat uniqueness are enforced in SQLite.
- Each owner can manage **multiple theatres**. Each theatre can have **multiple screens**, and each screen can hold **multiple shows**.
- Seat layouts are generated based on row count (1–8) and column count (1–10), e.g. `A1,A2,B1`.
- Seat reservations are saved transactionally with a unique `(show_id, seat)` constraint to prevent double bookings.
- New bookings start as `PENDING`; simulated successful payment changes them to `PAID`. Cancelling a paid booking records a simulated refund.
- The owner of a booking, the relevant theatre owner, and the admin see different scoped histories according to their roles.

## Scope and limitations

- **Payments and refunds are simulations only:** No card numbers, actual payment gateway, real money transfer, taxes, or payment credentials are collected.
- This is a **training-level** application, not a production ticketing service. Reservations marked `PENDING` hold seats until paid or cancelled; automatic expiry is not implemented.
- Screening overlap/duration rules, past-date business restrictions, and seat-class pricing are not implemented. The program prevents duplicate start times on a single screen.
- **Database files from earlier 2-role versions have a different schema and are not automatically migrated.** For a clean demo, use a fresh `data/cinema.db` in this version. Back up old databases rather than deleting them if you need their data.
- Database files and account passwords are deliberately **not included** in the ZIP.

## Project layout

```text
Movie_Ticket_Booking_3Roles/
├── src/
│   ├── main.cpp
│   ├── App.cpp
│   └── Database.cpp
├── include/
│   ├── App.h
│   └── Database.h
├── docs/
│   └── DEMO_GUIDE.md
├── tests/
│   └── test_app.py
├── data/                 (created automatically)
├── Makefile
├── run.sh
├── .gitignore
└── README.md
```

## Bulk movie addition and languages

Platform Admin menu option **2. Movie Management** then **1. Add Movies (Bulk)** asks how many movies to add (1-50). For each title, enter genre, duration, language count (1-10), and each language (e.g. Telugu, Hindi, Tamil). The application validates duplicate languages and stores languages in `movie_languages`. Each movie and its languages are saved together in a database transaction.

An approved Theatre Owner uses **3. Show Management**: select a movie, select one of that movie's available languages, choose an owned screen, then enter date/time and price. The chosen show language is stored in SQLite `shows.language` and shown in the show listings. Customer search also accepts a language.

For existing database files from this three-role version, the application automatically adds the `shows.language` field if missing; pre-existing shows retain an empty (unspecified) language. Keep a backup of `data/cinema.db` before updating your project files.

## New: Movie removal and cancelled-show replacement

**Platform Admin → Movie Management:** Add Movies (Bulk), View, Search, Update Movie Details, or Remove Movie. Removal is a **soft deletion** (`movies.active=0`) so historical records are preserved. It is blocked if the movie still has scheduled shows; cancel those shows first.

**Theatre Owner → Show Management:** Add Show, View My Shows (including cancelled), Cancel Show, or Replace Cancelled Show. Owners can operate **only on their own** shows.

To replace a cancelled show:
1. Select **Cancel Show** and confirm. The system cancels existing pending/paid reservations, releases their seats, and inserts simulated refunds for paid bookings.
2. Select **Replace Cancelled Show**, enter the original cancelled Show ID, select a new active movie, its registered language, and ticket price.
3. A **new Show ID** is created on the same theatre screen, date and time. The original cancelled show remains in SQLite with its original movie and booking history. Customers must make a fresh booking for the replacement.

The SQLite schema upgrades existing **three-role** databases automatically on startup. It may rebuild the legacy `shows` table to replace the old unconditional screen/time uniqueness constraint with a constraint for active scheduled shows only. **Back up `data/cinema.db` before first execution of this update.** The older two-role project uses a different database schema and is not supported by this migration.

### Update in Codespaces

Keep the old `data/cinema.db` file, but back it up before replacing any files:

```bash
cp data/cinema.db data/cinema_backup.db
make clean
make
./movie_booking
```

When copying the new ZIP, merge the source directories (`src`, `include`, `docs`, `tests`) and replace `Makefile` with the updated one. Do not overwrite your existing database with a test or sample database. You can use `make test` to run isolated regression tests without changing your real database.

## Login and Password Recovery

The main menu offers **Login**, **Sign Up**, and **Exit**. Selecting Login opens:

```text
1. Enter Username and Password
2. Forgot Password
3. Back
```

During first-time administrator setup or customer/theatre-owner registration, the app generates a unique recovery code and displays it **once**. Store it securely: only its PBKDF2-HMAC-SHA256 hash is saved in the SQLite `account_recovery` table. To reset a forgotten password, choose **Login -> Forgot Password**, enter your username, recovery code, and a new password twice. The used recovery code is invalidated and a new code is displayed once. Password reset does not remove bookings or account details.

Every logged-in role has an **Account Security** option (Admin/Owner option 11, Customer option 13) to change its password or generate/replace its recovery code. Both require the current password. These options allow accounts created with earlier versions of the project to register a recovery code without recreating any records.

**Legacy accounts:** An account created before this update has *no recovery code*. If you can still sign in, use Account Security to generate one. If the password is already forgotten and no recovery code was issued, this console-only demo cannot verify ownership and therefore cannot perform a self-service reset. Do not delete `data/cinema.db` to resolve this: that would erase your saved records. A separate trusted administrator recovery process would be required.

### Testing

```bash
mkdir -p data
make
make test
./movie_booking
```

The authentication test uses an isolated temporary database; it does not modify the real `data/cinema.db`.

## Final demo enhancements (15 November 2026)

- **Theatre Owner > Add Show:** validates calendar date/time, rejects shows that have already started, and checks scheduling overlap for the same screen using the full movie duration **plus a 15-minute cleanup gap**. Separate screens can run simultaneous shows.
- **Customer > Book Tickets:** validates that the show has not started. The existing maximum of **six unique seats per booking** remains enforced.
- **Customer > Cancel My Booking:** rejects cancellation after the show starts; pending and paid bookings that are cancelled before start release their seats, and paid bookings receive simulated refund entries. Owner-driven whole-show cancellation remains a separate function.
- **Customer > Option 14 Search My Booking / Receipt:** look up an owned booking using its ID. Receipts include show ID, theatre/city, screen, language, seats, booking status, and amount.
- **Admin > Movie Management > Option 7 Reactivate Movie:** restore an inactive movie without losing its existing languages or history. Option 6 remains Back.
- **Theatre Owner > Replace Cancelled Show:** also validates that the original time is still in the future and that the replacement will not overlap another scheduled movie on the screen.
- **Owner/Admin reports:** Existing booking, revenue, and occupancy summaries are retained.

All updates use the same SQLite data model. **Back up `data/cinema.db` before replacing an older project** and keep the real database in the same working directory when running `./movie_booking`.

To compile and run from the folder containing `Makefile`:

```bash
mkdir -p data
make
./movie_booking
```

To run all regression tests without editing your real database:

```bash
make test
```
