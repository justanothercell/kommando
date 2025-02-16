from flask import Flask, send_from_directory, request, redirect
import requests

app = Flask(__name__)

@app.route('/')
def index():
    return send_from_directory('.', 'index.html')

@app.route('/docs/guide')
def guide():
    return redirect('/docs/guide/introduction.html')

@app.route('/docs/guide/<path:item>')
def guide_item(item):
    return send_from_directory('../docs/guide/book/', item)

@app.route('/editor.css')
def style():
    return send_from_directory('.', 'editor.css')

@app.route('/execute', methods = [ 'POST' ])
def execute():
    print(request.json)
    r = requests.post('http://localhost:7878/execute', json=request.json)
    return r.json()

if __name__ == '__main__':
    app.run('0.0.0.0', 80)