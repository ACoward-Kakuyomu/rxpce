# rxpceの画像リソース

## アプリケーションアイコン

2026-10-28にユーザーがA案を選択しました。濃紺の角丸タイル、水色のピクセル状のr、赤いアクセントを使います。

* rxpce.png：組込みimage_genツールで生成した採用案の原本。RGBA、1254×1254。生成結果からバイト単位でコピーしています。
* rxpce.ico：上記PNGからICOへの形式変換で作成。透過付き32bit、16/20/24/32/40/48/64/96/128/256px。RCはこのファイルをIDR_MAINFRAMEとして参照します。

生成画像キャッシュやAPIはビルドに不要です。アプリのビルド時にPythonやPillowも実行しません。ICOを作り直す場合だけ、Pillowを利用できるPythonでリポジトリ直下から次を実行します。

```python
from PIL import Image

with Image.open("src/res/rxpce.png") as image:
    image.save("src/res/rxpce.ico", format="ICO",
               sizes=[(s, s) for s in (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)])
```

元の生成プロンプト（組込みimage_gen、transparent_background=true）：

> Use case: logo-brand. Asset type: preview concept for a Windows 11 desktop application icon for a PC Engine emulator named rxpce. Create one polished, professionally designed square app-icon concept, large and centered, front-facing orthographic view, generous transparent outer margin, actual transparent alpha outside the icon. Intended to remain recognizable at 16x16 and 32x32: bold silhouette, very few large shapes, high contrast, crisp edges, no tiny lettering, no mockup, no desktop, no screenshot, no surrounding labels, no watermark. A fresh identity, not a reproduction of an existing manufacturer's logo. Primary mark: one beautifully proportioned lowercase r monogram, constructed from a few substantial pixel-like geometric steps with clear negative space. The r is bright pale turquoise on a deep midnight navy rounded square tile. A single small coral-red square accents the lower right area, visually separate from the letter. Refined restrained depth and soft edge highlights, predominantly flat graphic rendering, no texture. Typography is just the abstract lowercase r, no other text. Confident, compact, distinctive emulator identity.

chars.bmpは原作由来の画面内表示用文字画像です。アイコン変更では編集していません。
