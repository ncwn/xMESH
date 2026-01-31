#!/usr/bin/env python3
from os import read
import serial
import time
import json

CTX = {}

def run_cmd(cmd, wait_till_cb):
    run = True
    result = None
    print('> ' + cmd)
    s.write(cmd.encode() + b'\r\n')
    lines = []
    while run:
        data = s.readline()
        data = data.decode().strip()
        lines.append(data)
        print('< ' + data)
        run, result = wait_till_cb(lines)

    time.sleep(1)
    return result

def wait_for_ok_or_error(lines):
    data = lines[-1]
    return data not in ['OK', 'AT_BUSY_ERROR', 'AT_ERROR'], None

def wait_for_reset(lines):
    data = lines[-1]
    return data not in ['AT? to list all available functions'] , None

def wait_for_joined(lines):
    data = lines[-1]
    return data not in  ['+EVT:JOINED', '+EVT:JOIN FAILED'], None

def wait_for_join_status(lines):
    run, result = wait_for_ok_or_error(lines)
    if run == False:
        if lines[-3] == '0':
            status = 'not-joined'
        else:
            status = 'joined'
        return False, status
    return True, None

def write_context(ctx):
    with open('ctx.dat', 'w') as f:
        f.write(json.dumps(ctx, indent=4))

def read_context():
    data = {}
    with open('ctx.dat', 'r') as f:
        data = json.loads(f.read())
    return data

def wait_for_context(lines):
    run , result = wait_for_ok_or_error(lines)
    if run == False and len(lines) > 1:
        for i in range(len(lines) - 1):
            ln = lines[i]
            key = ln[0:6]
            if ln == '' or key[0:5] != '+CTX=':
                continue
            CTX[ln[0:6]] = ln
    return run, CTX

def load_context():
    try:
        CTX = read_context()
        for k in CTX:
            run_cmd('AT'+ CTX[k] + '\r\n', wait_for_ok_or_error)
    except Exception as e:
        print (e)

def store_context():
    try:
        write_context(CTX)
    except Exception as e:
        print (e)

if __name__ == '__main__':
    s = serial.Serial('/dev/ttyACM0', 9600)
    run_cmd('ATZ', wait_for_reset)

    load_context()

    joined = run_cmd('AT+NJS=?', wait_for_join_status) == 'joined'

    if not joined:
        run_cmd('AT+APPEUI=12:12:12:12:12:12:12:12', wait_for_ok_or_error)
    
    while not joined:
        run_cmd('AT+JOIN=1', wait_for_joined)
        joined = run_cmd('AT+NJS=?', wait_for_join_status) == 'joined'

    run_cmd('AT+CTX=?', wait_for_context)
    store_context()

    period = 2
    i = 0
    while True:
        print ('** TX: {}'.format(i))
        run_cmd('AT+SEND=21:0:123456789abcdeff', wait_for_ok_or_error)
        time.sleep(6)
        if (i % period == 0):
            run_cmd('AT+CTX=?', wait_for_context)
            store_context()
        i += 1
