#!/usr/bin/env python3
"""
墨芽童伴 (InkSprout) 电脑端传故事与自动配音助手
用法示例:
  1) USB 优盘模式自动拷入故事 (并可选用 macOS 中文语音自动生成 16kHz .wav 伴读):
     python3 tools/send_story.py story.txt --tts
  2) 连接设备热点 InkSprout-Kids 后通过 Wi-Fi 秒传:
     python3 tools/send_story.py story.txt --wifi --tts
"""
import argparse, os, shutil, subprocess, tempfile, urllib.parse, urllib.request, wave

def read_text_auto(path):
    raw = open(path, 'rb').read()
    for enc in ('utf-8-sig', 'utf-8', 'gb18030'):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return raw.decode('utf-8', errors='ignore')

def synth_macos_tts_wav(text, out_wav):
    with tempfile.NamedTemporaryFile(suffix='.aiff', delete=False) as tf:
        aiff_path = tf.name
    try:
        subprocess.run(['say', '-v', 'Tingting', '-r', '175', '-o', aiff_path, text], check=True)
        subprocess.run(['afconvert', '-f', 'WAVE', '-d', 'LEI16@16000', '-c', '1', aiff_path, out_wav], check=True)
        return True
    except Exception as e:
        print('TTS 合成提示 (需 macOS Tingting 语音):', e)
        return False
    finally:
        if os.path.exists(aiff_path):
            os.remove(aiff_path)

def find_usb_stories_dir():
    if os.path.isdir('/Volumes'):
        for v in os.listdir('/Volumes'):
            p = os.path.join('/Volumes', v)
            if v.upper().startswith('INKSPROUT') or os.path.isdir(os.path.join(p, 'stories')):
                sd = os.path.join(p, 'stories')
                os.makedirs(sd, exist_ok=True)
                return sd
    return None

def upload_wifi(host, filename, data_bytes):
    q = urllib.parse.quote(filename)
    url = f'http://{host}/api/upload?name={q}'
    req = urllib.request.Request(url, data=data_bytes, method='POST',
                                 headers={'Content-Type': 'application/octet-stream'})
    with urllib.request.urlopen(req, timeout=30) as resp:
        print('Wi-Fi 上传成功:', filename, '->', resp.read().decode('utf-8', 'ignore'))

def main():
    ap = argparse.ArgumentParser(description='InkSprout 故事导入与配音工具')
    ap.add_argument('files', nargs='+', help='.txt 或 .wav 文件路径')
    ap.add_argument('--wifi', action='store_true', help='通过设备热点 (192.168.4.1) 无线上传')
    ap.add_argument('--host', default='192.168.4.1', help='设备热点 IP (默认 192.168.4.1)')
    ap.add_argument('--usb-dir', default=None, help='指定优盘挂载路径 (默认自动查找 /Volumes)')
    ap.add_argument('--tts', action='store_true', help='对 .txt 自动调用 macOS 中文语音生成同名 16kHz .wav 伴读')
    args = ap.parse_args()

    usb_dir = args.usb_dir or (None if args.wifi else find_usb_stories_dir())
    if not args.wifi and not usb_dir:
        raise SystemExit('未检测到 InkSprout USB 优盘！请先在故事机进入【传书工坊 -> USB电脑优盘传书】，或加 --wifi 使用热点上传。')

    for fpath in args.files:
        base = os.path.basename(fpath)
        stem, ext = os.path.splitext(base)
        if ext.lower() == '.txt':
            clean_utf8 = read_text_auto(fpath).encode('utf-8')
            if args.wifi:
                upload_wifi(args.host, base, clean_utf8)
            else:
                dst = os.path.join(usb_dir, base)
                open(dst, 'wb').write(clean_utf8)
                print('已拷入 USB 优盘:', dst)

            if args.tts:
                with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as wf:
                    tmp_wav = wf.name
                if synth_macos_tts_wav(clean_utf8.decode('utf-8'), tmp_wav):
                    wav_name = stem + '.wav'
                    if args.wifi:
                        upload_wifi(args.host, wav_name, open(tmp_wav, 'rb').read())
                    else:
                        shutil.copyfile(tmp_wav, os.path.join(usb_dir, wav_name))
                        print('已生成并拷入同步伴读音频:', os.path.join(usb_dir, wav_name))
                if os.path.exists(tmp_wav):
                    os.remove(tmp_wav)
        else:
            raw = open(fpath, 'rb').read()
            if args.wifi:
                upload_wifi(args.host, base, raw)
            else:
                dst = os.path.join(usb_dir, base)
                open(dst, 'wb').write(raw)
                print('已拷入 USB 优盘:', dst)

if __name__ == '__main__':
    main()
