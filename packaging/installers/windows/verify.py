"""Test the shipping EXE in a private D-drive fixture, without touching installed plug-ins."""
from pathlib import Path
import argparse, ctypes, hashlib, json, subprocess, tempfile
from ctypes import wintypes

p=argparse.ArgumentParser()
for name in ['installer','work','previous','receipt']:p.add_argument('--'+name,type=Path,required=True)
args=p.parse_args()
lock=json.loads((Path(__file__).resolve().parents[1]/'inputs.json').read_text())
args.work.mkdir(parents=True,exist_ok=True)
fixture=Path(tempfile.mkdtemp(prefix='verified-installer-',dir=args.work)).resolve()
assert fixture.is_relative_to(args.work.resolve())
dest=fixture/'VST3';bundle=dest/'QQEasyTool.vst3'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def execute(command,timeout=120):
    return subprocess.run([str(c) for c in command],timeout=timeout,creationflags=subprocess.CREATE_NO_WINDOW).returncode
def install(name):
    return execute([args.installer,'/CURRENTUSER','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/DIR='+str(dest),'/LOG='+str(fixture/(name+'.log'))])
def verify():
    actual={p.relative_to(bundle).as_posix():sha(p) for p in bundle.rglob('*') if p.is_file()}
    assert actual=={row['path']:row['sha256'] for row in lock['windowsFiles']},actual
    assert other.read_bytes()==b'unrelated plugin data'
    assert userdata.read_bytes()==b'user settings preserved'
other=dest/'Other Vendor.vst3'/'keep';other.parent.mkdir(parents=True);other.write_bytes(b'unrelated plugin data')
userdata=fixture/'user-settings';userdata.write_bytes(b'user settings preserved')
assert install('fresh')==0,'Fresh install failed'
verify()
for row in lock['windowsFiles']:
    (bundle/row['path']).write_bytes((args.previous/row['path']).read_bytes())
assert install('upgrade-from-1.08')==0,'Upgrade failed'
verify()
kernel=ctypes.WinDLL('kernel32',use_last_error=True)
kernel.CreateFileW.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,wintypes.LPVOID,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE]
kernel.CreateFileW.restype=wintypes.HANDLE
kernel.CloseHandle.argtypes=[wintypes.HANDLE]
handle=kernel.CreateFileW(str(bundle/'Contents/x86_64-win/QQEasyTool.vst3'),0x80000000,0,None,3,0,None)
assert handle not in (None,wintypes.HANDLE(-1).value)
try:blocked=install('locked-file')
finally:kernel.CloseHandle(handle)
assert blocked!=0,'Installer accepted a locked plug-in'
verify()
assert install('retry')==0
verify()
uninstallers=list((dest/'.QQEasyTool-Uninstall').glob('unins*.exe'))
assert len(uninstallers)==1
assert execute([uninstallers[0],'/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/LOG='+str(fixture/'uninstall.log')])==0
assert not (bundle/'Contents/x86_64-win/QQEasyTool.vst3').exists()
assert not (bundle/'Contents/Resources/moduleinfo.json').exists()
assert other.read_bytes()==b'unrelated plugin data' and userdata.read_bytes()==b'user settings preserved'
result=dict(status='PASS',version='1.09',installer=args.installer.name,sha256=sha(args.installer),bytes=args.installer.stat().st_size,
    freshInstall=True,actual108Upgrade=True,lockedFileRejected=blocked,successfulRetry=True,uninstall=True,
    unrelatedFilesAndSettingsPreserved=True,systemVST3Untouched=True,fixture=str(fixture),payload=lock['windowsFiles'])
args.receipt.parent.mkdir(parents=True,exist_ok=True);args.receipt.write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2),flush=True)
