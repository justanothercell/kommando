from flask import Flask, request, abort
from random import randint
import subprocess
import os
import shlex

TIMEOUT = 10

app = Flask(__name__)

@app.route('/execute', methods = [ 'POST' ])
def execute():
    runner_id = f'{randint(0, 0xFFFFFFFFFFFFFFFF):x}'
    try:
        data = request.json
        if data is None:
            return abort(400)
        with open(f'/runs/sandbox{runner_id}.kdo', 'w') as sandfile:
            sandfile.write(data['code'])
        try:
            process = subprocess.run(f'./kommando $(./kdolib/link) /runs/sandbox{runner_id}.kdo /runs/sandbox{runner_id} -c {shlex.quote(data.get('compiler_flags', ''))}', shell=True, capture_output=True, timeout=TIMEOUT)
        except subprocess.TimeoutExpired:
            return { 'success': False, 'output': f'Compilation timed out after {TIMEOUT}s', 'exit_code': -1 }
        exit_code = process.returncode
        output = '\n\n'.join([process.stdout.decode() + process.stderr.decode()]).strip().split('\n')
        if exit_code != 0:
            return { 'success': False, 'output': '\n'.join(output[-1000:]), 'exit_code': exit_code }
        if not os.path.isfile(f'/runs/sandbox{runner_id}'):
            return { 'success': True, 'output': '\n'.join(output[-1000:]), 'exit_code': exit_code }
        try:
            run_process = subprocess.run(f'/runs/sandbox{runner_id}', shell=True, cwd='.', capture_output=True, timeout=TIMEOUT)
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
            os.remove(f'/runs/sandbox{runner_id}')
        except OSError:
            pass
        try:
            os.remove(f'/runs/sandbox{runner_id}.kdo')
        except OSError:
            pass
        try:
            os.remove(f'/runs/sandbox{runner_id}.c')
        except OSError:
            pass
        try:
            os.remove(f'/runs/sandbox{runner_id}.h')
        except OSError:
            pass

if __name__ == '__main__':
    app.run('0.0.0.0', 7878)