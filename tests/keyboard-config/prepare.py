#!/usr/bin/env python3
"""Prepare an isolated stock-keyboard module smoke test (no receiver manifest)."""
import argparse
from pathlib import Path
import shutil
import subprocess
import yaml

parser = argparse.ArgumentParser()
parser.add_argument('--source', required=True)
parser.add_argument('--workspace', required=True)
args = parser.parse_args()
source = Path(args.source).resolve()
workspace = Path(args.workspace).resolve()
config = workspace / 'config'
if config.exists(): parser.error('Test config already exists; choose a new workspace')
revision = subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'], text=True).strip()
config.mkdir(parents=True)
shutil.copyfile(Path(__file__).parent/'corne.conf',config/'corne.conf')
manifest = {'manifest': {'projects': [
    {'name':'zmk','url':'https://github.com/zmkfirmware/zmk','revision':'v0.3','import':'app/west.yml'},
    {'name':'zmk-satellite','url':source.as_uri(),'revision':revision,
     'path':'modules/zmk-satellite','import':'module/west.yml'}], 'self': {'path':'config'}}}
(config/'west.yml').write_text(yaml.safe_dump(manifest,sort_keys=False),encoding='utf-8')
print('Prepared stock Corne test with one ON setting and local module import')
