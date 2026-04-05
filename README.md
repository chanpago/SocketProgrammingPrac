# SocketProgrammingPrac
소켓프로그래밍 연습

## 1일차 작업 내용
- 기존 콘솔 기반 소켓 서버를 Win32 + DirectX 11 + ImGui 구조로 전환
- ImGui 라이브러리를 `ThirdParty/ImGui`에 추가하고 프로젝트에 연결
- DirectX 11 렌더링 기반과 ImGui 모니터링 창을 구성
- 소켓 서버를 별도 스레드로 분리해 UI와 동시에 동작하도록 변경
- 서버 상태, 접속 수, 로그를 ImGui 창에서 확인할 수 있도록 구현
- `SocketProgramming.cpp`에 몰려 있던 코드를 아래 클래스로 분리
  - `MainApp`
  - `SocketServer`
  - `D3D11Context`
  - `ServerMonitorUI`
- Visual Studio 솔루션 탐색기 필터를 정리해 `App`, `Server`, `Renderer`, `UI`, `ThirdParty/ImGui` 구조로 관리하도록 수정
- `.gitignore`에 `.claude/`, `docs/`를 추가해 로컬 설정/문서 폴더가 추적되지 않도록 설정

## 현재 실행 방식
- 실행 파일 실행 시 Win32 창이 열리고 ImGui 기반 서버 모니터가 표시됨
- 브라우저에서 `http://127.0.0.1:9000/` 접속 시 서버 요청이 처리되고 로그가 UI에 표시됨

## 비고
- Debug/x64 기준 빌드 성공 확인
- 출력 파일: `SocketProgramming/x64/Debug/SocketProgramming.exe`

## 참고 문서
- `docs/imgui_dx11_integration_summary.md`
