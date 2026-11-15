#pragma once

#include "Database.h"
#include <string>

class App {
private:
    Database db;
    int uid = 0;
    std::string username;
    std::string role;

    std::string input(const std::string& prompt);
    std::string password(const std::string& prompt);
    int num(const std::string& prompt, int low, int high);

    void setup();
    void signup();
    void login();
    void loginMenu();
    void forgotPassword();
    void accountSecurity();
    void logout();

    void admin();
    void owner();
    void customer();

    void movies(bool search = false);
    void details();
    void shows(int ownerId = 0);
    void seats();

    void movieManagement();
    void addMovie();
    void updateMovie();
    void removeMovie();
    void reactivateMovie();

    void owners();
    void customers();
    void theatres();
    void screens();

    void showManagement();
    void addShow();
    void changePrice();
    void cancelShow();
    void replaceCancelledShow();

    void booking();
    void payment();
    void myBookings(bool all = false);
    void cancel(bool all = false);
    void showBooking(int id);
    void searchBooking();
    void payments();
    void report(bool ownerOnly);
    void profile();

    bool ownerApproved();
    bool isOwnerOfShow(int showId);
    bool isOwnerOfBooking(int bookingId);

public:
    explicit App(const std::string& path) : db(path) {}
    void run();
};
