"""Repackage the locked, already released signed Mac bundles; no plug-in rebuild."""
from pathlib import Path, PurePosixPath
import argparse, hashlib, json, os, plistlib, shutil, subprocess, time, urllib.request, zipfile
import xml.etree.ElementTree as ET

parser=argparse.ArgumentParser()
parser.add_argument('--arch',choices=['arm64','x86_64'],required=True)
parser.add_argument('--work',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
HERE=Path(__file__).resolve().parent
LOCK=json.loads((HERE.parent/'inputs.json').read_text())
work=args.work.resolve();out=args.output.resolve()
work.mkdir(parents=True,exist_ok=True);out.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(*cmd):
    try:return subprocess.check_output([str(c) for c in cmd],stderr=subprocess.STDOUT).decode('utf-8','replace')
    except subprocess.CalledProcessError as error:
        print(error.output.decode('utf-8','replace'),flush=True)
        raise
def filemap(root):return {p.relative_to(root).as_posix():sha(p) for p in sorted(root.rglob('*')) if p.is_file()}
def download(asset):
    path=work/asset['name']
    if not (path.is_file() and sha(path)==asset['sha256']):
        for attempt in range(3):
            try:
                req=urllib.request.Request(asset['url'],headers={'User-Agent':'QQEasyTool-Installer-Packaging'})
                with urllib.request.urlopen(req,timeout=75) as response, path.open('wb') as f:shutil.copyfileobj(response,f)
                break
            except OSError:
                if attempt==2:raise
                time.sleep(2)
    assert sha(path)==asset['sha256'] and path.stat().st_size==asset['bytes']
    return path

platform='Apple-Silicon' if args.arch=='arm64' else 'Intel'
sources={}
for fmt,assetname,bundle,arches in [
    ('VST3',f'QQEasyTool-macOS-{platform}-1.09.zip','QQEasyTool.vst3',[args.arch]),
    ('Components','QQEasyTool-macOS-AU-Universal-1.09.zip','QQEasyTool.component',['arm64','x86_64'])]:
    archive=download(LOCK['macAssets'][assetname])
    payload=work/('payload-'+fmt)
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        for item in z.infolist():
            parts=PurePosixPath(item.filename).parts
            if item.is_dir() or parts[0]=='__MACOSX' or parts[-1].startswith('._'):continue
            assert parts[0]==bundle and '..' not in parts and not item.filename.startswith('/')
            assert (item.external_attr>>16)&0o170000 != 0o120000,'Unexpected symlink'
            target=payload/'Library/Audio/Plug-Ins'/fmt/Path(*parts)
            target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(z.read(item))
            target.chmod(0o755 if 'MacOS' in parts else 0o644)
    installed=payload/'Library/Audio/Plug-Ins'/fmt/bundle
    info=plistlib.loads((installed/'Contents/Info.plist').read_bytes())
    assert info['CFBundleShortVersionString']==info['CFBundleVersion']=='1.0.9'
    binary=installed/'Contents/MacOS/QQEasyTool'
    assert sorted(run('lipo','-archs',binary).split())==sorted(arches)
    signature=run('codesign','--verify','--all-architectures','--deep','--strict','--verbose=2',installed)
    scripts=work/('scripts-'+fmt);scripts.mkdir()
    script=scripts/'preinstall'
    script.write_text((HERE/'preinstall').read_text().replace('@FORMAT@',fmt).replace('@BUNDLE@',bundle))
    script.chmod(0o755)
    policy=work/(fmt+'-bundles.plist')
    run('pkgbuild','--analyze','--root',payload,policy)
    rows=plistlib.loads(policy.read_bytes());assert len(rows)==1
    for row in rows:row.update(BundleIsRelocatable=False,BundleIsVersionChecked=False,BundleOverwriteAction='upgrade')
    policy.write_bytes(plistlib.dumps(rows))
    identifier='com.qingaudio.qqeasytool.'+('vst3' if fmt=='VST3' else 'au')
    component=work/(fmt+'.pkg')
    run('pkgbuild','--root',payload,'--component-plist',policy,'--scripts',scripts,'--install-location','/',
        '--ownership','recommended','--identifier',identifier,'--version','1.0.9',component)
    sources[fmt]=dict(bundle=bundle,arches=arches,identifier=identifier,payload=payload,files=filemap(payload),signature=signature)

distribution=work/'Distribution.xml'
supported='arm64' if args.arch=='arm64' else 'x86_64,arm64'
# Intel VST3 can also be used by an Intel/Rosetta DAW on Apple Silicon.
xml=f'''<?xml version="1.0" encoding="UTF-8"?>
<installer-gui-script minSpecVersion="2">
  <title>QQEasyTool 1.09 — {platform}</title>
  <options customize="allow" require-scripts="false" hostArchitectures="{supported}"/>
  <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
  <volume-check><allowed-os-versions><os-version min="11.0"/></allowed-os-versions></volume-check>
  <welcome file="Welcome.html"/>
  <license file="License.txt"/>
  <choices-outline><line choice="vst3"/><line choice="au"/></choices-outline>
  <choice id="vst3" title="VST3 ({args.arch})" description="QQEasyTool for Studio One, Cubase, REAPER and VST3 hosts" start_selected="true"><pkg-ref id="com.qingaudio.qqeasytool.vst3"/></choice>
  <choice id="au" title="Audio Unit (Universal 2)" description="QQEasyTool for Logic Pro and AU hosts; arm64 + x86_64" start_selected="true"><pkg-ref id="com.qingaudio.qqeasytool.au"/></choice>
  <pkg-ref id="com.qingaudio.qqeasytool.vst3" version="1.0.9" auth="Root">VST3.pkg</pkg-ref>
  <pkg-ref id="com.qingaudio.qqeasytool.au" version="1.0.9" auth="Root">Components.pkg</pkg-ref>
</installer-gui-script>
'''
distribution.write_text(xml)
resources=work/'resources';resources.mkdir()
(resources/'Welcome.html').write_text(f'''<!doctype html><meta charset="utf-8"><h1>QQEasyTool 1.09</h1>
<p>Qing Audio · {platform} · macOS 11+</p><p>请保存工程并完全退出音频宿主。可点“自定”选择 VST3、AU 或两者。</p>
<p>Save your project and fully quit audio hosts. Use Customize to select VST3, AU or both.</p>
<p>安装到 /Library/Audio/Plug-Ins；升级同名插件，保留工程和用户设置。<br>Installs to /Library/Audio/Plug-Ins, updating the same-named plug-ins while preserving projects/settings.</p>
<p>完整功能，无期限限制。非商业源码共享许可；禁止商业使用。<br>Full functionality without expiry. Non-commercial source-share license; no commercial use.</p>
<p>Installer is unsigned and not notarized. Plug-in bundles retain their original ad-hoc signatures.</p>''')
(resources/'License.txt').write_bytes((HERE.parents[2]/'LICENSE').read_bytes())
setup=out/f'QQEasyTool-1.09-macOS-{platform}-Setup.pkg'
run('productbuild','--distribution',distribution,'--resources',resources,'--package-path',work,setup)
expanded=work/'expanded'
run('pkgutil','--expand-full',setup,expanded)
for fmt,row in sources.items():
    unpacked=expanded/(fmt+'.pkg')/'Payload'
    assert filemap(unpacked)==row['files'],'Package payload differs from verified released files'
    run('codesign','--verify','--all-architectures','--deep','--strict',unpacked/'Library/Audio/Plug-Ins'/fmt/row['bundle'])
choices=run('/usr/sbin/installer','-showChoicesXML','-pkg',setup,'-target','/')
(work/'choices.xml').write_text(choices)
assert 'vst3' in choices and 'au' in choices
signature_result=subprocess.run(['pkgutil','--check-signature',str(setup)],capture_output=True,text=True)
assert signature_result.returncode!=0 # No Developer ID Installer identity was supplied.

# The following real install/upgrade checks run only on the disposable CI runner.
assert os.environ.get('GITHUB_ACTIONS')=='true','Native installation tests require the disposable CI runner'
for fmt,row in sources.items():assert not Path('/Library/Audio/Plug-Ins',fmt,row['bundle']).exists()
sentinel=work/'preserved-user-data';sentinel.write_text('unrelated user data')
other=Path('/Library/Audio/Plug-Ins/VST3/QQEasyTool-Installer-Test-OtherVendor.vst3')
run('sudo','mkdir','-p',other)
run('sudo','cp',sentinel,other/'keep')
au_only=work/'au-only.plist'
au_only.write_bytes(plistlib.dumps([dict(choiceIdentifier='vst3',choiceAttribute='selected',attributeSetting=0),dict(choiceIdentifier='au',choiceAttribute='selected',attributeSetting=1)]))
run('sudo','/usr/sbin/installer','-pkg',setup,'-target','/','-applyChoiceChangesXML',au_only)
assert not Path('/Library/Audio/Plug-Ins/VST3/QQEasyTool.vst3').exists()
def verify_installed(formats):
    for fmt in formats:
        row=sources[fmt];bundle=Path('/Library/Audio/Plug-Ins',fmt,row['bundle'])
        expected={str(Path(p).relative_to(Path('Library/Audio/Plug-Ins')/fmt/row['bundle'])):h for p,h in row['files'].items()}
        assert filemap(bundle)==expected
        run('codesign','--verify','--all-architectures','--deep','--strict',bundle)
    assert (other/'keep').read_bytes()==sentinel.read_bytes()
verify_installed(['Components'])
run('sudo','/usr/sbin/installer','-pkg',setup,'-target','/')
verify_installed(['VST3','Components'])
# Simulate older metadata and a stale resource, then exercise bundle replacement.
for fmt,row in sources.items():
    bundle=Path('/Library/Audio/Plug-Ins',fmt,row['bundle'])
    run('sudo','/usr/libexec/PlistBuddy','-c','Set :CFBundleShortVersionString 1.0.8',bundle/'Contents/Info.plist')
    run('sudo','cp',sentinel,bundle/'Contents/old-installer-test-resource')
run('sudo','/usr/sbin/installer','-pkg',setup,'-target','/')
verify_installed(['VST3','Components'])
receipt={fmt:plistlib.loads(run('pkgutil','--pkg-info-plist',row['identifier']).encode())['pkg-version'] for fmt,row in sources.items()}
assert set(receipt.values())=={'1.0.9'}
evidence=dict(status='PASS',version='1.09',internalVersion='1.0.9',arch=args.arch,installer=setup.name,
    bytes=setup.stat().st_size,sha256=sha(setup),sourceCommit=LOCK['sourceCommit'],packagingCommit=os.environ.get('GITHUB_SHA'),
    originalBundlesByteIdentical=True,productRebuilt=False,minimumMacOS='11.0',installerSigned=False,notarized=False,
    pluginAdHocSignaturesVerified=True,auOnlyChoice=True,freshNativeInstall=True,simulatedOlderMetadataUpgrade=True,
    staleResourceRemoved=True,unrelatedFilePreserved=True,receipts=receipt,actualDawVerified=False,
    payloads={fmt:{k:v for k,v in row.items() if k!='payload'} for fmt,row in sources.items()})
(out/f'Mac-{platform}-Verification.json').write_text(json.dumps(evidence,indent=2))
print(json.dumps({k:v for k,v in evidence.items() if k!='payloads'},indent=2),flush=True)
