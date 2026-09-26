# Julretsu 1.2.0
### ネイティブのスプレッドシート ワークベンチ

[English](README.md) · [한국어](README.ko.md) · **日本語**

Julretsu は、Windows で動作確認したネイティブの ImGui アプリケーション、スパース方式の
C++20 スプレッドシート エンジン、制限付きで動く Lua の数式とマクロで構成されています。
Julretsu は無料で使えますが、所有権は Matthew Menchinton にあり、オープンソースでは
ありません。[LICENSE](LICENSE) を参照してください。アーキテクチャは Linux と macOS も対象で、
それらのビルドは GitHub Actions で作成しています。

- アプリには提供されたアイコンと起動バナーを使っています。[ブランディング メモ](docs/BRANDING-0.2.1.md)を参照してください。

## アプリの起動
このフォルダーの **Start Julretsu.cmd** をダブルクリックするか、
**release/Julretsu-1.2.0/Julretsu.exe** を実行します。付属の DLL は実行ファイルと同じ場所に置いてください。

[クイック スタートと操作](docs/QUICKSTART.md) ·
[Lua の仕様](docs/LUA.md) ·
[ユーザー マニュアル (日本語 PDF)](docs/Julretsu-User-Manual-ja.pdf)

**ローカル保存はファイル > 保存、名前を付けて保存、開くから行えます。** ネイティブの .julretsu
ファイルは、入力値、数式、行の書式、Lua スクリプトをそのまま保持します。未保存の変更は、ブックを
閉じたり置き換えたりする前に保存できます。CSV と Excel (.xlsx) のインポート / エクスポートで
ほかのスプレッドシート ソフトとデータをやり取りできます。何が引き継がれるかは
[データ交換](docs/DATA-EXCHANGE.md)にまとめています。任意の [AI アシスタント](docs/AI.md)は、
ご自身の Claude または OpenAI 互換のプロバイダーを使い、適用前にすべての編集をプレビューします。
本プロジェクトは編集可能なネイティブ MVP であり、Excel を完全に置き換えるものではありません。

## 実装済みの機能
- 1,000,000 論理行 × 16,384 列 (XFD1000000 まで)、スパース格納。
- ネイティブの四則演算、比較、SUM、AVERAGE、遅延評価の IF、構造化されたエラー。
- 増分的な依存関係の伝播、反復的な循環検出、アトミックな編集、スパースな行スタイル、
  コンパクトな選択範囲、元に戻す / やり直し。
- =LUA("return cell('A1') * 2") のような Lua 数式。実行時の依存関係の検出、循環の診断、
  計算された参照、分岐の変化にも対応します。
- 命令数、メモリ、出力、ホスト呼び出し、書き込み回数に制限を設けたトランザクション型 Lua マクロ。
- ネイティブの数式バー、座標ボックス、セル編集、移動、範囲と複数行の選択、行の書式、
  スクリプト エディター、ステータス バー。
- 縦横ともに仮想化したグリッド、DPI に応じたフォントとスタイルの再構築、計測された
  アロケーション数とフレーム時間。
- CSV のインポート / エクスポート (引用符内のカンマ、引用符、複数行セル、数式インジェクションに
  安全なエクスポート)。
- Excel .xlsx のインポート / エクスポート: 最初のワークシート、SUM/AVERAGE/IF と四則演算の数式、
  太字、色、行ごとの小数書式、そして何が引き継がれるかを示すインポート プレビュー。
- インターフェイスの言語: 英語、韓国語、日本語。システムの言語から自動で選ばれ、設定で変更でき、
  サンプル ブックやレポートも選んだ言語に従います。
- 任意の AI アシスタント: Anthropic または OpenAI 互換のプロバイダー、Windows 資格情報マネージャーに
  保管されるキー、検証済みの編集提案を確認してから 1 回の操作で適用し、元に戻せます。
- ランタイム DLL と依存ライブラリのライセンス表記を含むポータブルな Windows パッケージ。

## インストーラー (1.2.0)
- **Windows:** `.\package.ps1` が `release/Julretsu-1.2.0-Windows-Setup.exe` を作成します。
  ライセンス ページ、スタート メニューとデスクトップのショートカット、`.julretsu` の関連付け、
  設定 > アプリから使えるアンインストーラーを備えた、ユーザー単位の単一ファイル インストーラーです。
- **macOS と Linux:** `Installers` の GitHub Actions ワークフローが `Julretsu-1.2.0-macOS.dmg`
  (ユニバーサル、macOS 13.3 以降)、`Julretsu-1.2.0-linux-amd64.deb`、
  `Julretsu-1.2.0-linux-x86_64.tar.gz` を作成します。`v*` タグを push すると下書きリリースに添付されます。
- **マニュアル:** `docs/Julretsu-User-Manual.pdf` (英語)、`docs/Julretsu-User-Manual-ko.pdf` (韓国語)、
  `docs/Julretsu-User-Manual-ja.pdf` (日本語)。`docs/manual/` の HTML から生成し、画面写真は
  `julretsu --manual-shots <フォルダー> --language ja` で作成します。どのインストーラーにも 3 つとも含まれます。

