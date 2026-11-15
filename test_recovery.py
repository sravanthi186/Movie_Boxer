#!/usr/bin/env python3
"""Authentication recovery smoke test on a throwaway database."""
import pathlib
import re
import sqlite3
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]

def run(folder, data):
    proc = subprocess.run([str(ROOT / 'movie_booking')], input='\n'.join(map(str, data))+'\n', text=True, cwd=folder, capture_output=True, timeout=20)
    assert proc.returncode == 0, proc.stdout + proc.stderr
    return proc.stdout

with tempfile.TemporaryDirectory() as temp:
    work=pathlib.Path(temp)
    (work/'data').mkdir()
    out=run(work,['main_admin','AdminPwd123','AdminPwd123',3])
    admin_code=re.search(r'YOUR RECOVERY CODE \(SAVE IT NOW\): ([0-9a-f]{32})',out).group(1)
    assert 'administrator created' in out
    out=run(work,[2,1,'customer1','Customer One','CustomerPwd123','CustomerPwd123',3])
    user_code=re.search(r'YOUR RECOVERY CODE \(SAVE IT NOW\): ([0-9a-f]{32})',out).group(1)
    assert admin_code != user_code
    out=run(work,[1,2,'customer1','wrong-code',3,3]); assert 'Invalid username or recovery code.' in out
    out=run(work,[1,2,'customer1',user_code,'NewCustomerPwd123','NewCustomerPwd123',3,3]); assert 'Password updated successfully' in out
    new_code=re.search(r'NEW RECOVERY CODE \(SAVE IT NOW\): ([0-9a-f]{32})',out).group(1)
    out=run(work,[1,1,'customer1','CustomerPwd123',3]); assert 'Invalid username or password.' in out
    out=run(work,[1,1,'customer1','NewCustomerPwd123',12,3]); assert 'Welcome customer1 (CUSTOMER)' in out
    out=run(work,[1,2,'customer1',user_code,3,3]); assert 'Invalid username or recovery code.' in out
    out=run(work,[1,2,'main_admin',admin_code,'UpdatedAdminPwd123','UpdatedAdminPwd123',3,3]); assert 'Password updated successfully' in out
    # Existing account without recovery code (simulating a legacy DB): security menu can issue one.
    con=sqlite3.connect(work/'data/cinema.db')
    con.execute('DELETE FROM account_recovery WHERE account_id=(SELECT id FROM accounts WHERE username=?)',('customer1',))
    con.commit(); con.close()
    out=run(work,[1,1,'customer1','NewCustomerPwd123',13,2,'NewCustomerPwd123',12,3]); assert 'YOUR RECOVERY CODE' in out
    con=sqlite3.connect(work/'data/cinema.db')
    assert con.execute('SELECT COUNT(*) FROM account_recovery').fetchone()[0]==2
    assert len(con.execute('SELECT code_hash FROM account_recovery').fetchone()[0])==64
    assert con.execute('PRAGMA foreign_key_check').fetchall()==[]
    con.close()
    print('PASS: nested login, invalid code, customer/admin reset, code rotation, old password rejection, legacy recovery setup.')
