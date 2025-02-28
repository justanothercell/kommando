from pathlib import Path
import time
from flask import Flask, send_from_directory, request, redirect, abort, render_template, Response, jsonify
from werkzeug.security import safe_join
import requests
import logging
from datetime import datetime
from dotenv import load_dotenv
from apscheduler.schedulers.background import BackgroundScheduler
import os

load_dotenv()

DOMAIN_NAME = os.getenv('DOMAIN_NAME')
USE_SSL = os.getenv('SANDBOX_USE_SSL', 'false') == 'true'

def delete_old_files():
    for item in Path('./artifacts').iterdir():
        if item.name == '.gitignore':
            continue
        file_age = time.time() - item.stat().st_mtime
        if file_age > 60 * 15: # 15 minutes
            item.unlink()

delete_old_files()
scheduler = BackgroundScheduler()
scheduler.add_job(func=delete_old_files, trigger='interval', seconds=60)
scheduler.start()

logging.basicConfig(filename='log/latest.log', level=logging.INFO)

app = Flask(__name__)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/docs')
@app.route('/docs/')
@app.route('/docs/index.html')
@app.route('/docs/introduction.html')
def docs():
    response = send_from_directory('../docs/book/', 'index.html')
    if DOMAIN_NAME is not None:
        if USE_SSL:
            response.headers['Link'] = f'<https://{DOMAIN_NAME}/docs>; rel="canonical"'
        else:
            response.headers['Link'] = f'<http://{DOMAIN_NAME}/docs>; rel="canonical"'
    return response

@app.route('/docs/<path:item>')
def docs_item(item):
    if item.endswith('.html'):
        print(f'{datetime.now().strftime("%Y-%m-%d %H:%M:%S")} [\x1b[1;34mDOCS\x1b[0m] \x1b[1m{request.remote_addr}\x1b[0m {item}')
    return send_from_directory('../docs/book/', item)

@app.route('/editor.css')
def style():
    return send_from_directory('static', 'editor.css')

@app.route('/execute', methods = [ 'POST' ])
def execute():
    print(f'{datetime.now().strftime("%Y-%m-%d %H:%M:%S")} [\x1b[1;33mEXEC\x1b[0m] \x1b[1m{request.remote_addr}\x1b[0m')
    print(request.json)
    try:
        r = requests.post('http://localhost:7878/execute', json=request.json)
    except Exception:
        return abort(503)
    r = r.json()
    if 'artifacts' in r:
        artifacts = r['artifacts']
        if len(artifacts) > 0:
            r['artifacts'] = []
            for name, content in artifacts.items():
                with open(f'artifacts/{name}', 'w') as artifactfile:
                    artifactfile.write(content)
                r['artifacts'].append(name)
        else:
            del r['artifacts']
    response = jsonify(r)
    response.headers['X-Robots-Tag'] = 'noindex, nofollow'
    return response

@app.route('/artifacts/<name>')
def artifacts(name):
    if name == '.gitignore':
        return 'That\'s not an artifact :)', 404
    file = safe_join('artifacts', name)
    if not os.path.isfile(file):
        return 'This artifact does not exist. Note that artifacts periodically get deleted', 404
    with open(file) as artifactfile:
        content = artifactfile.read()

    response = Response(render_template('viewer.html', file_name=name, file_content=content, file_language='c'))
    response.headers['X-Robots-Tag'] = 'noindex, nofollow'
    return response
    
@app.route('/raw_artifacts/<name>')
def raw_artifacts(name):
    if name == '.gitignore':
        return 'That\'s not an artifact :)', 404
    file = safe_join('artifacts', name)
    if not os.path.isfile(file):
        return 'This artifact does not exist. Note that artifacts periodically get deleted', 404
    with open(file) as artifactfile:
        content = artifactfile.read()
    if "text/html" in request.headers.get("Accept", ""):
        response = Response(f'<pre style="font-family: monospace;">{content}</pre>', mimetype='text/html')
    else:
        response = Response(content, mimetype='text/plain')
    response.headers['X-Robots-Tag'] = 'noindex, nofollow'
    return response
    


if __name__ == '__main__':
    if USE_SSL:
        context = (os.getenv('SANDBOX_DOMAIN_CERT_FILE'), os.getenv('SANDBOX_DOMAIN_KEY_FILE'))
        app.run('0.0.0.0', 443, ssl_context=context)
    else:
        app.run('0.0.0.0', 80, debug=True)