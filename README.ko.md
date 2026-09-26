# Julretsu 1.2.0
### 네이티브 스프레드시트 작업 공간

[English](README.md) · **한국어** · [日本語](README.ja.md)

Julretsu는 Windows에서 검증한 네이티브 ImGui 애플리케이션, 희소 저장 방식의 C++20
스프레드시트 엔진, 제한된 환경에서 실행되는 Lua 수식과 매크로로 이루어져 있습니다.
Julretsu는 무료로 사용할 수 있지만 소유권은 Matthew Menchinton에게 있으며 오픈 소스가
아닙니다. [LICENSE](LICENSE)를 참고하세요. 아키텍처는 Linux와 macOS도 대상으로 하며,
해당 빌드는 GitHub Actions에서 만들어집니다.

- 앱에는 제공된 아이콘과 시작 배너가 사용됩니다. [브랜딩 메모](docs/BRANDING-0.2.1.md)를 참고하세요.

## 앱 실행
이 폴더의 **Start Julretsu.cmd**를 두 번 클릭하거나
**release/Julretsu-1.2.0/Julretsu.exe**를 실행하세요. 함께 제공되는 DLL은 실행 파일 옆에 두어야 합니다.

[빠른 시작과 조작법](docs/QUICKSTART.md) ·
[Lua 계약](docs/LUA.md) ·
[사용 설명서(한국어 PDF)](docs/Julretsu-User-Manual-ko.pdf)

**로컬 저장은 파일 > 저장, 다른 이름으로 저장, 열기로 할 수 있습니다.** 네이티브 .julretsu 파일은
입력값, 수식, 행 서식, Lua 스크립트를 그대로 보존합니다. 저장하지 않은 변경은 통합 문서를
닫거나 바꾸기 전에 저장할 수 있습니다. CSV와 Excel(.xlsx) 가져오기/내보내기로 다른 스프레드시트
프로그램과 데이터를 주고받을 수 있습니다. 무엇이 옮겨지는지는 [데이터 교환](docs/DATA-EXCHANGE.md)에
정리되어 있습니다. 선택 사항인 [AI 도우미](docs/AI.md)는 사용자의 Claude 또는 OpenAI 호환 공급자를
사용하며, 적용 전에 모든 편집을 미리 보여 줍니다. 이 프로젝트는 편집 가능한 네이티브 MVP이며
Excel을 완전히 대체하지는 않습니다.

## 구현된 기능
- 1,000,000 논리 행 × 16,384 열(XFD1000000까지), 희소 저장.
- 네이티브 사칙연산, 비교, SUM, AVERAGE, 지연 평가 IF, 구조화된 오류.
- 증분 의존성 전파, 반복적 순환 참조 검출, 원자적 편집, 희소 행 스타일,
  압축된 선택 영역, 실행 취소/다시 실행.
- =LUA("return cell('A1') * 2") 같은 Lua 수식. 런타임 의존성 탐색, 순환 진단,
  계산된 참조, 분기 변화까지 지원합니다.
- 명령 수, 메모리, 출력, 호스트 호출, 쓰기 횟수에 제한을 둔 트랜잭션 Lua 매크로.
- 네이티브 수식 입력줄, 좌표 상자, 셀 편집, 이동, 범위와 여러 행 선택, 행 서식,
  스크립트 편집기, 상태 표시줄.
- 양방향 가상화 격자, DPI에 맞춘 글꼴/스타일 재구성, 측정된 할당 횟수와 프레임 시간.
- CSV 가져오기/내보내기(따옴표 안의 쉼표, 따옴표, 여러 줄 셀, 수식 주입에 안전한 내보내기).
- Excel .xlsx 가져오기/내보내기: 첫 워크시트, SUM/AVERAGE/IF와 사칙연산 수식, 굵게, 색,
  행 소수 서식, 그리고 무엇이 옮겨지는지 보여 주는 가져오기 미리 보기.
