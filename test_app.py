#!/usr/bin/env python3
"""End-to-end CLI regression test on isolated SQLite database. No test passwords in deliverable database."""
import os, sqlite3, subprocess, tempfile, pathlib
ROOT=pathlib.Path(__file__).resolve().parents[1]
APP=ROOT/'movie_booking'

def run(folder, steps):
    p=subprocess.run([str(APP)],input='\n'.join(map(str,steps))+'\n',cwd=folder,text=True,capture_output=True,timeout=30)
    if p.returncode:
        raise AssertionError(f'exit={p.returncode}\nSTDOUT:\n{p.stdout[-4000:]}\nSTDERR:\n{p.stderr}')
    return p.stdout

def verify(value, output):
    assert value in output, f'Missing {value!r} in\n{output[-3500:]}'

with tempfile.TemporaryDirectory() as t:
    d=pathlib.Path(t);(d/'data').mkdir()
    out=run(d,['platform_admin','AdminPass123','AdminPass123',3]); verify('administrator created',out)
    out=run(d,[2,2,'owner_a','Cinema Owner A','OwnerPass123','OwnerPass123',2,2,'owner_b','Cinema Owner B','OwnerPass456','OwnerPass456',2,1,'customer_a','Customer A','CustomerPass123','CustomerPass123',2,1,'customer_b','Customer B','CustomerPass456','CustomerPass456',3]);verify('Owner request submitted',out);verify('Customer registered',out)
    out=run(d,[1,1,'owner_a','OwnerPass123',3]);verify('pending admin approval',out)
    out=run(d,[1,1,'platform_admin','AdminPass123',3,1,2,2,2,3,4,2,1,1,'Interstellar Demo','Sci Fi',165,2,'English','Hindi',6,10,3]);verify('Approved.',out);verify('Movie added!',out)
    out=run(d,[1,1,'owner_a','OwnerPass123',1,'Cinema Alpha','Hyderabad',2,1,'Screen One',5,8,3,1,1,1,1,'2026-12-16','18:30',250,5,6,10,3]);verify('Theatre added.',out);verify('Screen added.',out);verify('Show added in English.',out)
    out=run(d,[1,1,'owner_b','OwnerPass456',1,'Cinema Beta','Guntur',2,2,'Screen Main',4,6,3,1,1,1,2,'2026-12-16','19:00',180,5,6,4,1,300,10,3]);verify('Access denied or show not found.',out);verify('Show added in English.',out)
    out=run(d,[1,1,'customer_a','CustomerPass123',6,1,'A1,A2','y',7,1,1,'y',8,12,3]);verify('RESERVATION CREATED.',out);verify('SIMULATED PAYMENT SUCCESS.',out)
    out=run(d,[1,1,'customer_b','CustomerPass456',6,1,'A1','y',8,9,1,12,3]);verify('already reserved',out);verify('Access denied: not your booking.',out)
    out=run(d,[1,1,'customer_a','CustomerPass123',9,1,'y',10,12,3]);verify('simulated refund recorded',out)
    out=run(d,[1,1,'customer_b','CustomerPass456',6,1,'A1','y',8,12,3]);verify('RESERVATION CREATED.',out)
    # Admin can add multiple titles without reopening the menu.
    out=run(d,[1,1,'platform_admin','AdminPass123',2,1,2,'Movie Two','Comedy',120,3,'Telugu','Tamil','Hindi','Movie Three','Action',140,1,'English',6,10,3]);verify('2 of 2 movies added',out)
    # Owner B cancels their own scheduled show, including a paid customer ticket.
    out=run(d,[1,1,'customer_a','CustomerPass123',6,2,'A1','y',7,3,1,'y',12,3])
    verify('SIMULATED PAYMENT SUCCESS.',out)
    out=run(d,[1,1,'owner_a','OwnerPass123',3,3,2,5,10,3])
    verify('Access denied or show not found.',out)
    out=run(d,[1,1,'owner_b','OwnerPass456',3,3,2,'y',4,2,2,1,240,'y',5,10,3])
    verify('Show cancelled.',out)
    verify('Replacement created!',out)
    # Old show cannot accept new bookings after cancellation; replacement can.
    out=run(d,[1,1,'customer_b','CustomerPass456',6,2,12,3])
    verify('Show unavailable.',out)
    out=run(d,[1,1,'customer_b','CustomerPass456',6,3,'A1','y',12,3])
    verify('RESERVATION CREATED.',out)
    # Admin cannot deactivate a movie which still has scheduled shows.
    out=run(d,[1,1,'platform_admin','AdminPass123',2,5,1,6,10,3])
    verify('Cannot remove movie:',out)
    con=sqlite3.connect(d/'data/cinema.db')
    assert con.execute('select count(*) from movies').fetchone()[0]==3
    assert con.execute('select count(*) from movie_languages').fetchone()[0]==6
    assert con.execute('select language from shows where id=1').fetchone()[0]=='English'
    assert con.execute('select language from shows where id=2').fetchone()[0]=='English'
    assert con.execute("select count(*) from accounts where role='ADMIN'").fetchone()[0]==1
    assert con.execute("select count(*) from accounts where role='OWNER' and approved=1").fetchone()[0]==2
    assert con.execute("select count(*) from accounts where role='CUSTOMER'").fetchone()[0]==2
    assert con.execute("select count(*) from theatres").fetchone()[0]==2
    assert con.execute("select count(*) from shows").fetchone()[0]==3
    assert con.execute("select status from bookings where id=1").fetchone()[0]=='CANCELLED'
    assert con.execute("select status from bookings where id=2").fetchone()[0]=='PENDING'
    assert con.execute("select count(*) from payments").fetchone()[0]==2
    assert con.execute("select count(*) from refunds").fetchone()[0]==2
    assert con.execute("select count(*) from occupied_seats").fetchone()[0]==2
    assert con.execute("select password_hash != ? from accounts limit 1",('AdminPass123',)).fetchone()[0]==1
    assert con.execute("SELECT status FROM shows WHERE id=2").fetchone()[0]=='CANCELLED'
    assert con.execute("SELECT replacement_for_show_id FROM shows WHERE id=3").fetchone()[0]==2
    assert con.execute("PRAGMA foreign_key_check").fetchall()==[]
    print('PASS: admin setup, owner approval, two owners, isolation, bookings, double-book prevention, payments, cancellation, refunds, seat reuse, persistence, show cancellation/replacement, permissions.')
