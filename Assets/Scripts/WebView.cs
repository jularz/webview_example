/*
 * Copyright (C) 2012 GREE, Inc.
 * 
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

using System.Collections;
using UnityEngine;
#if UNITY_2018_4_OR_NEWER
using UnityEngine.Networking;
#endif
using UnityEngine.UI;

public class SampleWebView : MonoBehaviour
{
    public string Url;
    public Text status;
    WebViewObject webViewObject;

    private bool initRet;
    private bool zoomLevelRet;
    private bool centerLonLatRet;
    private bool addSymbolRet;
    private bool updateSymbolRet;
    private bool removeSymbolRet;

    private int symbolID = -1;

    string[] parts;
    string[] partPixels;
    string[] partLonLat;

    string function;
    string result;

    double last_clicked_x;
    double last_clicked_y;

    public Button buttonInit;  // 버튼에 대한 참조
    public Button buttonSetZoom;  // 버튼에 대한 참조
    public Button buttonSetCenter;  // 버튼에 대한 참조
    public Button buttonAddSymbol;
    public Button buttonUpdateSymbol;
    public Button buttonRemoveSymbol;
    
    public Text textLog;
    public Text textPosition;

    void OnButtonInitClick()
    {
        // 버튼이 클릭되면 convert_lonlat 코루틴을 시작합니다.
        webViewObject.SetMargins(5, 100, 5, Screen.height / 2);
        StartCoroutine(CallInit());
    }

    void OnButtonSetZoomClick()
    {
        // 버튼이 클릭되면 convert_lonlat 코루틴을 시작합니다.
        StartCoroutine(CallSetZoom());
    }

    void OnButtonSetCenterClick()
    {
        // 버튼이 클릭되면 convert_lonlat 코루틴을 시작합니다.
        StartCoroutine(CallSetCenter());
    }

    void OnButtonAddSymbolClick()
    {
        // 버튼이 클릭되면 convert_lonlat 코루틴을 시작합니다.
        StartCoroutine(CallAddSymbol());
    }

    void OnButtonUpdateSymbolClick()
    {
        // 버튼이 클릭되면 convert_lonlat 코루틴을 시작합니다.
        StartCoroutine(CallUpdateSymbol());
    }

    void OnButtonRemoveSymbolClick()
    {
        // 버튼이 클릭되면 convert_lonlat 코루틴을 시작합니다.
        StartCoroutine(CallRemoveSymbol());
    }



    public IEnumerator CallInit()
    {
        initRet = false;
        // JavaScript의 convert_mgrs 함수를 호출합니다.
        webViewObject.EvaluateJS($"init()");
        // JavaScript에서 convert_mgrs의 결과를 받을 때까지 기다립니다.
        yield return new WaitUntil(() => initRet != false);
    }

    
    public IEnumerator CallSetZoom()
    {
        zoomLevelRet = false;
        webViewObject.EvaluateJS($"setZoomLevel(15)");
        yield return new WaitUntil(() => zoomLevelRet != false);
    }

    public IEnumerator CallSetCenter()
    {
        centerLonLatRet = false;
        webViewObject.EvaluateJS($"setCenterLonLat(" + last_clicked_x.ToString() + ", "+ last_clicked_y.ToString() + ")");
        yield return new WaitUntil(() => centerLonLatRet != false);
    }

    public IEnumerator CallAddSymbol()
    {
        addSymbolRet = false;
        symbolID = 1;
        webViewObject.EvaluateJS($"addSymbol(" + last_clicked_x.ToString() + ", "
                                                + last_clicked_y.ToString() 
                                                + ", 'file:///data/data/com.DefaultCompany.android_build_test_webviewUI/files/target.png',"
                                                + symbolID.ToString() 
                                    + ")"
                                );
        yield return new WaitUntil(() => addSymbolRet != false);
    }

    public IEnumerator CallUpdateSymbol()
    {
        updateSymbolRet = false;
        symbolID = 1;
        webViewObject.EvaluateJS($"updateSymbol(" + last_clicked_x.ToString() + ", "+ last_clicked_y.ToString() + ", "+ symbolID.ToString() + ")");
        yield return new WaitUntil(() => updateSymbolRet != false);
    }

    public IEnumerator CallRemoveSymbol()
    {
        removeSymbolRet = false;
        symbolID = 1;
        webViewObject.EvaluateJS($"removeSymbol("+ symbolID.ToString() + ")");
        yield return new WaitUntil(() => removeSymbolRet != false);
    }

    public void update_last_clicked_position(string input)
    {
        string[] positions = input.Split(',');
        last_clicked_x = double.Parse(positions[0]);
        last_clicked_y = double.Parse(positions[1]);

        textPosition.text = "x : " + last_clicked_x.ToString() + " | y : " + last_clicked_y.ToString();
    }
/*
    public IEnumerator CallTranslateLonlatToPixel()
    {
        // 결과를 초기화합니다.
        centerLonLatRet = false;

        // JavaScript의 convert_lonlat 함수를 호출합니다.
        webViewObject.EvaluateJS($"convertLonLatToPixel(127.10, 37.42)");

        // JavaScript에서 convert_lonlat의 결과를 받을 때까지 기다립니다.
        yield return new WaitUntil(() => partPixels != null);

        string pixelX = partPixels[0];
        string pixelY = partPixels[1];
    }

    public IEnumerator CallTranslatePixelToLonlat()
    {
        // 결과를 초기화합니다.
        centerLonLatRet = false;

        // JavaScript의 convert_lonlat 함수를 호출합니다.
        webViewObject.EvaluateJS($"convertPixelToLonLat(0,0)");

        // JavaScript에서 convert_lonlat의 결과를 받을 때까지 기다립니다.
        yield return new WaitUntil(() => partLonLat != null);

        string lonlatX = partLonLat[0];
        string lonlatY = partLonLat[1];
    }
*/
    

    IEnumerator Start()
    {
        textLog.text = "";
        textPosition.text = "";

        buttonInit.onClick.AddListener(OnButtonInitClick);
        buttonSetZoom.onClick.AddListener(OnButtonSetZoomClick);
        buttonSetCenter.onClick.AddListener(OnButtonSetCenterClick);
        buttonAddSymbol.onClick.AddListener(OnButtonAddSymbolClick);
        buttonUpdateSymbol.onClick.AddListener(OnButtonUpdateSymbolClick);
        buttonRemoveSymbol.onClick.AddListener(OnButtonRemoveSymbolClick);
        //buttonInit.onClick.AddListener(OnButtonClick);


        webViewObject = (new GameObject("WebViewObject")).AddComponent<WebViewObject>();
        webViewObject.Init(
            cb: (msg) =>
            {
                //Debug.Log(string.Format("CallFromJS[{0}]", msg));
                //status.text = msg;
                //status.GetComponent<Animation>().Play();

                // JavaScript에서 보낸 메시지를 처리합니다.
                parts = msg.Split(':');
                if (parts.Length < 2)
                {
                    // 적절한 오류 메시지 출력 또는 오류 처리
                    result = parts[0];
                }
                else
                {
                    function = parts[0];
                    result = parts[1];
                }

                textLog.text = "get Callback : " + function + " " + result;

                // 어떤 함수의 결과인지를 확인하고, 해당 변수에 결과를 저장합니다.
                if (function == "init")
                {
                    initRet = true;
                }
                else if (function == "setZoomLevel")
                {
                    zoomLevelRet = true;
                }
                else if (function == "setCenterLonLat")
                {
                    centerLonLatRet = true;
                }
                else if (function == "click")
                {
                    update_last_clicked_position(result);
                }
                else if (function == "addSymbol")
                {
                    addSymbolRet = true;    
                }
                else if (function == "updateSymbol")
                {
                    updateSymbolRet = true;    
                }
                else if (function == "removeSymbol")
                {
                    removeSymbolRet = true;    
                }
                /*
                else if (function == "convertLonLatToPixel")
                {
                    partPixels = result.Split(',');
                }
                else if (function == "convertPixelToLonLat")
                {
                    partLonLat = result.Split(',');
                }
                */
                else{
                    Debug.Log($"[B] onMouseLeftClicked");
                }
            },
            err: (msg) =>
            {
                Debug.Log(string.Format("CallOnError[{0}]", msg));
                status.text = msg;
                status.GetComponent<Animation>().Play();
            },
            httpErr: (msg) =>
            {
                Debug.Log(string.Format("CallOnHttpError[{0}]", msg));
                status.text = msg;
                status.GetComponent<Animation>().Play();
            },
            started: (msg) =>
            {
                Debug.Log(string.Format("CallOnStarted[{0}]", msg));
            },
            hooked: (msg) =>
            {
                Debug.Log(string.Format("CallOnHooked[{0}]", msg));
            },
            cookies: (msg) =>
            {
                Debug.Log(string.Format("CallOnCookies[{0}]", msg));
            },
            ld: (msg) =>
            {
                Debug.Log(string.Format("CallOnLoaded[{0}]", msg));
#if UNITY_EDITOR_OSX || UNITY_STANDALONE_OSX || UNITY_IOS
                // NOTE: the following js definition is required only for UIWebView; if
                // enabledWKWebView is true and runtime has WKWebView, Unity.call is defined
                // directly by the native plugin.
#if true
                var js = @"
                    if (!(window.webkit && window.webkit.messageHandlers)) {
                        window.Unity = {
                            call: function(msg) {
                                window.location = 'unity:' + msg;
                            }
                        };
                    }
                ";
#else
                // NOTE: depending on the situation, you might prefer this 'iframe' approach.
                // cf. https://github.com/gree/unity-webview/issues/189
                var js = @"
                    if (!(window.webkit && window.webkit.messageHandlers)) {
                        window.Unity = {
                            call: function(msg) {
                                var iframe = document.createElement('IFRAME');
                                iframe.setAttribute('src', 'unity:' + msg);
                                document.documentElement.appendChild(iframe);
                                iframe.parentNode.removeChild(iframe);
                                iframe = null;
                            }
                        };
                    }
                ";
#endif
#elif UNITY_WEBPLAYER || UNITY_WEBGL
                var js = @"
                    window.Unity = {
                        call:function(msg) {
                            parent.unityWebView.sendMessage('WebViewObject', msg);
                        }
                    };
                ";
#else
                var js = "";
#endif
                webViewObject.EvaluateJS(js + @"Unity.call('ua=' + navigator.userAgent)");
            }
            //transparent: false,
            //zoom: true,
            //ua: "custom user agent string",
            //radius: 0,  // rounded corner radius in pixel
            //// android
            //androidForceDarkMode: 0,  // 0: follow system setting, 1: force dark off, 2: force dark on
            //// ios
            //enableWKWebView: true,
            //wkContentMode: 0,  // 0: recommended, 1: mobile, 2: desktop
            //wkAllowsLinkPreview: true,
            //// editor
            //separated: false
            );
#if UNITY_EDITOR_OSX || UNITY_STANDALONE_OSX
        webViewObject.bitmapRefreshCycle = 1;
#endif
        // cf. https://github.com/gree/unity-webview/pull/512
        // Added alertDialogEnabled flag to enable/disable alert/confirm/prompt dialogs. by KojiNakamaru · Pull Request #512 · gree/unity-webview
        //webViewObject.SetAlertDialogEnabled(false);

        // cf. https://github.com/gree/unity-webview/pull/728
        //webViewObject.SetCameraAccess(true);
        //webViewObject.SetMicrophoneAccess(true);

        // cf. https://github.com/gree/unity-webview/pull/550
        // introduced SetURLPattern(..., hookPattern). by KojiNakamaru · Pull Request #550 · gree/unity-webview
        //webViewObject.SetURLPattern("", "^https://.*youtube.com", "^https://.*google.com");

        // cf. https://github.com/gree/unity-webview/pull/570
        // Add BASIC authentication feature (Android and iOS with WKWebView only) by takeh1k0 · Pull Request #570 · gree/unity-webview
        //webViewObject.SetBasicAuthInfo("id", "password");

        //webViewObject.SetScrollbarsVisibility(true);

        //webViewObject.SetMargins(5, 100, 5, Screen.height / 2);
        webViewObject.SetMargins(500, 500, 500, Screen.height / 2);
        webViewObject.SetTextZoom(100);  // android only. cf. https://stackoverflow.com/questions/21647641/android-webview-set-font-size-system-default/47017410#47017410
        webViewObject.SetVisibility(true);

#if !UNITY_WEBPLAYER && !UNITY_WEBGL
        if (Url.StartsWith("http")) {
            webViewObject.LoadURL(Url.Replace(" ", "%20"));
        } else {
            var exts = new string[]{
                ".jpg",
                ".js",
                ".html"  // should be last
            };
            foreach (var ext in exts) {
                //var url = Url.Replace(".html", ext);
                //var url = "sample.html";
                var url = "index.html";
                var src = System.IO.Path.Combine(Application.streamingAssetsPath, url);
                var dst = System.IO.Path.Combine(Application.temporaryCachePath, url);
                byte[] result = null;
                if (src.Contains("://")) {  // for Android
#if UNITY_2018_4_OR_NEWER
                    // NOTE: a more complete code that utilizes UnityWebRequest can be found in https://github.com/gree/unity-webview/commit/2a07e82f760a8495aa3a77a23453f384869caba7#diff-4379160fa4c2a287f414c07eb10ee36d
                    var unityWebRequest = UnityWebRequest.Get(src);
                    yield return unityWebRequest.SendWebRequest();
                    result = unityWebRequest.downloadHandler.data;
#else
                    var www = new WWW(src);
                    yield return www;
                    result = www.bytes;
#endif
                } else {
                    result = System.IO.File.ReadAllBytes(src);
                }
                System.IO.File.WriteAllBytes(dst, result);
                if (ext == ".html") {
                    webViewObject.LoadURL("file://" + dst.Replace(" ", "%20"));
                    break;
                }
            }
        }
#else
        if (Url.StartsWith("http")) {
            webViewObject.LoadURL(Url.Replace(" ", "%20"));
        } else {
            webViewObject.LoadURL("StreamingAssets/" + Url.Replace(" ", "%20"));
        }
#endif
        yield break;
    }

    void Update()
    {
        if(Input.touchCount>0)
        {
            Touch touch = Input.GetTouch(0);
            Vector2 touchPosition = touch.position;

            textLog.text = "Touch : " + touchPosition;

            Debug.Log("Touch Position: "+ touchPosition);
        }
    }

    void OnGUI()
    {
        var x = 10;

        GUI.enabled = (webViewObject == null) ? false : webViewObject.CanGoBack();
        if (GUI.Button(new Rect(x, 10, 80, 80), "<")) {
            webViewObject?.GoBack();
        }
        GUI.enabled = true;
        x += 90;

        GUI.enabled = (webViewObject == null) ? false : webViewObject.CanGoForward();
        if (GUI.Button(new Rect(x, 10, 80, 80), ">")) {
            webViewObject?.GoForward();
        }
        GUI.enabled = true;
        x += 90;

        if (GUI.Button(new Rect(x, 10, 80, 80), "r")) {
            webViewObject?.Reload();
        }
        x += 90;

        GUI.TextField(new Rect(x, 10, 180, 80), "" + ((webViewObject == null) ? 0 : webViewObject.Progress()));
        x += 190;

        if (GUI.Button(new Rect(x, 10, 80, 80), "*")) {
            var g = GameObject.Find("WebViewObject");
            if (g != null) {
                Destroy(g);
            } else {
                StartCoroutine(Start());
            }
        }
        x += 90;

        if (GUI.Button(new Rect(x, 10, 80, 80), "c")) {
            webViewObject?.GetCookies(Url);
        }
        x += 90;

        if (GUI.Button(new Rect(x, 10, 80, 80), "x")) {
            webViewObject?.ClearCookies();
        }
        x += 90;

        if (GUI.Button(new Rect(x, 10, 80, 80), "D")) {
            webViewObject?.SetInteractionEnabled(false);
        }
        x += 90;

        if (GUI.Button(new Rect(x, 10, 80, 80), "E")) {
            webViewObject?.SetInteractionEnabled(true);
        }
        x += 90;
    }
}
