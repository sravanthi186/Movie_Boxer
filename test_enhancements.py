#!/usr/bin/env python3
"""Regression tests for theatre scheduling, ticket checks and new admin/customer menus."""
import pathlib
import sqlite3
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
EXECUTABLE = ROOT / 'movie_booking'


def play(directory, choices):
    result = subprocess.run([str(EXECUTABLE)], cwd=directory,
                            input='\n'.join(map(str, choices)) + '\n',
                            text=True, capture_output=True, timeout=30)
    assert result.returncode == 0, result.stdout[-2000:] + result.stderr
    return result.stdout


with tempfile.TemporaryDirectory() as directory:
    work = pathlib.Path(directory)
    (work / 'data').mkdir()
    assert 'administrator created' in play(work, ['root_admin', 'RootPass123', 'RootPass123', 3])
    assert 'Owner request submitted' in play(work, [2, 2, 'owner_x', 'Owner X', 'OwnerPass123', 'OwnerPass123', 3])
    out = play(work, [1, 1, 'root_admin', 'RootPass123', 3, 2, 2, 4, 2, 1, 1, 'First Movie', 'Drama', 180, 1, 'Telugu',
                      6, 10, 3])
    assert 'Approved.' in out and 'Movie added!' in out
    out = play(work, [1, 1, 'owner_x', 'OwnerPass123',
                      1, 'Test Cinema', 'Guntur',
                      2, 1, 'Screen A', 3, 8,
                      3, 1, 1, 1, 1, '2030-12-16', '18:00', 200,
                      1, 1, 1, 1, '2030-12-16', '19:00', 200,
                      1, 1, 1, 1, '2020-01-01', '18:00', 200,
                      5, 10, 3])
    assert 'Show added in Telugu.' in out
    assert 'Screen conflict:' in out
    assert 'Choose a valid future show time.' in out
    connection = sqlite3.connect(work / 'data' / 'cinema.db')
    assert connection.execute('SELECT COUNT(*) FROM shows').fetchone()[0] == 1
    assert 'Customer registered' in play(work, [2, 1, 'customer_x', 'Customer X', 'CustomerPass123', 'CustomerPass123', 3])
    out = play(work, [1, 1, 'customer_x', 'CustomerPass123',
                      6, 1, 'A1,A2,A3,A4,A5,A6,A7',
                      6, 1, 'A1', 'y',
                      14, 1, 12, 3])
    assert 'Select 1-6 unique seats.' in out
    assert 'RESERVATION CREATED.' in out
    assert 'BOOKING RECEIPT' in out and 'Language: Telugu' in out
    connection.execute("UPDATE shows SET show_date='2020-01-01' WHERE id=1")
    connection.commit()
    out = play(work, [1, 1, 'customer_x', 'CustomerPass123', 9, 1, 6, 1, 12, 3])
    assert 'Cancellation closed:' in out
    assert 'Booking closed:' in out
    # Movie without active scheduled shows may be deactivated and later reactivated.
    out = play(work, [1, 1, 'root_admin', 'RootPass123',
                      2, 1, 1, 'Second Movie', 'Comedy', 110, 1, 'English',
                      5, 2, 'y', 7, 2, 6, 10, 3])
    assert 'Movie removed from active catalog' in out
    assert 'Movie reactivated.' in out
    assert connection.execute('SELECT active FROM movies WHERE id=2').fetchone()[0] == 1
    print('PASS: show overlap, past-time restrictions, six-seat limit, receipt/search, movie reactivation.')
