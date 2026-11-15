# Final feature changes

| Section | Enhancement | Expected behavior |
|---|---|---|
| Owner: Add Show | Show timing conflicts | Reject same-screen overlaps including a 15-minute gap after each film; allow another screen at the same time |
| Owner: Add Show | Future date/time | Reject impossible dates and any show scheduled in the past |
| Customer: Book Tickets | Booking limit | Maximum six distinct seats; block occupied/invalid seats and started shows |
| Customer: Cancel Booking | Cancellation cutoff | Cancel only before the show starts; keep bookings/refunds in the database |
| Customer: Option 14 | Search My Booking | Look up own booking ID and print a detailed receipt; no access to another user's booking |
| Admin: Movie Management option 7 | Reactivate Movie | Restore an inactive catalog movie while preserving its language list and history |
| Owner: Replace Cancelled Show | Conflict and date validation | Only allow a future replacement without an overlapping scheduled show |
| Reports | Revenue and occupancy | Existing role-specific reporting is preserved |

**Note:** Payment/refund flows are simulations; this project does not charge or refund real money.