- 인터페이스 언어: 영어, 한국어, 일본어. 시스템 언어에 따라 자동으로 선택되고 설정에서 바꿀 수
  있으며, 샘플 통합 문서와 보고서도 선택한 언어를 따릅니다.
- 선택 사항인 AI 도우미: Anthropic 또는 OpenAI 호환 공급자, Windows 자격 증명 관리자에 보관되는 키,
  검증된 편집 제안을 검토한 뒤 한 번에 적용하고 되돌릴 수 있습니다.
- 런타임 DLL과 의존성 라이선스 고지가 포함된 이동식 Windows 패키지.

## 설치 파일(1.2.0)
- **Windows:** `.\package.ps1`이 `release/Julretsu-1.2.0-Windows-Setup.exe`를 만듭니다. 라이선스
  페이지, 시작 메뉴와 바탕 화면 바로 가기, `.julretsu` 파일 연결, 설정 > 앱에서 쓰는 제거 프로그램이
  들어 있는 사용자 단위 단일 파일 설치 프로그램입니다.
- **macOS와 Linux:** `Installers` GitHub Actions 워크플로가 `Julretsu-1.2.0-macOS.dmg`
  (유니버설, macOS 13.3 이상), `Julretsu-1.2.0-linux-amd64.deb`,
  `Julretsu-1.2.0-linux-x86_64.tar.gz`를 만듭니다. `v*` 태그를 푸시하면 초안 릴리스에 첨부됩니다.
- **사용 설명서:** `docs/Julretsu-User-Manual.pdf`(영어), `docs/Julretsu-User-Manual-ko.pdf`(한국어),
  `docs/Julretsu-User-Manual-ja.pdf`(일본어). `docs/manual/`의 HTML에서 생성되며 화면 이미지는
  `julretsu --manual-shots <폴더> --language ko`로 만듭니다. 모든 설치 파일에 세 가지가 모두 들어갑니다.

## 이 Windows 컴퓨터에서 빌드와 테스트
```powershell
.\build.ps1 -Gui -Run
.\build.ps1 -Gui -Smoke
.\build.ps1 -Gui -Configuration Debug
.\package.ps1
```
.tools에 있는 이동식 C++ 도구가 PATH를 영구적으로 바꾸지 않고 자동으로 사용됩니다. .deps에 이미
받아 둔 고정 소스는 다시 사용합니다. 두 폴더는 Git에서 제외됩니다. package.ps1은 로컬
LLVM-MinGW Release 빌드를 대상으로 합니다. 다른 컴파일러의 재배포 요구 사항은 각 패키저가
처리해야 합니다.

헤드리스 코어는 Lua나 그래픽 라이브러리 없이도 빌드됩니다.
```powershell
.\build.ps1 -Example -Benchmarks
```

## 새로 받은 소스 / 다른 플랫폼
CMake 3.20 이상과 C++20 컴파일러(MSVC 2022, GCC 12 이상, Clang 16 이상)를 사용하세요. 코어는
컴파일러 외에 별도 SDK가 필요 없습니다. 네이티브 빌드에는 OpenGL과 플랫폼 윈도 시스템 개발
라이브러리가 필요합니다.

```sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release -DJULRETSU_BUILD_GUI=ON
cmake --build build-native --config Release --parallel 2
ctest --test-dir build-native -C Release --output-on-failure
```

- Linux와 macOS에서는 ./build-native/julretsu를 실행하세요. 헤드리스용 편의 스크립트는
  sh ./build.sh입니다. Linux에서는 GLFW에 xorg-dev, libwayland-dev, libxkbcommon-dev,
  libgl1-mesa-dev가 필요할 수 있습니다(패키지 이름은 배포판마다 다릅니다). 한국어와 일본어
  글자를 표시하려면 fonts-noto-cjk 같은 CJK 글꼴도 설치하세요. macOS에는 Xcode 명령줄 도구가
  필요합니다. Windows에는 OpenGL을 지원하는 그래픽 드라이버가 필요합니다.

