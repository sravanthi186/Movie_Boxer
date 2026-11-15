# Password Recovery — Demo

1. Start `./movie_booking` in the project directory (ensure `data/` exists).
2. First run: create the single admin account. **Save the recovery code shown once.**
3. Main menu > **Login** > **Enter Username and Password** to sign in.
4. Main menu > **Sign Up** to create customers and theatre owners. **Save each account's recovery code.**
5. Main menu > **Login** > **Forgot Password** to enter the username and its recovery code.
6. Set a new password, confirm it, and **save the new recovery code**. The old code cannot be reused.
7. Main menu > **Login** > **Enter Username and Password** to log in with the new password.
8. For accounts from older versions, log in with the existing password and select **Account Security** in the role menu to generate a recovery code.

Do not share actual recovery codes publicly or add them to GitHub. The test files use disposable credentials and a temporary database only.
