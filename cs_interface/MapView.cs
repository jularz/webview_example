using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class MapView
{
    private WebViewObject webviewObject;
    private MonoBehaviour monoBehaviour;

    private bool _javaRetInit;
    private bool _javaRetSetZoomLevel;
    private bool _javaRetSetCenterLonLat;
    private bool _javaRetAddSymbol;
    private bool _javaRetUpdateSymbol;
    private bool _javaRetRemoveSymbol;

    private ReturnCode _retCodeInit;
    private ReturnCode _retCodeSetZoomLevel;
    private ReturnCode _retCodeSetCenterLonLat;
    private ReturnCode _retCodeAddSymbol;
    private ReturnCode _retCodeUpdateSymbol;
    private ReturnCode _retCodeRemoveSymbol;

    string[] parts;
    string[] partPixels;
    string[] partLonLat;

    string function;
    string result;

    double last_clicked_x;
    double last_clicked_y;


    public bool toggle = true;

    public MapView(MonoBehaviour monoBehaviour)
    {
        this.monoBehaviour = monoBehaviour;
        webViewObject = (new GameObject("WebViewObject")).AddComponent<WebViewObject>();
    }

    public ~MapView()
    {

    }

    public ReturnCode Init(int left, int top, int right, int bottom)
    {
        // WebViewPlugin Init callback 함수 coroutine 처리용
        monoBehaviour.StartCoroutine(CoroutineInit(left, top, right, bottom));

        // Javascript 영역 초기화 호출
        //monoBehaviour.StartCoroutine(CallInit());
        var resultWrapper = new CoroutineResult<ReturnCode>(monoBehaviour, CallInit());
        yield return resultWrapper.Coroutine;

        return resultWrapper.Result;
    }

    public ReturnCode SetZoom(int zoom)
    {
        var result Wrapper = new CoroutineResult<REturnCode>(monoBehaviour, CoroutineSetZoom(zoom));
        yield return 

    }

    private IEnumerator CoroutineInit(int left, int top, int right, int bottom)
    {
        // webview plugin init
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
                else
                {
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
            
        );

        // webview plugin 전시영역 초기화

        webViewObject.SetMargins(left, top, right, bottom);
        webViewObject.SetTextZoom(100);  // android only. cf. https://stackoverflow.com/questions/21647641/android-webview-set-font-size-system-default/47017410#47017410
        webViewObject.SetVisibility(true);

        
        // Webview - javascript bundle 연결 영역
        if (Url.StartsWith("http"))
        {
            webViewObject.LoadURL(Url.Replace(" ", "%20"));
        }
        else
        {
            var exts = new string[]{
                ".jpg",
                ".js",
                ".html"  // should be last
            };
            foreach (var ext in exts)
            {
                //var url = Url.Replace(".html", ext);
                //var url = "sample.html";
                var url = "index.html";
                var src = System.IO.Path.Combine(Application.streamingAssetsPath, url);
                var dst = System.IO.Path.Combine(Application.temporaryCachePath, url);
                byte[] result = null;
                if (src.Contains("://"))
                {  // for Android
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
                }
                else
                {
                    result = System.IO.File.ReadAllBytes(src);
                }
                System.IO.File.WriteAllBytes(dst, result);
                if (ext == ".html")
                {
                    webViewObject.LoadURL("file://" + dst.Replace(" ", "%20"));
                    break;
                }
            }
        }

        yield break;
    }


    public IEnumerator CallInit()
    {
        ReturnCode result = /* 조건 */;

        initRet = false;
        // JavaScript의 convert_mgrs 함수를 호출합니다.
        webViewObject.EvaluateJS($"init()");
        // JavaScript에서 convert_mgrs의 결과를 받을 때까지 기다립니다.
        yield return new WaitUntil(() => initRet != false);

        if (initRet == 0)
            result = ReturnCode.ERROR;
        else
            result = ReturnCode.OK;

        yield return result;
    }


    public IEnumerator CoroutineSetZoom()
    {
        zoomLevelRet = false;
        webViewObject.EvaluateJS($"setZoomLevel(15)");
        yield return new WaitUntil(() => zoomLevelRet != false);
    }

    public IEnumerator CallSetCenter()
    {
        centerLonLatRet = false;
        webViewObject.EvaluateJS($"setCenterLonLat(" + last_clicked_x.ToString() + ", " + last_clicked_y.ToString() + ")");
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
        webViewObject.EvaluateJS($"updateSymbol(" + last_clicked_x.ToString() + ", " + last_clicked_y.ToString() + ", " + symbolID.ToString() + ")");
        yield return new WaitUntil(() => updateSymbolRet != false);
    }

    public IEnumerator CallRemoveSymbol()
    {
        removeSymbolRet = false;
        symbolID = 1;
        webViewObject.EvaluateJS($"removeSymbol(" + symbolID.ToString() + ")");
        yield return new WaitUntil(() => removeSymbolRet != false);
    }

    public void update_last_clicked_position(string input)
    {
        string[] positions = input.Split(',');
        last_clicked_x = double.Parse(positions[0]);
        last_clicked_y = double.Parse(positions[1]);
    }
}