| 옵션 | 기본값 | 동작 |
| --- | --- | --- |
| JULRETSU_BUILD_GUI | OFF | 네이티브 앱 빌드, Lua도 함께 활성화 |
| JULRETSU_ENABLE_LUA | OFF | Lua 어댑터와 통합 테스트 빌드 |
| JULRETSU_BUILD_TESTS | ON | 헤드리스 테스트 실행 파일 등록 |
| JULRETSU_BUILD_BENCHMARKS | OFF | 엔진 벤치마크 빌드 |
| JULRETSU_ENABLE_SANITIZERS | OFF | Windows가 아닌 GCC/Clang에서 ASan/UBSan |

현재 대상: julretsu_core, julretsu_ai, julretsu_headless, 선택적으로 julretsu_benchmarks,
julretsu_lua, julretsu_xlsx, julretsu_ui, julretsu(로컬 tests/ 폴더가 있으면 테스트 실행 파일 포함).

## 성능 확인
앱은 --smoke(키보드/마우스 점검과 화면 캡처)와 --benchmark(10,000셀 희소 픽스처)를 지원합니다.
둘 다 기본적으로 창 없이 실행되며 --visible을 붙이면 창이 보입니다. 워밍업 후 VSync를 끈 상태에서
프레임 전체 작업과 화면 전환 시간을 측정합니다. 일반적인 사용에서는 VSync가 켜집니다. 실제 결과와
측정 범위는 승인 보고서를 참고하세요. 보편적인 프레임 속도를 보장하지는 않습니다.

## 소스 구성
- include/julretsu, src: 코어, 수식, 의존성 그래프, LuaEngine, GridUI, GridViewport,
  네이티브 main, 할당 계측, I18n(번역)과 Glyphs(글자 수집).
- tests: 코어 계약, Lua 샌드박스/의존성/매크로 테스트, 뷰포트 경계.
- examples, benchmarks: 헤드리스 예제와 엔진 벤치마크.
- cmake/Dependencies.cmake: 검증된 고정 업스트림 의존성.
- docs: 아키텍처, 수식, Lua, 조작법, 파일 교환, AI 도우미, 승인 보고서, 사용 설명서.
- .github/workflows/ci.yml: 플랫폼별 헤드리스/Lua 빌드와 새니타이저 작업.

[아키텍처](docs/ARCHITECTURE.md), [수식 의미](docs/FORMULAS.md),
[의존성](docs/DEPENDENCIES.md), [남은 작업](docs/ROADMAP.md)도 읽어 보세요.

## 통합 문서 편집 업데이트(0.5)
클립보드 편집, 셀 단위 서식, 수식을 이해하는 드래그 채우기, 행/열 작업, 정렬, 텍스트 필터,
머리글 고정, 여러 네이티브 워크시트, 복구 사본, 차트와 PDF 인쇄 보고서를 사용할 수 있습니다.
창에는 통합 문서 최소화/복원/닫기 단추가 있는 Office 스타일 제목 표시줄과 Excel 스타일로 묶은
홈 리본이 있습니다. 내장 안전망은 흔한 스프레드시트 실수를 잡아냅니다. 위험한 변경(큰 붙여넣기,
일부만 정렬, 덮어쓴 수식, 크기가 맞지 않는 숫자)은 유지하기 전에 검토하고, 실시간 시트 검사는
깨진 수식 패턴, 행을 빠뜨린 합계, 텍스트로 저장된 숫자, 표 안의 빈 행을 알려 주며, 모든 변경은
통합 문서와 함께 저장되는 셀별 기록에 남습니다. 조작법과 현재 호환성 한계는
[빠른 시작](docs/QUICKSTART.md)을 참고하세요. 네이티브 파일은 새 서식과 워크시트 기능을
보존하며, Excel 내보내기는 현재 활성 시트만 지원합니다.
