#include "Database.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <stdexcept>
#include <vector>

namespace {
class Stmt {
 sqlite3_stmt* p=nullptr;
public:
 Stmt(sqlite3* db,const std::string& sql){if(sqlite3_prepare_v2(db,sql.c_str(),-1,&p,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db));}
 ~Stmt(){sqlite3_finalize(p);} sqlite3_stmt* get(){return p;}
};
std::string hex(const unsigned char* data,size_t n){static const char* h="0123456789abcdef";std::string r;for(size_t i=0;i<n;i++){r+=h[data[i]>>4];r+=h[data[i]&15];}return r;}
std::vector<unsigned char> unhex(const std::string& s){std::vector<unsigned char> a;if(s.size()%2)return a;for(size_t i=0;i<s.size();i+=2){auto f=[](char x)->int {if(x>='0'&&x<='9')return x-'0';if(x>='a'&&x<='f')return x-'a'+10;return -1;};int x=f(s[i]),y=f(s[i+1]);if(x<0||y<0)return {};a.push_back((unsigned char)(16*x+y));}return a;}
void bind(sqlite3_stmt* p,const std::vector<std::string>& args){for(size_t i=0;i<args.size();i++)sqlite3_bind_text(p,(int)i+1,args[i].c_str(),-1,SQLITE_TRANSIENT);}
}
Database::Database(const std::string& path){if(sqlite3_open(path.c_str(),&db)!=SQLITE_OK){std::string e=db?sqlite3_errmsg(db):"unknown";if(db)sqlite3_close(db);db=nullptr;throw std::runtime_error("Database open failed: "+e);}sqlite3_busy_timeout(db,5000);schema();}
Database::~Database(){if(db)sqlite3_close(db);}
void Database::exec(const std::string& sql){char* err=nullptr;if(sqlite3_exec(db,sql.c_str(),nullptr,nullptr,&err)!=SQLITE_OK){std::string e=err?err:"SQLite error";sqlite3_free(err);throw std::runtime_error(e);}}
std::vector<Row> Database::query(const std::string& sql,const std::vector<std::string>& args){Stmt s(db,sql);bind(s.get(),args);std::vector<Row> result;int rc;while((rc=sqlite3_step(s.get()))==SQLITE_ROW){Row row;for(int i=0;i<sqlite3_column_count(s.get());i++){const char* v=(const char*)sqlite3_column_text(s.get(),i);row.values.push_back(v?v:"");}result.push_back(row);}if(rc!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(db));return result;}
int Database::change(const std::string& sql,const std::vector<std::string>& args){Stmt s(db,sql);bind(s.get(),args);if(sqlite3_step(s.get())!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(db));return sqlite3_changes(db);}
int Database::id()const{return (int)sqlite3_last_insert_rowid(db);}
void Database::begin(){exec("BEGIN IMMEDIATE");}void Database::commit(){exec("COMMIT");}void Database::rollback(){sqlite3_exec(db,"ROLLBACK",nullptr,nullptr,nullptr);}
std::string newSalt(){unsigned char b[16];if(RAND_bytes(b,sizeof b)!=1)throw std::runtime_error("Random salt generation failed");return hex(b,sizeof b);}
std::string passwordHash(const std::string& password,const std::string& saltHex){auto salt=unhex(saltHex);if(salt.size()!=16)throw std::runtime_error("Invalid salt");unsigned char digest[32];if(PKCS5_PBKDF2_HMAC(password.c_str(),(int)password.size(),salt.data(),(int)salt.size(),210000,EVP_sha256(),32,digest)!=1)throw std::runtime_error("Password hash failure");return hex(digest,32);}
bool passwordMatches(const std::string& password,const std::string& salt,const std::string& hash){auto a=unhex(passwordHash(password,salt)),b=unhex(hash);return a.size()==32&&b.size()==32&&CRYPTO_memcmp(a.data(),b.data(),32)==0;}
void Database::schema(){exec(R"SQL(
PRAGMA foreign_keys=ON;
CREATE TABLE IF NOT EXISTS accounts (
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 username TEXT NOT NULL COLLATE NOCASE UNIQUE,
 salt TEXT NOT NULL,password_hash TEXT NOT NULL,
 role TEXT NOT NULL CHECK(role IN ('ADMIN','OWNER','CUSTOMER')),
 approved INTEGER NOT NULL DEFAULT 0 CHECK(approved IN(0,1)),
 active INTEGER NOT NULL DEFAULT 1 CHECK(active IN(0,1)),
 display_name TEXT NOT NULL DEFAULT '',
 created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE UNIQUE INDEX IF NOT EXISTS one_admin ON accounts(role) WHERE role='ADMIN';
CREATE TABLE IF NOT EXISTS movies (
 id INTEGER PRIMARY KEY AUTOINCREMENT,title TEXT NOT NULL,genre TEXT NOT NULL,
 duration_minutes INTEGER NOT NULL CHECK(duration_minutes BETWEEN 30 AND 400),active INTEGER NOT NULL DEFAULT 1
);
CREATE TABLE IF NOT EXISTS movie_languages (
 movie_id INTEGER NOT NULL REFERENCES movies(id) ON DELETE CASCADE,
 language TEXT NOT NULL COLLATE NOCASE,
 PRIMARY KEY(movie_id,language)
);
CREATE TABLE IF NOT EXISTS theatres (
 id INTEGER PRIMARY KEY AUTOINCREMENT,owner_id INTEGER NOT NULL REFERENCES accounts(id),
 name TEXT NOT NULL,city TEXT NOT NULL,active INTEGER NOT NULL DEFAULT 1,
 UNIQUE(owner_id,name,city)
);
CREATE TABLE IF NOT EXISTS screens (
 id INTEGER PRIMARY KEY AUTOINCREMENT,theatre_id INTEGER NOT NULL REFERENCES theatres(id),
 name TEXT NOT NULL,rows_count INTEGER NOT NULL CHECK(rows_count BETWEEN 1 AND 8),
 cols_count INTEGER NOT NULL CHECK(cols_count BETWEEN 1 AND 10),
 UNIQUE(theatre_id,name)
);
CREATE TABLE IF NOT EXISTS shows (
 id INTEGER PRIMARY KEY AUTOINCREMENT,movie_id INTEGER NOT NULL REFERENCES movies(id),
 screen_id INTEGER NOT NULL REFERENCES screens(id),show_date TEXT NOT NULL,show_time TEXT NOT NULL,
 price_paise INTEGER NOT NULL CHECK(price_paise BETWEEN 100 AND 1000000),
 language TEXT NOT NULL DEFAULT '',
 status TEXT NOT NULL DEFAULT 'SCHEDULED' CHECK(status IN ('SCHEDULED','CANCELLED')),
 replacement_for_show_id INTEGER
);
CREATE TABLE IF NOT EXISTS bookings (
 id INTEGER PRIMARY KEY AUTOINCREMENT,customer_id INTEGER NOT NULL REFERENCES accounts(id),
 show_id INTEGER NOT NULL REFERENCES shows(id),status TEXT NOT NULL DEFAULT 'PENDING' CHECK(status IN('PENDING','PAID','CANCELLED')),
 total_paise INTEGER NOT NULL CHECK(total_paise>=0),created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS booking_seats (
 booking_id INTEGER NOT NULL REFERENCES bookings(id),seat TEXT NOT NULL,PRIMARY KEY(booking_id,seat)
);
CREATE TABLE IF NOT EXISTS occupied_seats (
 show_id INTEGER NOT NULL REFERENCES shows(id),seat TEXT NOT NULL,booking_id INTEGER NOT NULL REFERENCES bookings(id),
 PRIMARY KEY(show_id,seat)
);
CREATE TABLE IF NOT EXISTS payments (
 id INTEGER PRIMARY KEY AUTOINCREMENT,booking_id INTEGER NOT NULL UNIQUE REFERENCES bookings(id),
 amount_paise INTEGER NOT NULL,method TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'SUCCESS',
 paid_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS refunds (
 id INTEGER PRIMARY KEY AUTOINCREMENT,booking_id INTEGER NOT NULL UNIQUE REFERENCES bookings(id),
 amount_paise INTEGER NOT NULL,status TEXT NOT NULL DEFAULT 'SIMULATED',
 created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS booking_customer ON bookings(customer_id);
CREATE INDEX IF NOT EXISTS show_screen ON shows(screen_id);
)SQL");
 // Recovery details are stored separately so older account databases migrate safely.
 exec("CREATE TABLE IF NOT EXISTS account_recovery ("
      "account_id INTEGER PRIMARY KEY REFERENCES accounts(id) ON DELETE CASCADE,"
      "salt TEXT NOT NULL,code_hash TEXT NOT NULL)");
 // Legacy databases used a table-level UNIQUE constraint which blocks replacement
 // shows at the original screen and time, even if the old show was cancelled.
 // Rebuild only that table once, preserving show IDs and all booking references.
 auto table = query("SELECT sql FROM sqlite_master WHERE type='table' AND name='shows'");
 bool oldUnique = !table.empty() &&
     table[0][0].find("UNIQUE(screen_id,show_date,show_time)") != std::string::npos;
 if (oldUnique) {
     exec("PRAGMA foreign_keys=OFF");
     try {
         begin();
         exec("CREATE TABLE shows_migrated ("
              "id INTEGER PRIMARY KEY AUTOINCREMENT,"
              "movie_id INTEGER NOT NULL REFERENCES movies(id),"
              "screen_id INTEGER NOT NULL REFERENCES screens(id),"
              "show_date TEXT NOT NULL,show_time TEXT NOT NULL,"
              "price_paise INTEGER NOT NULL CHECK(price_paise BETWEEN 100 AND 1000000),"
              "language TEXT NOT NULL DEFAULT '',"
              "status TEXT NOT NULL DEFAULT 'SCHEDULED' CHECK(status IN ('SCHEDULED','CANCELLED')) ,"
              "replacement_for_show_id INTEGER)");
         auto columns = query("PRAGMA table_info(shows)");
         bool hasLanguage = false;
         for (const auto& c : columns) if (c[1] == "language") hasLanguage = true;
         exec(std::string("INSERT INTO shows_migrated "
                          "(id,movie_id,screen_id,show_date,show_time,price_paise,language) "
                          "SELECT id,movie_id,screen_id,show_date,show_time,price_paise,") +
              (hasLanguage ? "language" : "''") + " FROM shows");
         exec("DROP TABLE shows");
         exec("ALTER TABLE shows_migrated RENAME TO shows");
         commit();
     } catch (...) {
         rollback();
         exec("PRAGMA foreign_keys=ON");
         throw;
     }
     exec("PRAGMA foreign_keys=ON");
 }
 // Idempotent upgrade for any intermediate project database.
 auto cols = query("PRAGMA table_info(shows)");
 bool hasLanguage = false, hasStatus = false, hasReplacement = false;
 for (const auto& c : cols) {
     if (c[1] == "language") hasLanguage = true;
     if (c[1] == "status") hasStatus = true;
     if (c[1] == "replacement_for_show_id") hasReplacement = true;
 }
 if (!hasLanguage) exec("ALTER TABLE shows ADD COLUMN language TEXT NOT NULL DEFAULT ''");
 if (!hasStatus) exec("ALTER TABLE shows ADD COLUMN status TEXT NOT NULL DEFAULT 'SCHEDULED'");
 if (!hasReplacement) exec("ALTER TABLE shows ADD COLUMN replacement_for_show_id INTEGER");
 exec("CREATE INDEX IF NOT EXISTS show_screen ON shows(screen_id)");
 exec("CREATE UNIQUE INDEX IF NOT EXISTS only_scheduled_screen_slot "
      "ON shows(screen_id,show_date,show_time) WHERE status='SCHEDULED'");
 exec("CREATE UNIQUE INDEX IF NOT EXISTS only_one_replacement_per_show "
      "ON shows(replacement_for_show_id) WHERE replacement_for_show_id IS NOT NULL");
 auto bad = query("PRAGMA foreign_key_check");
 if (!bad.empty()) throw std::runtime_error("Database migration failed foreign-key check");
}
