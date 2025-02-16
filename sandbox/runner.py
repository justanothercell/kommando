from flask import Flask, request, abort
from random import randint
import subprocess
import os
import shlex
import atexit
from apscheduler.schedulers.background import BackgroundScheduler
from pathlib import Path
import time

def rmdir(directory):
    directory = Path(directory)
    for item in directory.iterdir():
        if item.is_dir():
            rmdir(item)
        else:
            item.unlink()
    directory.rmdir()

def delete_old_files():
    for item in Path('/tempdata').iterdir():
        if item.is_dir(): # invalid, delete either way
            rmdir(item)
        else: # delete if older than 1 minute
            file_age = time.time() - item.stat().st_mtime
            if file_age > 60:
                item.unlink()

scheduler = BackgroundScheduler()
scheduler.add_job(func=delete_old_files, trigger='interval', seconds=60 * 5)
scheduler.start()

atexit.register(lambda: scheduler.shutdown())

TIMEOUT = 10

app = Flask(__name__)

@app.route('/execute', methods = [ 'POST' ])
def execute():
    runner_id = f'{randint(0, 0xFFFFFFFFFFFFFFFF):x}'
    try:
        data = request.json
        if data is None:
            return abort(400)
        with open(f'/tempdata/sandbox{runner_id}.kdo', 'w') as sandfile:
            sandfile.write(data['code'])
        try:
            args = [shlex.quote(arg) for arg in data.get('compiler_flags', '').split()]
            process = subprocess.run(f'./kommando $(./kdolib/link) /tempdata/sandbox{runner_id}.kdo /tempdata/sandbox{runner_id} -c {' '.join(args)}', shell=True, capture_output=True, timeout=TIMEOUT, vars={'TMPDIR': '/tempdata'})
        except subprocess.TimeoutExpired:
            return { 'success': False, 'output': f'Compilation timed out after {TIMEOUT}s', 'exit_code': -1 }
        exit_code = process.returncode
        output = '\n\n'.join([process.stdout.decode() + process.stderr.decode()]).strip().split('\n')
        if exit_code != 0:
            return { 'success': False, 'output': '\n'.join(output[-1000:]), 'exit_code': exit_code }
        if not os.path.isfile(f'/tempdata/sandbox{runner_id}'):
            return { 'success': True, 'output': '\n'.join(output[-1000:]), 'exit_code': exit_code }
        try:
            run_process = subprocess.run(f'./sandbox{runner_id}', shell=True, cwd='/tempdata', capture_output=True, timeout=TIMEOUT)
        except subprocess.TimeoutExpired:
            return { 'success': False, 'output': f'Execution timed out after {TIMEOUT}s', 'exit_code': -1 }
        run_exit_code = run_process.returncode
        run_output = '\n\n'.join([run_process.stdout.decode() + run_process.stderr.decode()]).strip().split('\n')
        if data.get('show_compiler_output', False):
            return { 'success': True, 'output': '\n'.join((output + [''] + run_output)[-1000:]), 'exit_code': run_exit_code }
        else:
            return { 'success': True, 'output': '\n'.join(run_output[-1000:]), 'exit_code': run_exit_code }
    finally:
        try:
            os.remove(f'/tempdata/sandbox{runner_id}')
        except OSError:
            pass
        try:
            os.remove(f'/tempdata/sandbox{runner_id}.kdo')
        except OSError:
            pass
        try:
            os.remove(f'/tempdata/sandbox{runner_id}.c')
        except OSError:
            pass
        try:
            os.remove(f'/tempdata/sandbox{runner_id}.h')
        except OSError:
            pass

if __name__ == '__main__':
    app.run('0.0.0.0', 7878)