#include "App.h"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <termios.h>
#include <unistd.h>
using namespace std;
namespace {
int integer(const string& s){try{return stoi(s);}catch(...){return 0;}}
string money(int paise){ostringstream os;os<<"Rs. "<<fixed<<setprecision(2)<<paise/100.0;return os.str();}
bool validUsername(const string& x){if(x.size()<3||x.size()>30)return false;for(unsigned char c:x)if(!isalnum(c)&&c!='_')return false;return true;}
bool validDate(const string& d){if(d.size()!=10||d[4]!='-'||d[7]!='-')return false;for(int i=0;i<10;i++)if(i!=4&&i!=7&&!isdigit((unsigned char)d[i]))return false;int y=integer(d.substr(0,4)),m=integer(d.substr(5,2)),day=integer(d.substr(8,2));if(y<2020||y>2100||m<1||m>12||day<1||day>31)return false;tm t={};t.tm_year=y-1900;t.tm_mon=m-1;t.tm_mday=day;t.tm_isdst=-1;mktime(&t);return t.tm_year==y-1900&&t.tm_mon==m-1&&t.tm_mday==day;}
bool validTime(const string& t){return t.size()==5&&isdigit(t[0])&&isdigit(t[1])&&t[2]==':'&&isdigit(t[3])&&isdigit(t[4])&&integer(t.substr(0,2))<24&&integer(t.substr(3,2))<60;}
string trim(string s){size_t a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");return a==string::npos?"":s.substr(a,b-a+1);}
string upper(string s){for(char& c:s)c=(char)toupper((unsigned char)c);return s;}
// Local show times are stored in YYYY-MM-DD HH:MM format.
time_t showStart(const string& date, const string& time) {
    if (!validDate(date) || !validTime(time)) return (time_t)-1;
    tm value = {};
    value.tm_year = integer(date.substr(0, 4)) - 1900;
    value.tm_mon = integer(date.substr(5, 2)) - 1;
    value.tm_mday = integer(date.substr(8, 2));
    value.tm_hour = integer(time.substr(0, 2));
    value.tm_min = integer(time.substr(3, 2));
    value.tm_isdst = -1;
    return mktime(&value);
}
bool isFutureShow(const string& date, const string& time) {
    time_t start = showStart(date, time);
    return start != (time_t)-1 && difftime(start, ::time(nullptr)) > 0;
}
bool hasScreenOverlap(Database& db, int screen, const string& date,
                      const string& clock, int movie, int exclude = 0) {
    auto duration = db.query("SELECT duration_minutes FROM movies WHERE id=? AND active=1",
                             {to_string(movie)});
    if (duration.empty()) return true;
    time_t requested = showStart(date, clock);
    if (requested == (time_t)-1) return true;
    // Include 15 minutes for cleaning before the next show.
    time_t requestedEnd = requested + (integer(duration[0][0]) + 15) * 60;
    auto existing = db.query(
        "SELECT s.id,s.show_date,s.show_time,m.duration_minutes "
        "FROM shows s JOIN movies m ON m.id=s.movie_id "
        "WHERE s.screen_id=? AND s.status='SCHEDULED' AND s.id<>?",
        {to_string(screen), to_string(exclude)});
    for (const auto& row : existing) {
        time_t start = showStart(row[1], row[2]);
        time_t end = start + (integer(row[3]) + 15) * 60;
        if (requested < end && start < requestedEnd) return true;
    }
    return false;
}
}
string App::input(const string& prompt){cout<<prompt<<flush;string value;if(!getline(cin,value))throw runtime_error("Input ended");return trim(value);}
string App::password(const string& prompt){
    cout<<prompt<<flush;
    termios original;
    bool hidden=isatty(STDIN_FILENO)&&tcgetattr(STDIN_FILENO,&original)==0;
    if(hidden){
        termios masked=original;
        masked.c_lflag&=~ECHO;
        if(tcsetattr(STDIN_FILENO,TCSANOW,&masked)!=0)hidden=false;
    }
    string value;
    bool good=(bool)getline(cin,value);
    if(hidden){
        tcsetattr(STDIN_FILENO,TCSANOW,&original);
        cout<<'\n';
    }
    if(!good)throw runtime_error("Input ended");
    return value;
}
int App::num(const string& prompt,int low,int high){
    while(true){
        string text=input(prompt);
        try{
            size_t end=0;
            long x=stol(text,&end);
            if(end==text.size()&&x>=low&&x<=high)return (int)x;
        }
        catch(...){
        }
        cout<<"Enter a valid number ("<<low<<"-"<<high<<").\n";
    }
}
void App::setup(){
    cout<<"\n=== FIRST RUN: CREATE THE SINGLE PLATFORM ADMIN ===\n";
    string u=input("Admin username: "),p=password("Password (minimum 8 characters): "),confirm=password("Confirm password: ");
    if(!validUsername(u)||p.size()<8||p.size()>128||p!=confirm){
        cout<<"Invalid username/password or confirmation.\n";
        return;
    }
    string salt=newSalt();
    try{
        db.change("INSERT INTO accounts(username,salt,password_hash,role,approved,display_name) VALUES(?,?,?,'ADMIN',1,?)",{
            u,salt,passwordHash(p,salt),u
        });
        string recovery = newSalt();
        string recoverySalt = newSalt();
        db.change("INSERT INTO account_recovery(account_id,salt,code_hash) "
                  "VALUES((SELECT id FROM accounts WHERE username=?),?,?)",
                  {u,recoverySalt,passwordHash(recovery,recoverySalt)});
        cout<<"Platform administrator created. Please log in.\n";
        cout<<"YOUR RECOVERY CODE (SAVE IT NOW): "<<recovery<<"\n";
        cout<<"This code will not be shown again.\n";
    }
    catch(const exception& e){
        cout<<"Setup failed: "<<e.what()<<"\n";
    }
}
void App::signup(){
    cout<<"\n=== REGISTER ACCOUNT ===\n1. Customer\n2. Theatre Owner (requires admin approval)\n";
    int choice=num("Choose role: ",1,2);
    string u=input("Username (3-30 letters/numbers/_): "),name=input("Display name: "),p=password("Password (8-128 characters): "),confirm=password("Confirm password: ");
    if(!validUsername(u)||trim(name).empty()||name.size()>70||p.size()<8||p.size()>128||p!=confirm){
        cout<<"Invalid details or passwords do not match.\n";
        return;
    }
    string salt=newSalt();
    try{
        db.change("INSERT INTO accounts(username,display_name,salt,password_hash,role,approved) VALUES(?,?,?,?,?,?)",{
            u,name,salt,passwordHash(p,salt),choice==1?"CUSTOMER":"OWNER",choice==1?"1":"0"
        });
        string recovery = newSalt();
        string recoverySalt = newSalt();
        db.change("INSERT INTO account_recovery(account_id,salt,code_hash) "
                  "VALUES((SELECT id FROM accounts WHERE username=?),?,?)",
                  {u,recoverySalt,passwordHash(recovery,recoverySalt)});
        cout<<(choice==1?"Customer registered. Please log in.\n":"Owner request submitted. Admin approval required before login.\n");
        cout<<"YOUR RECOVERY CODE (SAVE IT NOW): "<<recovery<<"\n";
        cout<<"This code will not be shown again.\n";
    }
    catch(const exception&){
        cout<<"Username already exists. Try another.\n";
    }
}
void App::login(){
    string u=input("Username: "),p=password("Password: ");
    auto a=db.query("SELECT id,username,salt,password_hash,role,approved,active FROM accounts WHERE username=?",{
        u
    });
    if(a.empty()||!passwordMatches(p,a[0][2],a[0][3])){
        cout<<"Invalid username or password.\n";
        return;
    }
    if(a[0][6]!="1"){
        cout<<"Account disabled. Contact administrator.\n";
        return;
    }
    if(a[0][4]=="OWNER"&&a[0][5]!="1"){
        cout<<"Owner account pending admin approval.\n";
        return;
    }
    uid=integer(a[0][0]);
    username=a[0][1];
    role=a[0][4];
    cout<<"Welcome "<<username<<" ("<<role<<")!\n";
}
void App::forgotPassword() {
    cout << "\n========== FORGOT PASSWORD ==========\n";
    string user = input("Username: ");
    string code = password("Recovery code: ");
    auto account = db.query(
        "SELECT a.id,r.salt,r.code_hash FROM accounts a "
        "JOIN account_recovery r ON r.account_id=a.id WHERE a.username=? AND a.active=1",
        {user});
    if (account.empty() || !passwordMatches(code,account[0][1],account[0][2])) {
        cout << "Invalid username or recovery code.\n";
        return;
    }
    string fresh = password("New password (8-128 characters): ");
    string confirm = password("Confirm new password: ");
    if (fresh.size()<8 || fresh.size()>128 || fresh!=confirm) {
        cout << "Invalid password or confirmation does not match.\n";
        return;
    }
    string salt = newSalt();
    // Rotate the recovery code to make a used code invalid immediately.
    string newCode = newSalt(), newRecoverySalt = newSalt();
    try {
        db.begin();
        db.change("UPDATE accounts SET salt=?,password_hash=? WHERE id=?",
                  {salt,passwordHash(fresh,salt),account[0][0]});
        db.change("UPDATE account_recovery SET salt=?,code_hash=? WHERE account_id=?",
                  {newRecoverySalt,passwordHash(newCode,newRecoverySalt),account[0][0]});
        db.commit();
        cout << "Password updated successfully. Please login again.\n";
        cout << "NEW RECOVERY CODE (SAVE IT NOW): " << newCode << "\n";
    } catch (...) {
        db.rollback();
        cout << "Password reset failed.\n";
    }
}
void App::loginMenu() {
    while (true) {
        cout << "\n========== LOGIN ==========\n"
             << "1. Enter Username and Password\n"
             << "2. Forgot Password\n"
             << "3. Back\n";
        int option = num("Choose: ",1,3);
        if (option==3) return;
        if (option==2) {
            forgotPassword();
            continue;
        }
        login();
        return;
    }
}
void App::accountSecurity() {
    cout << "\n====== ACCOUNT SECURITY ======\n"
         << "1. Change Password\n"
         << "2. Generate / Replace Recovery Code\n"
         << "3. Back\n";
    int option = num("Choose: ",1,3);
    if (option==3) return;
    auto current = db.query("SELECT salt,password_hash FROM accounts WHERE id=?",
                            {to_string(uid)});
    if (current.empty()) return;
    string old = password("Current password: ");
    if (!passwordMatches(old,current[0][0],current[0][1])) {
        cout << "Incorrect password.\n";
        return;
    }
    if (option==1) {
        string fresh = password("New password (8-128 characters): ");
        string confirm = password("Confirm new password: ");
        if (fresh.size()<8 || fresh.size()>128 || fresh!=confirm) {
            cout << "Invalid password or confirmation does not match.\n";
            return;
        }
        string salt = newSalt();
        db.change("UPDATE accounts SET salt=?,password_hash=? WHERE id=?",
                  {salt,passwordHash(fresh,salt),to_string(uid)});
        cout << "Password changed successfully.\n";
        return;
    }
    string recovery = newSalt(), salt = newSalt();
    db.change("INSERT OR REPLACE INTO account_recovery(account_id,salt,code_hash) VALUES(?,?,?)",
              {to_string(uid),salt,passwordHash(recovery,salt)});
    cout << "YOUR RECOVERY CODE (SAVE IT NOW): " << recovery << "\n";
    cout << "Previous recovery codes are now invalid.\n";
}
void App::logout(){uid=0;username.clear();role.clear();cout<<"Logged out.\n";}
void App::run(){
    while(true){
        if(db.query("SELECT id FROM accounts WHERE role='ADMIN'").empty()){
            setup();
            continue;
        }
        cout<<"\n========== MOVIE TICKET BOOKING =========="<<"\n1. Login\n2. Sign Up\n3. Exit\n";
        int c=num("Choose: ",1,3);
        if(c==3){
            cout<<"Goodbye!\n";
            return;
        }
        if(c==2){
            signup();
            continue;
        }
        loginMenu();
        if(role=="ADMIN")admin();
        else if(role=="OWNER")owner();
        else if(role=="CUSTOMER")customer();
    }
}
void App::movies(bool search){
 string key;if(search)key=input("Search movie title/genre/language: ");
 auto a=db.query("SELECT m.id,m.title,m.genre,m.duration_minutes,COALESCE(group_concat(ml.language,', '),'Not specified') FROM movies m LEFT JOIN movie_languages ml ON ml.movie_id=m.id WHERE m.active=1 AND (m.title LIKE ? OR m.genre LIKE ? OR EXISTS(SELECT 1 FROM movie_languages x WHERE x.movie_id=m.id AND x.language LIKE ?)) GROUP BY m.id ORDER BY m.id",{"%"+key+"%","%"+key+"%","%"+key+"%"});
 cout<<"\nMOVIES\n";
 for(auto& r:a)cout<<r[0]<<" | "<<r[1]<<" | "<<r[2]<<" | "<<r[3]<<" min | Languages: "<<r[4]<<'\n';
 if(a.empty())cout<<"No movies found.\n";
}
void App::details(){
    int id=num("Movie ID: ",1,1000000);
    auto a=db.query("SELECT id,title,genre,duration_minutes FROM movies WHERE id=? AND active=1",{
        to_string(id)
    });
    if(a.empty()){
        cout<<"Movie not found.\n";
        return;
    }
    auto r=a[0];
    auto langs=db.query("SELECT language FROM movie_languages WHERE movie_id=? ORDER BY language",{
        to_string(id)
    });
    cout<<"Movie: "<<r[1]<<" | Genre: "<<r[2]<<" | Runtime: "<<r[3]<<" min | Languages: ";
    for(auto &l:langs)cout<<l[0]<<" ";
    cout<<"\n";
    auto ss=db.query("SELECT s.id,t.name,t.city,sc.name,s.show_date,s.show_time,s.price_paise FROM shows s JOIN screens sc ON sc.id=s.screen_id JOIN theatres t ON t.id=sc.theatre_id WHERE s.movie_id=? AND t.active=1 AND s.status='SCHEDULED' ORDER BY s.show_date,s.show_time",{
        to_string(id)
    });
    for(auto& x:ss)cout<<"Show "<<x[0]<<" | "<<x[1]<<", "<<x[2]<<" / "<<x[3]<<" | "<<x[4]<<" "<<x[5]<<" | "<<money(integer(x[6]))<<"\n";
}
void App::shows(int ownerId){
    string sql="SELECT s.id,m.title,t.name,t.city,sc.name,s.show_date,s.show_time,s.price_paise,sc.rows_count,sc.cols_count,s.language,s.status FROM shows s JOIN movies m ON m.id=s.movie_id JOIN screens sc ON sc.id=s.screen_id JOIN theatres t ON t.id=sc.theatre_id WHERE t.active=1";
    vector<string> args;
    if(!ownerId)sql+=" AND s.status='SCHEDULED'";
    if(ownerId){
        sql+=" AND t.owner_id=?";
        args.push_back(to_string(ownerId));
    }
    sql+=" ORDER BY s.show_date,s.show_time,s.id";
    auto a=db.query(sql,args);
    cout<<"\nAVAILABLE SHOWS\n";
    for(auto& x:a)cout<<"ID "<<x[0]<<" | "<<x[1]<<" | "<<x[2]<<", "<<x[3]<<" - "<<x[4]<<" | "<<x[5]<<" "<<x[6]<<" | "<<money(integer(x[7]))<<" | "<<integer(x[8])*integer(x[9])<<" seats | Language: "<<(x[10].empty()?"Not specified":x[10])<<" | Status: "<<x[11]<<"\n";
    if(a.empty())cout<<"No shows.\n";
}
void App::seats(){
    shows();
    int id=num("Show ID: ",1,1000000);
    auto a=db.query("SELECT sc.rows_count,sc.cols_count FROM shows s JOIN screens sc ON sc.id=s.screen_id WHERE s.id=? AND s.status='SCHEDULED'",{
        to_string(id)
    });
    if(a.empty()){
        cout<<"Show not found.\n";
        return;
    }
    int rows=integer(a[0][0]),cols=integer(a[0][1]);
    auto occ=db.query("SELECT seat FROM occupied_seats WHERE show_id=?",{
        to_string(id)
    });
    set<string> occupied;
    for(auto& x:occ)occupied.insert(x[0]);
    cout<<"\n============== SCREEN ==============\n";
    for(int r=0;r<rows;r++){
        for(int c=1;c<=cols;c++){
            string code=string(1,char('A'+r))+to_string(c);
            cout<<(occupied.count(code)?"[XX]":"["+code+"]")<<" ";
        }
        cout<<'\n';
    }
    cout<<"XX = Reserved; Available "<<rows*cols-(int)occupied.size()<<" of "<<rows*cols<<"\n";
}
void App::addMovie(){
 int count=num("How many movies do you want to add (1-50)? ",1,50);
 int saved=0;
 for(int i=1;i<=count;i++){
  cout<<"\n----- MOVIE "<<i<<" OF "<<count<<" -----\n";
  string title=input("Title: "),genre=input("Genre: ");
  int duration=num("Duration (minutes): ",30,400);
  if(title.empty()||title.size()>100||genre.empty()||genre.size()>50){cout<<"Invalid movie details. Skipped.\n";continue;}
  int n=num("How many languages (1-10)? ",1,10);
  vector<string> languages;set<string> unique;
  bool valid=true;
  for(int j=1;j<=n;j++){
   string lang=input("Language "+to_string(j)+": ");
   string normalized=upper(lang);
   if(lang.empty()||lang.size()>40||!unique.insert(normalized).second){cout<<"Invalid or duplicate language. Movie skipped.\n";valid=false;break;}
   languages.push_back(lang);
  }
  if(!valid)continue;
  try{
   db.begin();
   db.change("INSERT INTO movies(title,genre,duration_minutes) VALUES(?,?,?)",{title,genre,to_string(duration)});
   int id=db.id();
   for(const string& lang:languages)db.change("INSERT INTO movie_languages(movie_id,language) VALUES(?,?)",{to_string(id),lang});
   db.commit();saved++;
   cout<<"Movie added! Movie ID: "<<id<<" | Languages: "<<languages.size()<<"\n";
  }catch(const exception& e){db.rollback();cout<<"Could not save movie: "<<e.what()<<"\n";}
 }
 cout<<"\nCompleted. "<<saved<<" of "<<count<<" movies added.\n";
}
void App::owners(){
    while(true){
        cout<<"\nTHEATRE OWNER MANAGEMENT\n1. View accounts\n2. Approve owner\n3. Disable/enable owner\n4. Back\n";
        int c=num("Choose: ",1,4);
        if(c==4)return;
        if(c==1){
            auto a=db.query("SELECT id,username,display_name,approved,active FROM accounts WHERE role='OWNER' ORDER BY id");
            for(auto& r:a)cout<<r[0]<<" | "<<r[1]<<" | "<<r[2]<<" | approved="<<r[3]<<" | active="<<r[4]<<'\n';
        }
        if(c==2){
            int id=num("Owner ID to approve: ",1,1000000);
            cout<<(db.change("UPDATE accounts SET approved=1 WHERE id=? AND role='OWNER' AND approved=0",{
                to_string(id)
            })?"Approved.\n":"No pending owner with that ID.\n");
        }
        if(c==3){
            int id=num("Owner ID: ",1,1000000);
            int active=num("Enter 1 to enable, 0 to disable: ",0,1);
            cout<<(db.change("UPDATE accounts SET active=? WHERE id=? AND role='OWNER'",{
                to_string(active),to_string(id)
            })?"Updated.\n":"Owner not found.\n");
        }
    }
}
void App::customers(){
    auto a=db.query("SELECT id,username,display_name,active FROM accounts WHERE role='CUSTOMER' ORDER BY id");
    cout<<"\nCUSTOMER ACCOUNTS\n";
    for(auto& r:a)cout<<r[0]<<" | "<<r[1]<<" | "<<r[2]<<" | active="<<r[3]<<'\n';
    int id=num("Customer ID to change status (0 to go back): ",0,1000000);
    if(id){
        int active=num("1 = enable, 0 = disable: ",0,1);
        cout<<(db.change("UPDATE accounts SET active=? WHERE id=? AND role='CUSTOMER'",{
            to_string(active),to_string(id)
        })?"Updated.\n":"Not found.\n");
    }
}
bool App::ownerApproved(){auto a=db.query("SELECT 1 FROM accounts WHERE id=? AND role='OWNER' AND approved=1 AND active=1",{to_string(uid)});return !a.empty();}
void App::theatres(){
    auto a=db.query("SELECT id,name,city FROM theatres WHERE owner_id=? ORDER BY id",{
        to_string(uid)
    });
    cout<<"\nMY THEATRES\n";
    for(auto& r:a)cout<<r[0]<<" | "<<r[1]<<" | "<<r[2]<<'\n';
    string name=input("Add new theatre name (blank to go back): ");
    if(name.empty())return;
    string city=input("City: ");
    if(name.size()>80||city.empty()||city.size()>60){
        cout<<"Invalid theatre.\n";
        return;
    }
    try{
        db.change("INSERT INTO theatres(owner_id,name,city) VALUES(?,?,?)",{
            to_string(uid),name,city
        });
        cout<<"Theatre added.\n";
    }
    catch(const exception&e){
        cout<<"Unable to add: "<<e.what()<<'\n';
    }
}
void App::screens(){
    int theatre=num("Your theatre ID: ",1,1000000);
    auto a=db.query("SELECT id,name,rows_count,cols_count FROM screens WHERE theatre_id=? AND theatre_id IN (SELECT id FROM theatres WHERE owner_id=?)",{
        to_string(theatre),to_string(uid)
    });
    auto own=db.query("SELECT 1 FROM theatres WHERE id=? AND owner_id=?",{
        to_string(theatre),to_string(uid)
    });
    if(own.empty()){
        cout<<"Access denied: not your theatre.\n";
        return;
    }
    for(auto&r:a)cout<<"Screen "<<r[0]<<" | "<<r[1]<<" | "<<r[2]<<" rows x "<<r[3]<<" columns\n";
    string name=input("Add screen name (blank = back): ");
    if(name.empty())return;
    int rows=num("Rows (1-8): ",1,8),cols=num("Columns (1-10): ",1,10);
    try{
        db.change("INSERT INTO screens(theatre_id,name,rows_count,cols_count) VALUES(?,?,?,?)",{
            to_string(theatre),name,to_string(rows),to_string(cols)
        });
        cout<<"Screen added.\n";
    }
    catch(const exception&e){
        cout<<"Cannot add screen: "<<e.what()<<'\n';
    }
}
void App::addShow(){
 movies();int movie=num("Movie ID: ",1,1000000);
 auto langs=db.query("SELECT ml.language FROM movie_languages ml JOIN movies m ON m.id=ml.movie_id WHERE ml.movie_id=? AND m.active=1 ORDER BY ml.language",{to_string(movie)});
 if(langs.empty()){cout<<"This movie has no registered languages or does not exist. Ask admin to add languages.\n";return;}
 cout<<"LANGUAGES FOR SELECTED MOVIE\n";
 for(size_t i=0;i<langs.size();i++)cout<<(i+1)<<". "<<langs[i][0]<<'\n';
 int selection=num("Choose language: ",1,(int)langs.size());
 string language=langs[selection-1][0];
 auto screens=db.query("SELECT sc.id,t.name,sc.name FROM screens sc JOIN theatres t ON t.id=sc.theatre_id WHERE t.owner_id=? AND t.active=1",{to_string(uid)});
 cout<<"YOUR SCREENS\n";for(auto&r:screens)cout<<r[0]<<" | "<<r[1]<<" - "<<r[2]<<'\n';
 int screen=num("Screen ID: ",1,1000000);
 auto own=db.query("SELECT 1 FROM screens sc JOIN theatres t ON t.id=sc.theatre_id WHERE sc.id=? AND t.owner_id=? AND t.active=1",{to_string(screen),to_string(uid)});
 if(own.empty()){cout<<"Cannot use another owner's screen.\n";return;}
 string date=input("Date YYYY-MM-DD: "),time=input("Time HH:MM: ");int rupees=num("Price in rupees: ",1,10000);
 if (!validDate(date) || !validTime(time) || !isFutureShow(date, time)) {
     cout << "Invalid date/time. Choose a valid future show time.\n";
     return;
 }
 try {
     db.begin();
     if (hasScreenOverlap(db, screen, date, time, movie)) {
         cout << "Screen conflict: another show overlaps this slot (including 15-minute cleanup).\n";
         db.rollback();
         return;
     }
     db.change("INSERT INTO shows(movie_id,screen_id,show_date,show_time,price_paise,language) "
               "VALUES(?,?,?,?,?,?)",
               {to_string(movie),to_string(screen),date,time,to_string(rupees*100),language});
     db.commit();
     cout << "Show added in " << language << ".\n";
 } catch (const exception& e) {
     db.rollback();
     cout << "Could not add show: " << e.what() << '\n';
 }
}
void App::changePrice(){
    shows(uid);
    int id=num("Show ID: ",1,1000000),price=num("New price (rupees): ",1,10000);
    cout<<(db.change("UPDATE shows SET price_paise=? WHERE id=? AND status='SCHEDULED' AND screen_id IN (SELECT sc.id FROM screens sc JOIN theatres t ON t.id=sc.theatre_id WHERE t.owner_id=?)",{
        to_string(price*100),to_string(id),to_string(uid)
    })?"Price updated.\n":"Access denied or show not found.\n");
}
bool App::isOwnerOfShow(int showId){
    auto a=db.query("SELECT 1 FROM shows s JOIN screens sc ON sc.id=s.screen_id JOIN theatres t ON t.id=sc.theatre_id WHERE s.id=? AND t.owner_id=?",{
        to_string(showId),to_string(uid)
    });
    return !a.empty();
}
bool App::isOwnerOfBooking(int id){
    auto a=db.query("SELECT 1 FROM bookings b JOIN shows s ON s.id=b.show_id JOIN screens sc ON sc.id=s.screen_id JOIN theatres t ON t.id=sc.theatre_id WHERE b.id=? AND t.owner_id=?",{
        to_string(id),to_string(uid)
    });
    return !a.empty();
}
void App::booking(){
    shows();
    int show=num("Choose show ID: ",1,1000000);
    auto details=db.query("SELECT sc.rows_count,sc.cols_count,s.price_paise,m.title FROM shows s JOIN screens sc ON sc.id=s.screen_id JOIN movies m ON m.id=s.movie_id JOIN theatres t ON t.id=sc.theatre_id WHERE s.id=? AND t.active=1 AND s.status='SCHEDULED'",{
        to_string(show)
    });
    if(details.empty()){
        cout<<"Show unavailable.\n";
        return;
    }
    auto start=db.query("SELECT show_date,show_time FROM shows WHERE id=?", {to_string(show)});
    if (start.empty() || !isFutureShow(start[0][0], start[0][1])) {
        cout << "Booking closed: this show has already started.\n";
        return;
    }
    int rows=integer(details[0][0]),cols=integer(details[0][1]),price=integer(details[0][2]);
    auto occupied=db.query("SELECT seat FROM occupied_seats WHERE show_id=?",{
        to_string(show)
    });
    set<string> used;
    for(auto& r:occupied)used.insert(r[0]);
    cout<<"\n"<<details[0][3]<<" | SCREEN\n";
    for(int r=0;r<rows;r++){
        for(int c=1;c<=cols;c++){
            string s=string(1,'A'+r)+to_string(c);
            cout<<(used.count(s)?"[XX]":"["+s+"]")<<' ';
        }
        cout<<'\n';
    }
    string raw=input("Seats separated by commas (max 6, e.g. A1,A2): ");
    stringstream ss(raw);
    string token;
    vector<string> selected;
    set<string> unique;
    while(getline(ss,token,',')){
        token=upper(trim(token));
        if(token.size()<2||token.size()>3||token[0]<'A'||token[0]>=char('A'+rows)){
            cout<<"Invalid seat: "<<token<<'\n';
            return;
        }
        string col=token.substr(1);
        if(col.empty()||col[0]=='0'||!all_of(col.begin(),col.end(),[](unsigned char c){
            return isdigit(c);
        })||integer(col)<1||integer(col)>cols||!unique.insert(token).second){
            cout<<"Invalid or repeated seat: "<<token<<'\n';
            return;
        }
        selected.push_back(token);
    }
    if(selected.empty()||selected.size()>6||(!raw.empty()&&raw.back()==',')){
        cout<<"Select 1-6 unique seats.\n";
        return;
    }
    cout<<"Price: "<<money(price*(int)selected.size())<<" (payment recorded separately)\n";
    if(upper(input("Confirm seat reservation? y/n: "))!="Y")return;
    try{
        db.begin();
        auto current=db.query("SELECT status,show_date,show_time FROM shows WHERE id=?", {to_string(show)});
        if (current.empty() || current[0][0]!="SCHEDULED" ||
            !isFutureShow(current[0][1], current[0][2]))
            throw runtime_error("Show closed or no longer available");
        for(auto&s:selected){
            auto o=db.query("SELECT 1 FROM occupied_seats WHERE show_id=? AND seat=?",{
                to_string(show),s
            });
            if(!o.empty())throw runtime_error("Seat "+s+" already reserved");
        }
        db.change("INSERT INTO bookings(customer_id,show_id,total_paise) VALUES(?,?,?)",{
            to_string(uid),to_string(show),to_string(price*(int)selected.size())
        });
        int id=db.id();
        for(auto&s:selected){
            db.change("INSERT INTO booking_seats(booking_id,seat) VALUES(?,?)",{
                to_string(id),s
            });
            db.change("INSERT INTO occupied_seats(show_id,seat,booking_id) VALUES(?,?,?)",{
                to_string(show),s,to_string(id)
            });
        }
        db.commit();
        cout<<"RESERVATION CREATED. Booking ID: "<<id<<". Status: PENDING PAYMENT.\n";
    }
    catch(const exception&e){
        db.rollback();
        cout<<"Booking failed: "<<e.what()<<'\n';
    }
}
void App::showBooking(int id){
    auto a=db.query("SELECT b.id,a.username,m.title,t.name,s.show_date,s.show_time,b.total_paise,b.status,b.created_at,COALESCE((SELECT group_concat(seat,',') FROM booking_seats WHERE booking_id=b.id),''),s.id,sc.name,s.language,t.city FROM bookings b JOIN accounts a ON a.id=b.customer_id JOIN shows s ON s.id=b.show_id JOIN movies m ON m.id=s.movie_id JOIN screens sc ON sc.id=s.screen_id JOIN theatres t ON t.id=sc.theatre_id WHERE b.id=?",{
        to_string(id)
    });
    if(a.empty()){
        cout<<"Booking not found.\n";
        return;
    }
    auto r=a[0];
    cout << "\n---------- BOOKING RECEIPT ----------\n"
         << "Booking ID: " << r[0] << " | Status: " << r[7] << '\n'
         << "Customer: " << r[1] << " | Movie: " << r[2] << '\n'
         << "Theatre: " << r[3] << ", " << r[13] << " | Screen: " << r[11] << '\n'
         << "Show ID: " << r[10] << " | Language: " << r[12] << '\n'
         << "When: " << r[4] << " " << r[5] << " | Seats: " << r[9] << '\n'
         << "Amount: " << money(integer(r[6])) << '\n'
         << "-------------------------------------\n";
}
void App::myBookings(bool all){
    string sql="SELECT b.id FROM bookings b JOIN shows s ON s.id=b.show_id JOIN screens sc ON sc.id=s.screen_id JOIN theatres t ON t.id=sc.theatre_id WHERE ";
    vector<string> args;
    if(role=="ADMIN"&&all)sql+="1=1";
    else if(role=="OWNER"&&all){
        sql+="t.owner_id=?";
        args={
            to_string(uid)
        };
    }
    else{
        sql+="b.customer_id=?";
        args={
            to_string(uid)
        };
    }
    sql+=" ORDER BY b.id DESC";
    auto a=db.query(sql,args);
    cout<<"\nBOOKINGS\n";
    for(auto&r:a)showBooking(integer(r[0]));
    if(a.empty())cout<<"No bookings.\n";
}
void App::searchBooking() {
    int id = num("Booking ID: ", 1, 1000000);
    auto found = db.query("SELECT customer_id FROM bookings WHERE id=?", {to_string(id)});
    if (found.empty() || (role == "CUSTOMER" && integer(found[0][0]) != uid)) {
        cout << "Booking not found or access denied.\n";
        return;
    }
    if (role == "OWNER" && !isOwnerOfBooking(id)) {
        cout << "Access denied: another theatre.\n";
        return;
    }
    showBooking(id);
}
void App::payment(){
    int id=num("Booking ID for payment: ",1,1000000);
    auto a=db.query("SELECT b.total_paise,b.status FROM bookings b JOIN shows s ON s.id=b.show_id WHERE b.id=? AND b.customer_id=? AND s.status='SCHEDULED'",{
        to_string(id),to_string(uid)
    });
    if(a.empty()){
        cout<<"Your booking not found.\n";
        return;
    }
    if(a[0][1]!="PENDING"){
        cout<<"Only pending bookings can be paid.\n";
        return;
    }
    cout<<"Amount: "<<money(integer(a[0][0]))<<"\n1. UPI (simulation)\n2. Card (simulation)\n3. Cash (simulation)\n";
    int method=num("Method: ",1,3);
    if(upper(input("Simulate successful payment? y/n: "))!="Y"){
        cout<<"No payment recorded.\n";
        return;
    }
    try{
        db.begin();
        auto state=db.query("SELECT b.status FROM bookings b JOIN shows s ON s.id=b.show_id WHERE b.id=? AND b.customer_id=? AND s.status='SCHEDULED'",{
            to_string(id),to_string(uid)
        });
        if(state.empty()||state[0][0]!="PENDING")throw runtime_error("Booking no longer pending");
        db.change("INSERT INTO payments(booking_id,amount_paise,method) VALUES(?,?,?)",{
            to_string(id),a[0][0],method==1?"UPI_SIM":method==2?"CARD_SIM":"CASH_SIM"
        });
        db.change("UPDATE bookings SET status='PAID' WHERE id=?",{
            to_string(id)
        });
        db.commit();
        cout<<"SIMULATED PAYMENT SUCCESS. Ticket confirmed.\n";
        showBooking(id);
    }
    catch(const exception&e){
        db.rollback();
        cout<<"Payment failed: "<<e.what()<<'\n';
    }
}
void App::cancel(bool all){
    int id=num("Booking ID to cancel: ",1,1000000);
    auto a=db.query("SELECT customer_id,status,total_paise FROM bookings WHERE id=?",{
        to_string(id)
    });
    if(a.empty()){
        cout<<"Booking not found.\n";
        return;
    }
    if(!all&&integer(a[0][0])!=uid){
        cout<<"Access denied: not your booking.\n";
        return;
    }
    if(role=="OWNER"&&!isOwnerOfBooking(id)){
        cout<<"Access denied: another theatre.\n";
        return;
    }
    if(a[0][1]=="CANCELLED"){
        cout<<"Already cancelled.\n";
        return;
    }
    auto schedule=db.query("SELECT s.show_date,s.show_time FROM bookings b "
                           "JOIN shows s ON s.id=b.show_id WHERE b.id=?", {to_string(id)});
    if (schedule.empty() || !isFutureShow(schedule[0][0], schedule[0][1])) {
        cout << "Cancellation closed: the show has already started.\n";
        return;
    }
    if(upper(input("Confirm cancellation? y/n: "))!="Y")return;
    try{
        db.begin();
        auto valid=db.query("SELECT s.show_date,s.show_time FROM bookings b "
                            "JOIN shows s ON s.id=b.show_id WHERE b.id=?", {to_string(id)});
        if (valid.empty() || !isFutureShow(valid[0][0], valid[0][1]))
            throw runtime_error("Cancellation deadline has passed");
        db.change("DELETE FROM occupied_seats WHERE booking_id=?",{
            to_string(id)
        });
        db.change("UPDATE bookings SET status='CANCELLED' WHERE id=?",{
            to_string(id)
        });
        if(a[0][1]=="PAID")db.change("INSERT INTO refunds(booking_id,amount_paise) VALUES(?,?)",{
            to_string(id),a[0][2]
        });
        db.commit();
        cout<<(a[0][1]=="PAID"?"Booking cancelled, seats released, simulated refund recorded.\n":"Pending booking cancelled; seats released.\n");
    }
    catch(const exception&e){
        db.rollback();
        cout<<"Cancellation failed: "<<e.what()<<'\n';
    }
}
void App::payments(){
    string sql="SELECT p.id,p.booking_id,p.amount_paise,p.method,p.status FROM payments p JOIN bookings b ON b.id=p.booking_id JOIN shows s ON s.id=b.show_id JOIN screens sc ON sc.id=s.screen_id JOIN theatres t ON t.id=sc.theatre_id WHERE ";
    vector<string> args;
    if(role=="ADMIN")sql+="1=1";
    else if(role=="OWNER"){
        sql+="t.owner_id=?";
        args={
            to_string(uid)
        };
    }
    else{
        sql+="b.customer_id=?";
        args={
            to_string(uid)
        };
    }
    sql+=" ORDER BY p.id DESC";
    cout<<"\nPAYMENT HISTORY (SIMULATED)\n";
    for(auto&r:db.query(sql,args))cout<<"Payment "<<r[0]<<" | Booking "<<r[1]<<" | "<<money(integer(r[2]))<<" | "<<r[3]<<" | "<<r[4]<<'\n';
    sql="SELECT r.id,r.booking_id,r.amount_paise,r.status FROM refunds r JOIN bookings b ON b.id=r.booking_id JOIN shows s ON s.id=b.show_id JOIN screens sc ON sc.id=s.screen_id JOIN theatres t ON t.id=sc.theatre_id WHERE ";
    if(role=="ADMIN")sql+="1=1";
    else if(role=="OWNER")sql+="t.owner_id=?";
    else sql+="b.customer_id=?";
    sql+=" ORDER BY r.id DESC";
    cout<<"REFUNDS (SIMULATED)\n";
    for(auto&r:db.query(sql,args))cout<<"Refund "<<r[0]<<" | Booking "<<r[1]<<" | "<<money(integer(r[2]))<<" | "<<r[3]<<'\n';
}
void App::report(bool ownerOnly){
    string filter=ownerOnly?" WHERE t.owner_id=?":"";
    vector<string> args=ownerOnly?vector<string>{
        to_string(uid)
    }
    :vector<string>{
    };
    auto a=db.query("SELECT COUNT(*),COALESCE(SUM(CASE WHEN b.status='PAID' THEN b.total_paise ELSE 0 END),0),COALESCE(SUM(CASE WHEN b.status='CANCELLED' THEN 1 ELSE 0 END),0),COALESCE(SUM(CASE WHEN b.status='PENDING' THEN 1 ELSE 0 END),0) FROM bookings b JOIN shows s ON s.id=b.show_id JOIN screens sc ON sc.id=s.screen_id JOIN theatres t ON t.id=sc.theatre_id"+filter,args);
    cout<<"\nDASHBOARD / ANALYTICS\n";
    if(!a.empty())cout<<"All bookings: "<<a[0][0]<<" | Paid & active revenue: "<<money(integer(a[0][1]))<<" | Cancelled: "<<a[0][2]<<" | Pending: "<<a[0][3]<<'\n';
    auto b=db.query(string("SELECT COUNT(*) FROM theatres t")+(ownerOnly?" WHERE t.owner_id=?":""),args);
    cout<<"Theatres: "<<b[0][0]<<'\n';
    auto s=db.query("SELECT COUNT(*),COALESCE(SUM(sc.rows_count*sc.cols_count),0) FROM shows s JOIN screens sc ON sc.id=s.screen_id JOIN theatres t ON t.id=sc.theatre_id"+filter,args);
    auto occupied=db.query("SELECT COUNT(*) FROM occupied_seats o JOIN shows s ON s.id=o.show_id JOIN screens sc ON sc.id=s.screen_id JOIN theatres t ON t.id=sc.theatre_id"+filter,args);
    cout<<"Shows: "<<s[0][0]<<" | Total show seats: "<<s[0][1]<<" | Occupied: "<<occupied[0][0]<<" | Available: "<<integer(s[0][1])-integer(occupied[0][0])<<'\n';
}
void App::profile(){
    auto a=db.query("SELECT username,display_name,role FROM accounts WHERE id=?",{
        to_string(uid)
    });
    if(a.empty())return;
    cout<<"Username: "<<a[0][0]<<" | Name: "<<a[0][1]<<" | Role: "<<a[0][2]<<'\n';
    string name=input("New display name (blank = no change): ");
    if(!name.empty()){
        if(name.size()>70){
            cout<<"Name too long.\n";
            return;
        }
        db.change("UPDATE accounts SET display_name=? WHERE id=?",{
            name,to_string(uid)
        });
        cout<<"Profile updated.\n";
    }
}
void App::movieManagement() {
    while (true) {
        cout << "\n====== MOVIE MANAGEMENT ======\n"
             << "1. Add Movies (Bulk)\n"
             << "2. View Movies\n"
             << "3. Search Movies\n"
             << "4. Update Movie Details\n"
             << "5. Remove Movie (Deactivate)\n"
             << "6. Back\n"
             << "7. Reactivate Movie\n";
        int choice = num("Choose: ", 1, 7);
        switch (choice) {
            case 1: addMovie(); break;
            case 2: movies(); break;
            case 3: movies(true); break;
            case 4: updateMovie(); break;
            case 5: removeMovie(); break;
            case 6: return;
            case 7: reactivateMovie(); break;
        }
    }
}

void App::reactivateMovie() {
    auto inactive = db.query("SELECT id,title FROM movies WHERE active=0 ORDER BY id");
    if (inactive.empty()) {
        cout << "No inactive movies.\n";
        return;
    }
    for (const auto& movie : inactive)
        cout << "ID " << movie[0] << " | " << movie[1] << '\n';
    int id = num("Movie ID to reactivate: ", 1, 1000000);
    if (db.change("UPDATE movies SET active=1 WHERE id=? AND active=0", {to_string(id)}))
        cout << "Movie reactivated.\n";
    else cout << "Inactive movie not found.\n";
}

void App::updateMovie() {
    movies();
    int id = num("Movie ID to update: ", 1, 1000000);
    auto found = db.query("SELECT id FROM movies WHERE id=? AND active=1", {to_string(id)});
    if (found.empty()) {
        cout << "Active movie not found.\n";
        return;
    }
    string title = input("New title: ");
    string genre = input("New genre: ");
    int duration = num("New duration (minutes): ", 30, 400);
    if (title.empty() || title.size() > 100 || genre.empty() || genre.size() > 50) {
        cout << "Invalid movie information.\n";
        return;
    }
    db.change("UPDATE movies SET title=?,genre=?,duration_minutes=? WHERE id=? AND active=1",
              {title, genre, to_string(duration), to_string(id)});
    cout << "Movie details updated. Existing languages are unchanged.\n";
}

void App::removeMovie() {
    movies();
    int id = num("Movie ID to remove: ", 1, 1000000);
    auto found = db.query("SELECT title FROM movies WHERE id=? AND active=1", {to_string(id)});
    if (found.empty()) {
        cout << "Active movie not found.\n";
        return;
    }
    auto scheduled = db.query(
        "SELECT COUNT(*) FROM shows WHERE movie_id=? AND status='SCHEDULED'",
        {to_string(id)});
    if (integer(scheduled[0][0]) > 0) {
        cout << "Cannot remove movie: it has scheduled shows. Cancel them first.\n";
        return;
    }
    if (upper(input("Deactivate '" + found[0][0] + "'? y/n: ")) != "Y") return;
    db.change("UPDATE movies SET active=0 WHERE id=?", {to_string(id)});
    cout << "Movie removed from active catalog. Past booking records are preserved.\n";
}

void App::showManagement() {
    while (true) {
        cout << "\n====== SHOW MANAGEMENT ======\n"
             << "1. Add Show\n"
             << "2. View My Shows (Including Cancelled)\n"
             << "3. Cancel Show\n"
             << "4. Replace Cancelled Show\n"
             << "5. Back\n";
        int choice = num("Choose: ", 1, 5);
        switch (choice) {
            case 1: addShow(); break;
            case 2: shows(uid); break;
            case 3: cancelShow(); break;
            case 4: replaceCancelledShow(); break;
            case 5: return;
        }
    }
}

void App::cancelShow() {
    shows(uid);
    int showId = num("Show ID to cancel: ", 1, 1000000);
    auto info = db.query(
        "SELECT s.id, m.title, s.show_date, s.show_time, s.status "
        "FROM shows s "
        "JOIN movies m ON m.id=s.movie_id "
        "JOIN screens sc ON sc.id=s.screen_id "
        "JOIN theatres t ON t.id=sc.theatre_id "
        "WHERE s.id=? AND t.owner_id=?",
        {to_string(showId), to_string(uid)});
    if (info.empty()) {
        cout << "Access denied or show not found.\n";
        return;
    }
    if (info[0][4] != "SCHEDULED") {
        cout << "This show is already cancelled.\n";
        return;
    }
    cout << "Cancel " << info[0][1] << " at " << info[0][2]
         << " " << info[0][3] << "?\n"
         << "All pending/paid bookings will be cancelled; paid bookings will receive "
         << "simulated refunds.\n";
    if (upper(input("Confirm cancellation? y/n: ")) != "Y") return;

    try {
        db.begin();
        // Recheck inside the write transaction to prevent a race with booking.
        auto state = db.query("SELECT status FROM shows WHERE id=?", {to_string(showId)});
        if (state.empty() || state[0][0] != "SCHEDULED")
            throw runtime_error("Show is no longer scheduled");

        auto bookings = db.query(
            "SELECT id, status, total_paise FROM bookings "
            "WHERE show_id=? AND status IN ('PAID','PENDING')",
            {to_string(showId)});
        for (const auto& booking : bookings) {
            db.change("DELETE FROM occupied_seats WHERE booking_id=?", {booking[0]});
            db.change("UPDATE bookings SET status='CANCELLED' WHERE id=?", {booking[0]});
            if (booking[1] == "PAID") {
                db.change(
                    "INSERT OR IGNORE INTO refunds(booking_id,amount_paise) VALUES(?,?)",
                    {booking[0], booking[2]});
            }
        }
        db.change("UPDATE shows SET status='CANCELLED' WHERE id=?", {to_string(showId)});
        db.commit();
        cout << "Show cancelled. " << bookings.size()
             << " bookings cancelled; paid bookings refunded (simulation).\n";
    } catch (const exception& e) {
        db.rollback();
        cout << "Show cancellation failed: " << e.what() << '\n';
    }
}

void App::replaceCancelledShow() {
    shows(uid);
    int oldId = num("Cancelled Show ID to replace: ", 1, 1000000);
    auto old = db.query(
        "SELECT s.screen_id, s.show_date, s.show_time, s.status "
        "FROM shows s "
        "JOIN screens sc ON sc.id=s.screen_id "
        "JOIN theatres t ON t.id=sc.theatre_id "
        "WHERE s.id=? AND t.owner_id=? AND t.active=1",
        {to_string(oldId), to_string(uid)});
    if (old.empty()) {
        cout << "Access denied or show not found.\n";
        return;
    }
    if (old[0][3] != "CANCELLED") {
        cout << "Cancel the original show first.\n";
        return;
    }
    auto previous = db.query(
        "SELECT id FROM shows WHERE replacement_for_show_id=?",
        {to_string(oldId)});
    if (!previous.empty()) {
        cout << "A replacement has already been created: Show ID " << previous[0][0] << '\n';
        return;
    }

    cout << "Replacement keeps screen and time: " << old[0][1]
         << " " << old[0][2] << "\n";
    movies();
    int movieId = num("New Movie ID: ", 1, 1000000);
    auto languages = db.query(
        "SELECT ml.language FROM movie_languages ml "
        "JOIN movies m ON m.id=ml.movie_id "
        "WHERE ml.movie_id=? AND m.active=1 ORDER BY ml.language",
        {to_string(movieId)});
    if (languages.empty()) {
        cout << "Movie not found or it has no registered languages.\n";
        return;
    }
    cout << "Available languages:\n";
    for (size_t i = 0; i < languages.size(); ++i)
        cout << i + 1 << ". " << languages[i][0] << '\n';
    int languageIndex = num("Choose language: ", 1, (int)languages.size());
    int price = num("New ticket price (rupees): ", 1, 10000);
    if (upper(input("Create replacement show? y/n: ")) != "Y") return;

    try {
        db.begin();
        auto state = db.query("SELECT status FROM shows WHERE id=?", {to_string(oldId)});
        if (state.empty() || state[0][0] != "CANCELLED")
            throw runtime_error("Original show is not cancelled");
        if (!isFutureShow(old[0][1], old[0][2]))
            throw runtime_error("Cannot replace a past show");
        if (hasScreenOverlap(db, integer(old[0][0]), old[0][1], old[0][2], movieId))
            throw runtime_error("Replacement overlaps an existing show");
        db.change(
            "INSERT INTO shows(movie_id,screen_id,show_date,show_time,"
            "price_paise,language,replacement_for_show_id) "
            "VALUES(?,?,?,?,?,?,?)",
            {to_string(movieId), old[0][0], old[0][1], old[0][2],
             to_string(price * 100), languages[languageIndex - 1][0], to_string(oldId)});
        int newId = db.id();
        db.commit();
        cout << "Replacement created! New Show ID: " << newId << "\n"
             << "Original cancelled Show ID " << oldId << " is preserved.\n";
    } catch (const exception& e) {
        db.rollback();
        cout << "Replacement failed (slot occupied or other error): " << e.what() << '\n';
    }
}

void App::admin(){
    while(uid){
        cout<<"\n====== PLATFORM ADMIN ======\n1. Dashboard / Reports\n2. Movie Management\n3. Theatre Owner Approvals / Management\n4. Customer Management\n5. View Movies\n6. View Shows\n7. All Bookings\n8. All Payments / Refunds\n9. Cancel Booking\n10. Logout\n11. Account Security\n";
        int c=num("Choose: ",1,11);
        switch(c){
            case 11:accountSecurity();
            break;
            case 1:report(false);
            break;
            case 2:movieManagement();
            break;
            case 3:owners();
            break;
            case 4:customers();
            break;
            case 5:movies();
            break;
            case 6:shows();
            break;
            case 7:myBookings(true);
            break;
            case 8:payments();
            break;
            case 9:cancel(true);
            break;
            case 10:logout();
            break;
        }
    }
}
void App::owner(){
    if(!ownerApproved()){
        cout<<"Owner not approved.\n";
        logout();
        return;
    }
    while(uid){
        cout<<"\n====== THEATRE OWNER ======\n1. My Theatres (View / Add)\n2. Screen Management\n3. Show Management (Add / Cancel / Replace)\n4. Ticket Pricing (Update)\n5. View Movies\n6. View My Shows\n7. My Theatre Bookings\n8. My Payments / Refunds\n9. My Reports\n10. Logout\n11. Account Security\n";
        int c=num("Choose: ",1,11);
        switch(c){
            case 11:accountSecurity();
            break;
            case 1:theatres();
            break;
            case 2:screens();
            break;
            case 3:showManagement();
            break;
            case 4:changePrice();
            break;
            case 5:movies();
            break;
            case 6:shows(uid);
            break;
            case 7:myBookings(true);
            break;
            case 8:payments();
            break;
            case 9:report(true);
            break;
            case 10:logout();
            break;
        }
    }
}
void App::customer(){
    while(uid){
        cout<<"\n====== CUSTOMER PANEL ======\n1. View Movies\n2. Search Movies\n3. View Movie Details\n4. View Available Shows\n5. View Seat Layout\n6. Book Tickets\n7. Make Payment (Simulation)\n8. View My Bookings\n9. Cancel My Booking\n10. Payment / Refund History\n11. Manage Profile\n12. Logout\n13. Account Security\n14. Search My Booking / Receipt\n";
        int c=num("Choose: ",1,14);
        switch(c){
            case 13:accountSecurity();
            break;
            case 14:searchBooking();
            break;
            case 1:movies();
            break;
            case 2:movies(true);
            break;
            case 3:details();
            break;
            case 4:shows();
            break;
            case 5:seats();
            break;
            case 6:booking();
            break;
            case 7:payment();
            break;
            case 8:myBookings();
            break;
            case 9:cancel();
            break;
            case 10:payments();
            break;
            case 11:profile();
            break;
            case 12:logout();
            break;
        }
    }
}
