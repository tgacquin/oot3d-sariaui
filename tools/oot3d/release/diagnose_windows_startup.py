"""Collect diagnostics for startup of the CI-built package on windows-2022."""
import ctypes
from ctypes import wintypes
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from package import copy_payload

bundle = Path('build-release-windows-x64/bundle').resolve()
gdb = shutil.which('gdb')
with tempfile.TemporaryDirectory() as tmp:
    work = Path(tmp)
    stage = work / 'SariaUI'
    stage.mkdir()
    copy_payload('windows-x64', bundle, stage)
    env = os.environ.copy()
    env.pop('QT_PLUGIN_PATH', None)
    env.pop('QT_QPA_PLATFORM_PLUGIN_PATH', None)
    env.pop('QTDIR', None)
    root = Path(env['SystemRoot'])
    env.update(PATH=os.pathsep.join(map(str,(stage,root/'System32',root))),
               QT_QPA_PLATFORM='windows', QT_DEBUG_PLUGINS='1', QT_FORCE_STDERR_LOGGING='1',
               OOT3D_USER_PATH=str(work/'user'))
    print('qt.conf:', (stage/'qt.conf').read_text(), flush=True)
    with (work/'trace').open('wb') as log:
        proc = subprocess.Popen([str(stage/'SariaUI.exe'),'--version'],cwd=work,env=env,
                                stdin=subprocess.DEVNULL,stdout=log,stderr=log)
        try:
            print('Version exit:',proc.wait(timeout=15),flush=True)
        except subprocess.TimeoutExpired:
            print('Startup output:',(work/'trace').read_bytes().decode(errors='replace'),flush=True)
            ui=ctypes.windll.user32
            callback=ctypes.WINFUNCTYPE(ctypes.c_int,wintypes.HWND,wintypes.LPARAM)
            def title(hwnd):
                buf=ctypes.create_unicode_buffer(1500)
                ui.GetWindowTextW(hwnd,buf,len(buf))
                return buf.value
            @callback
            def child(hwnd,unused):
                name=title(hwnd)
                if name: print('Child:',name[:500],flush=True)
                return 1
            @callback
            def window(hwnd,unused):
                pid=wintypes.DWORD()
                ui.GetWindowThreadProcessId(wintypes.HWND(hwnd),ctypes.byref(pid))
                if pid.value==proc.pid or ui.IsWindowVisible(wintypes.HWND(hwnd)):
                    print('Window:',pid.value,title(wintypes.HWND(hwnd)),flush=True)
                    ui.EnumChildWindows(wintypes.HWND(hwnd),child,0)
                return 1
            ui.EnumWindows(window,0)
            if gdb:
                subprocess.run([gdb,'-nx','-batch','-iex','set auto-load off','-p',str(proc.pid),
                                '-ex','info threads','-ex','thread apply all bt 16','-ex','detach'],
                               cwd=work,timeout=90)
        finally:
            if proc.poll() is None: proc.kill()
            proc.wait()
    print('Final output:',(work/'trace').read_bytes().decode(errors='replace'),flush=True)
    # Windows children inherit this error mode at process creation. Loader errors
    # can then surface as NTSTATUS exit codes without requiring a desktop click.
    kernel=ctypes.windll.kernel32
    kernel.SetErrorMode(kernel.GetErrorMode() | 1 | 2)
    for args in ([str(stage/'oot3d-seed.exe'),'--catalog',str(work/'catalog.json')],
                 [str(stage/'SariaUI.exe'),'--version']):
        try:
            p=subprocess.run(args,cwd=work,env=env,timeout=20,capture_output=True)
            print(Path(args[0]).name,hex(p.returncode & 0xffffffff),p.stdout[:400],p.stderr[-1200:],flush=True)
        except subprocess.TimeoutExpired:print(Path(args[0]).name,'still timed out',flush=True)
