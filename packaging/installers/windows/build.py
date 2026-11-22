"""Package the existing Windows 1.09 payload; do not compile the plug-in."""
from pathlib import Path
import argparse, hashlib, json, subprocess

parser=argparse.ArgumentParser()
for arg in ('payload','output','work','iscc'):
    parser.add_argument('--'+arg,type=Path,required=True)
args=parser.parse_args()
here=Path(__file__).resolve().parent
lock=json.loads((here.parent/'inputs.json').read_text(encoding='utf-8'))
for path in (args.output,args.work):path.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for row in lock['windowsFiles']:
    assert sha(args.payload/'QQEasyTool.vst3'/row['path'])==row['sha256'],row['path']
license_path=args.work/'License.txt'
license_path.write_text((here.parents[2]/'LICENSE').read_text(encoding='utf-8-sig'),encoding='utf-8-sig')
files={row['path']:row['sha256'] for row in lock['windowsFiles']}
subprocess.run([str(args.iscc),'/Qp','/DPayloadRoot='+str(args.payload),'/DOutputRoot='+str(args.output),
    '/DLicensePath='+str(license_path),'/DBinarySHA='+files['Contents/x86_64-win/QQEasyTool.vst3'],
    '/DModuleSHA='+files['Contents/Resources/moduleinfo.json'],str(here/'QQEasyTool.iss')],check=True)
setup=args.output/'QQEasyTool-1.09-Windows-x64-Setup.exe'
(args.work/'Windows-Build.json').write_text(json.dumps(dict(version='1.09',internalVersion='1.0.9',installer=setup.name,
    bytes=setup.stat().st_size,sha256=sha(setup),payload=lock['windowsFiles'],productRebuilt=False),indent=2),encoding='utf-8')
