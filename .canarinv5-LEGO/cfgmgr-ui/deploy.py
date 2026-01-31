#!/usr/bin/env python3
import json
import gzip
import shutil

essential_files = {
    "main.css": {
        "deploy_to": "/components/can5_httpserver/webapp/main.css.gz"
    },
    "main.js" : {
        "deploy_to": "/components/can5_httpserver/webapp/main.js.gz"
    },
    "index.html" : {
        "deploy_to": "/components/can5_httpserver/webapp/index.html.gz"
    },
    "main.js.LICENSE.txt" : {
        "deploy_to": "/components/can5_httpserver/webapp/main.js.LICENSE.txt.gz"
    },
    "favicon.ico" : {
        "deploy_to": "/components/can5_httpserver/webapp/favicon.ico.gz"
    },
    "manifest.json" : {
        "deploy_to": "/components/can5_httpserver/webapp/manifest.json.gz"
    }
}

def full_build_path(path):
    return f'build{path}'

def full_deploy_path(path):
    return f'..{path}'

def read_asset_manifest(path):
    with open(path) as f:
        data = json.loads(f.read())
    return data

def fix_index_html(path, manifest):
    with open(path) as f:
        data = f.read()
    
    data = data.replace(manifest["files"]["main.css"], "/static/css/main.css")
    data = data.replace(manifest["files"]["main.js"], "/static/js/main.js")

    with open(path, "w") as f:
        f.write(data)

def process_essential_files(files):
    for file in files:
        if file in essential_files:
            input_path = full_build_path(files[file])
            output_path = full_deploy_path(essential_files[file]["deploy_to"])
            print (f"Process {file}: Transforming {input_path} to {output_path}")
            with open(input_path, 'rb') as f_in:
                with gzip.open(output_path, 'wb') as f_out:
                    shutil.copyfileobj(f_in, f_out)

if __name__ == "__main__":
    manifest = read_asset_manifest(full_build_path("/asset-manifest.json"))
    manifest["files"]["main.js.LICENSE.txt"] = manifest["files"]["main.js"] + ".LICENSE.txt"
    manifest["files"]["favicon.ico"] = "/favicon.ico"
    manifest["files"]["manifest.json"] = "/manifest.json"
    fix_index_html(full_build_path(manifest["files"]["index.html"]), manifest)
    process_essential_files(manifest["files"])


