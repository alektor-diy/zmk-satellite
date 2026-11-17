#!/usr/bin/env python3
"""Test the bundled sender without fetching Prospector, Zephyr or a keyboard."""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc',default=os.environ.get('CC','cc'))
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    source=(root/'module/sender.c').read_text(encoding='utf-8')
    assert 'ZMK_SATELLITE_UPSTREAM_SOURCE' not in source
    assert 'defined(BT_LE_ADV_OPT_' not in source

    def section(start,end):
        a=source.index(start)
        return source[a:source.index(end,a)]

    with tempfile.TemporaryDirectory(prefix='satellite-adv-') as tmp:
        tmp=Path(tmp)
        callbacks=section('static int activity_state_listener(', '\nZMK_LISTENER(prospector_activity_listener')
        callbacks+=section('static void prospector_ble_connected(', '\nstatic struct bt_conn_cb prospector_conn_callbacks')
        callbacks+=section('static void adv_work_handler(', '\nstatic int init_prospector_status(')
        (tmp/'generated_callbacks.inc').write_text(callbacks,encoding='utf-8')
        for capability, studio in [(c,s) for c in [0,1] for s in [0,1]]:
            binary=tmp/f'test-{capability}-{studio}'
            flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if args.sanitize else []
            subprocess.run([args.cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
                            *flags,f'-DSATELLITE_HAS_FORCE_NAME_IN_AD={capability}',
                            f'-DSATELLITE_HAS_SCANNABLE={capability}',
                            f'-DSATELLITE_STUDIO_BLE={studio}',
                            '-I'+str(root/'module'),'-I'+str(tmp),str(root/'tests/test_advertising.c'),
                            '-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
    print('Bundled sender advertising regression passed; no external source or headers required.')


if __name__=='__main__':
    main()
