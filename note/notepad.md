notepad

# Unity android build

- Build Settings 진입
- 'Switch Platform' to android
- Palyer settings 진입
- Minimum API level 33으로 설정(임시로)
- Build 실행 눌러서 APK tool chain 업데이트 수행
- 다시 Minimum API Level을 요구사항에 맞춰 수정

# map tile, js, html path

- data/data/com.~~~./files/index.html
- data/data/com.~~~./files/main.js
- data/data/com.~~~./files/map_data
- html 에서 스크립트 호출 영역을 아래와 같이 실제 경로를 삽입 -> 추후 동적으로 path를 변경하거나, external path를 활용하는 방법을 적용할 필요가 있음
  <script src="file:///data/data/com.DefaultCompany.android_build_test_webviewUI/files/bundle.js"></script>
- script 내 에서 map_data 접근도 동일한 경로를 사용
  url: 'file:///data/data/com.DefaultCompany.android_build_test_webviewUI/files/map_data/{z}/{x}/{y}.png', // XYZ 타일의 경로를 지정
- 현재 webview.cs 는 streaming assets의 index.html 을 사용하는데, 지금 수준으로는 이 편이 더 편할 수 있음
- Streaming Assets 에 포함되어 빌드될 경우, apk 내에 압축되어 들어가게 되며, 수정할 수 있음
