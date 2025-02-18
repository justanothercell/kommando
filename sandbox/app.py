from flask import Flask, send_from_directory, request, redirect, abort
import requests
import logging
from datetime import datetime

logging.basicConfig(filename='log/latest.log', level=logging.INFO)

app = Flask(__name__)

@app.route('/')
def index():
    return send_from_directory('.', 'index.html')

@app.route('/docs')
def guide():
    return redirect('/docs/index.html')

@app.route('/docs/<path:item>')
def guide_item(item):
    if item.endswith('.html'):
        print(f'{datetime.now().strftime("%Y-%m-%d %H:%M:%S")} [\x1b[1;34mDOCS\x1b[0m] \x1b[1m{request.remote_addr}\x1b[0m {item}')
    return send_from_directory('../docs/book/', item)

@app.route('/editor.css')
def style():
    return send_from_directory('.', 'editor.css')

@app.route('/execute', methods = [ 'POST' ])
def execute():
    print(f'{datetime.now().strftime("%Y-%m-%d %H:%M:%S")} [\x1b[1;33mEXEC\x1b[0m] \x1b[1m{request.remote_addr}\x1b[0m')
    print(request.json)
    try:
        r = requests.post('http://localhost:7878/execute', json=request.json)
    except Exception:
        return abort(503)
    return r.json()

if __name__ == '__main__':
    app.run('0.0.0.0', 80)