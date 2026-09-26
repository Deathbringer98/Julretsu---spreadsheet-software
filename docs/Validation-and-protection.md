# Validation and protection

## English

Open **Data > Validation and protection**, or use the button on the Review tab. Select your input range first, then choose the allowed values and add the rule. The window lists saved rules and existing problems. Select a problem address to visit its cell. Remove a rule and add its replacement to change the configuration. Undo restores rule changes as well as cell edits.

Supported rules: numbers or whole numbers between inclusive limits, valid dates written as YYYY-MM-DD, dropdown choices, required values, and unique values within the entire selected rectangle. Dropdown choices can contain Korean and Japanese text. Select a cell and open the validation window to choose a value from its list. User choices are preserved when the interface language changes.

Protection blocks changes to contents and formatting; protected formulas still recalculate. This prevents accidental editing, and is not password protection or access control. Anyone with the workbook can remove its rules.

Invalid edits are rejected atomically, including paste, fill, sorting, macros, and AI proposals. Warning mode permits invalid entries and opens the validation results. Rules may be added to an existing sheet with problems, which are listed without discarding existing data. Required cells can therefore remain empty until filled; adding a rule is not certification that a sheet is complete.

CSV and Excel imports into a sheet with rules check the incoming values before replacing data, retaining the existing rules and formatting. Use a new workbook for an import without the current rules. Rules are saved only in native .julretsu format (version 5); older Julretsu releases cannot open these files. Older workbook formats still open in this build. CSV and Excel exports omit validation and protection.

Limits: 1,000 rules, 100,000 cells counted across rule ranges, 100 choices per list, 256 UTF-8 bytes per choice, and 4 MiB of rule text. Structural row or column insertions/deletions are blocked while rules exist. Sorting keeps rules at their original coordinates and validates the result. Dates use ISO text, not Excel date serials. This is accidental-edit protection, not a permissions system.

## 한국어

**데이터 > 데이터 유효성 검사 및 보호** 또는 검토 탭의 버튼을 사용하세요. 입력 범위를 선택한 후 허용할 값과 규칙을 설정하세요. 저장된 규칙과 기존 문제가 창에 표시됩니다. 문제의 셀 주소를 누르면 해당 셀로 이동합니다. 규칙을 변경하려면 삭제한 후 다시 추가하세요. 실행 취소로 규칙 변경도 되돌릴 수 있습니다.

숫자 및 정수 범위, YYYY-MM-DD 형식의 날짜, 드롭다운 목록, 필수 입력, 선택 범위 전체의 중복 값 검사를 지원합니다. 목록에는 한국어와 일본어를 사용할 수 있습니다. 셀을 선택한 후 유효성 검사 창에서 목록 값을 선택하세요. 인터페이스 언어를 바꿔도 사용자가 입력한 항목은 유지됩니다.

보호된 셀의 내용과 서식은 변경할 수 없지만 수식은 계속 계산됩니다. 암호나 접근 권한 기능이 아니므로 파일을 가진 사람은 누구나 규칙을 삭제할 수 있습니다. 잘못된 붙여넣기나 채우기 등은 전체 작업이 거부됩니다. 경고 모드는 입력을 허용하고 검사 결과를 표시합니다. 기존 데이터에 규칙을 추가하면 기존 문제를 표시하며 데이터는 삭제하지 않습니다.

CSV 및 Excel 가져오기는 현재 시트의 규칙으로 검사하고 기존 서식을 유지합니다. 현재 규칙 없이 가져오려면 새 통합 문서를 사용하세요. 규칙은 버전 5의 .julretsu 파일에만 저장됩니다. 이전 앱에서는 이 파일을 열 수 없습니다. 이 빌드에서는 이전 파일을 열 수 있습니다. CSV 및 Excel 내보내기에는 규칙이 포함되지 않습니다.

규칙은 최대 1,000개, 적용 셀은 합계 100,000개입니다. 목록은 최대 100개 항목, 항목당 UTF-8 256바이트입니다. 규칙이 있으면 행과 열의 삽입 및 삭제가 차단됩니다. 정렬 시 규칙은 원래 위치에 유지되며 결과를 검사합니다. 날짜는 Excel 일련번호가 아닌 YYYY-MM-DD 텍스트입니다.

## 日本語

**データ > 入力規則と保護**、または校閲タブのボタンを開きます。入力範囲を選び、許可する値を設定して規則を追加してください。保存済みの規則と既存の問題が表示されます。問題のセル番地を押すとそのセルに移動します。設定を変更するには規則を削除して追加し直してください。規則の変更も元に戻せます。

数値や整数の範囲、YYYY-MM-DD 形式の日付、ドロップダウン、必須入力、選択範囲全体での重複チェックに対応します。選択肢に韓国語や日本語を使用できます。セルを選択し、入力規則ウィンドウでリストから値を選んでください。画面の言語を変更しても入力済みの選択肢は変わりません。

保護されたセルは内容と書式を変更できませんが、数式の再計算は続きます。パスワードやアクセス権の機能ではなく、ファイルを持つ人は誰でも規則を削除できます。無効な貼り付けやフィルなどは操作全体を拒否します。警告モードでは入力を許可し、検証結果を表示します。既存のデータに規則を追加してもデータは削除されず、既存の問題が表示されます。

CSV と Excel の読み込みでは現在のシートの規則を検査し、既存の書式を保持します。現在の規則なしで読み込むには新しいブックを使用してください。規則はバージョン 5 の .julretsu ファイルにのみ保存されます。以前のアプリではこのファイルを開けません。このビルドでは古いファイルを開けます。CSV と Excel への書き出しには規則は含まれません。

規則は最大 1,000 件、対象セルは合計 100,000 個です。リストは最大 100 項目、各項目は UTF-8 で 256 バイトまでです。規則がある間は行や列の挿入と削除を禁止します。並べ替えでは規則の位置を保持し、結果を検査します。日付には Excel のシリアル値ではなく YYYY-MM-DD の文字列を使用します。
