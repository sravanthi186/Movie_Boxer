# Short Demo Guide

## Run

```bash
make
./movie_booking
```

This guide assumes a **fresh** database. The numbers of created records may differ on a database that already has data. Use the IDs displayed by the application.

## 1 — Create the admin

First-run prompt: enter a new admin username, a password of at least 8 characters, and confirmation. Keep the password private. The admin must then log in through the normal welcome menu.

## 2 — Register an owner and a customer

From the welcome menu, select `2. Sign Up`, then `2. Theatre Owner`. Choose a username, display name, and password. Repeat Sign Up for `1. Customer`.

Try signing in as the owner **before approval**. The program should reject owner access and show the pending-approval message.

## 3 — Admin operations

- `1. Login` → use the admin account.
- Admin `3. Theatre Owner Approvals / Management` → `1. View accounts` → find the owner's numeric ID → `2. Approve owner` → enter its ID → `4. Back`.
- Admin `2. Movie Management` → add movie `The Space Mission`, genre `Sci-Fi`, duration `140`.
- Admin `10. Logout`.

## 4 — Theatre owner operations

- Log in with approved owner credentials.
- Owner `1. My Theatres` → enter `Central Cinema`, city `Hyderabad`.
- Owner `2. Screen Management` → choose your theatre ID, add `Screen 1`, `5` rows and `8` columns.
- Owner `3. Show Management` → select the movie ID, your screen ID, `2026-12-16`, `18:30`, price `250` rupees.
- Owner `6. View My Shows` → confirm show appears, then `10. Logout`.

## 5 — Customer operations

- Log in as Customer.
- Customer `1` / `2` / `3` → browse, search, and view movie details.
- Customer `4` → view shows; note the show ID.
- Customer `5` → view the seats for that show (unoccupied seats appear as `[A1]` and booked seats as `[XX]`).
- Customer `6` → book that show, choose `A1,A2`, confirm with `y` → note the new Booking ID.
- Customer `7` → make simulated payment for that Booking ID; select payment method, confirm `y`.
- Customer `8` → verify booking now shows `PAID`.
- Customer `9` → cancel the same Booking ID and confirm `y`.
- Customer `10` → verify simulated payment and refund history.
- Customer `12` → log out.

## 6 — Permissions and reports

- Create a second customer and show they cannot cancel the first customer's bookings.
- Create a second owner and have admin approve it. That owner can add their own theatre but cannot change the first owner's show price.
- As admin, review all bookings, payments/refunds, and report dashboard.
- Restart the program to demonstrate that users, theatres, shows, bookings, and payments persist in `data/cinema.db`.

**Explain to the trainer:** This program stores booking data in SQLite, protects accounts with hashed passwords, controls permissions by role, and prevents seat double-booking through database transactions and a unique seat constraint. Payments/refunds are for demonstration only.

## Bulk movies and multilingual shows demonstration

1. Log in as Platform Admin and choose **2**.
2. Enter **3** for the number of movies. For each movie, supply its name, genre, duration, number of languages and individual languages.
3. From View Movies, confirm that all three titles and their languages appear.
4. Log in as an approved Theatre Owner, choose **3. Show Management**. Select one added movie; the system shows only its registered languages. Choose one, then select an owned screen, date/time and price.
5. Log in as Customer and view/search movies or view shows. Confirm the selected language is displayed with the show.

## Cancel and replace a show (new feature)

1. Admin → **Movie Management → Add Movies (Bulk)**: ensure at least two movies exist.
2. Theatre Owner → **Show Management → Add Show**: schedule the first movie in your screen and note its Show ID.
3. Customer → book seats in that show and complete a simulated payment.
4. Theatre Owner → **Show Management → Cancel Show**: enter the first Show ID and type `y` to confirm. The system cancels its reservations and records simulated refunds for paid bookings.
5. Theatre Owner → **Show Management → Replace Cancelled Show**: enter the cancelled Show ID, choose another movie, registered language, new price, and confirm with `y`.
6. View My Shows: the original is **CANCELLED** and the replacement has a **new Show ID** at the same screen, date and time. Customer books the new show separately.
7. Admin → **Movie Management → Remove Movie**: only movies without scheduled shows can be deactivated. Past booking records remain visible.