## この Windows マシンでのビルドとテスト
```powershell
.\build.ps1 -Gui -Run
.\build.ps1 -Gui -Smoke
.\build.ps1 -Gui -Configuration Debug
.\package.ps1
```
.tools にあるポータブルな C++ ツールが、PATH を恒久的に変更せずに自動で使われます。.deps に
取得済みの固定されたソースは再利用されます。どちらのフォルダーも Git の管理外です。package.ps1 は
ローカルの LLVM-MinGW Release ビルドを対象とします。ほかのコンパイラーの再頒布要件は、それぞれの
パッケージャーが対応してください。

ヘッドレス コアは Lua やグラフィックス ライブラリなしでもビルドできます。
```powershell
.\build.ps1 -Example -Benchmarks
```

## クリーンな取得 / ほかのプラットフォーム
CMake 3.20 以降と C++20 対応コンパイラー (MSVC 2022、GCC 12 以降、Clang 16 以降) を使います。
コアにはコンパイラー以外の SDK は不要です。ネイティブ ビルドには OpenGL とプラットフォームの
ウィンドウ システム開発ライブラリが必要です。

```sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release -DJULRETSU_BUILD_GUI=ON
cmake --build build-native --config Release --parallel 2
ctest --test-dir build-native -C Release --output-on-failure
```

- Linux と macOS では ./build-native/julretsu を実行します。ヘッドレス用の補助スクリプトは
  sh ./build.sh です。Linux では GLFW に xorg-dev、libwayland-dev、libxkbcommon-dev、
  libgl1-mesa-dev が必要な場合があります (パッケージ名はディストリビューションによって異なります)。
  韓国語と日本語の文字を表示するには fonts-noto-cjk などの CJK フォントもインストールしてください。
  macOS には Xcode コマンドライン ツールが必要です。Windows には OpenGL に対応したグラフィックス
  ドライバーが必要です。

| オプション | 既定値 | 動作 |
| --- | --- | --- |
| JULRETSU_BUILD_GUI | OFF | ネイティブ アプリをビルドし、Lua も有効にする |
| JULRETSU_ENABLE_LUA | OFF | Lua アダプターと統合テストをビルド |
| JULRETSU_BUILD_TESTS | ON | ヘッドレス テストの実行ファイルを登録 |
| JULRETSU_BUILD_BENCHMARKS | OFF | エンジンのベンチマークをビルド |
| JULRETSU_ENABLE_SANITIZERS | OFF | Windows 以外の GCC/Clang で ASan/UBSan |

現在のターゲット: julretsu_core、julretsu_ai、julretsu_headless、任意で julretsu_benchmarks、
julretsu_lua、julretsu_xlsx、julretsu_ui、julretsu (ローカルに tests/ フォルダーがある場合はテストの
実行ファイルも)。

## パフォーマンスの確認
アプリは --smoke (キーボード / マウスの確認と画面キャプチャ) と --benchmark (10,000 セルの
スパース フィクスチャ) に対応しています。どちらも既定ではウィンドウを表示せずに実行し、--visible を
付けるとウィンドウが表示されます。ウォームアップ後、VSync をオフにした状態でフレーム全体の処理と
画面の切り替えを計測します。通常の操作では VSync が有効になります。実際の結果と計測の範囲は
受け入れレポートを参照してください。普遍的なフレーム レートを保証するものではありません。

## ソースの構成
- include/julretsu、src: コア、数式、依存グラフ、LuaEngine、GridUI、GridViewport、
  ネイティブの main、アロケーション計測、I18n (翻訳) と Glyphs (文字の収集)。
- tests: コアの仕様、Lua のサンドボックス / 依存関係 / マクロのテスト、ビューポートの境界。
- examples、benchmarks: ヘッドレスのサンプルとエンジンのベンチマーク。
- cmake/Dependencies.cmake: 検証済みで固定された上流の依存関係。
- docs: アーキテクチャ、数式、Lua、操作、ファイル交換、AI アシスタント、受け入れレポート、マニュアル。
- .github/workflows/ci.yml: 各プラットフォームのヘッドレス / Lua ビルドとサニタイザーのジョブ。

[アーキテクチャ](docs/ARCHITECTURE.md)、[数式の意味](docs/FORMULAS.md)、
[依存関係](docs/DEPENDENCIES.md)、[残りの作業](docs/ROADMAP.md)もご覧ください。

## ブック編集のアップデート (0.5)
クリップボード編集、セル単位の書式、数式を理解するドラッグ フィル、行と列の操作、並べ替え、
文字列フィルター、見出しの固定、複数のネイティブ ワークシート、回復用コピー、グラフと PDF 印刷の
レポートが利用できます。ウィンドウには、ブックの最小化 / 元に戻す / 閉じるボタンを備えた Office 風の
タイトル バーと、Excel 風にグループ分けしたホーム リボンがあります。組み込みのセーフティ ネットは、
よくあるスプレッドシートのミスを捉えます。危険な変更 (大きな貼り付け、一部だけの並べ替え、
上書きされた数式、桁違いの数値) は確定する前に確認し、リアルタイムのシートチェックは壊れた数式の
パターン、行が抜けた合計、文字列として保存された数値、表の中の空白行を知らせ、すべての変更は
ブックとともに保存されるセルごとの履歴に残ります。操作と現在の互換性の制限は
[クイック スタート](docs/QUICKSTART.md)を参照してください。ネイティブ ファイルは新しい書式と
ワークシートの機能を保持します。Excel へのエクスポートは現在アクティブなシートのみが対象です。
