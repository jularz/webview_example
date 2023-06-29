window.addEventListener(
    'load',
    function() {
        document.body.style.backgroundColor = 'white';
        window.setTimeout(
            function() {
                document.body.style.backgroundColor = '#ABEBC6';
                var msg = document.getElementById("msg");
                msg.textContent = '(NOTE: the background color was changed by sample.js, for checking whether the external js code works)';
            },
            3000);
    });
function testSaveDataURL() {
    var canvas = document.createElement('canvas');
    canvas.width = 320;
    canvas.height = 240;
    var ctx = canvas.getContext("2d");
    ctx.fillStyle = "#fffaf0";
    ctx.fillRect(0, 0, 320, 240);
    ctx.fillStyle = "#000000";
    ctx.font = "48px serif";
    ctx.strokeText("Hello, world", 40, 132);
    Unity.saveDataURL("test.png", canvas.toDataURL());
    // NOTE: Unity.saveDataURL() for iOS cannot save a file under the common Downloads folder.
    // cf. https://github.com/gree/unity-webview/pull/904#issue-1650406563
};


// convert_mgrs 함수
function convert_mgrs(input) {
    // ... (input을 처리하는 코드) ...

    var result = "ABCD";  // 예시 결과

    // Unity로 결과를 보냅니다.
    Unity.call("convert_mgrs:" + result);
}

// convert_lonlat 함수
function convert_lonlat(input) {
    // ... (input을 처리하는 코드) ...

    var result = "100,200";  // 예시 결과

    // Unity로 결과를 보냅니다.
    Unity.call("convert_lonlat:" + result);
}
/*
// Heartbeat 메시지를 보냅니다.
setInterval(function () {
    Unity.call("Heartbeat:interval_1000");
}, 1000);
*/
