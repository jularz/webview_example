using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Networking;
using static UnityEngine.EventSystems.EventTrigger;



namespace SMARTGIS
{
    public class BrowserManager
    {
        private MonoBehaviour monoBehaviour;
        private WebViewObject webview;

        // JavaScript Return for MapView Class
        private bool _javaRetCreateMap;
        private bool _javaRetCreateView;
        private bool _javaRetSetView;

        // JavaScript Return for Other Class
        private bool _javaRetSetZoomLevel;
        public int _javaRetViewGetZoomLevel;

        // ReturnCode
        private ReturnCode _retCodeInit;
        private ReturnCode _retCoeSetZoomLevel;


        // WebView Callback variables
        string[] parts;
        string[] partPixels;
        string[] partLonLat;
        string function;
        string result;
        double last_clicked_x;
        double last_clicked_y;



        public BrowserManager(MonoBehaviour monoBehaviour)
        {
            this.monoBehaviour = monoBehaviour;
            webview = (new GameObject("WebViewObjet")).AddComponent<WebViewObject>();
        }

        public MonoBehaviour GetMonoBehaviour()
        {
            return monoBehaviour;
        }

        public IEnumerator InitBrowser(int left, int top, int right, int bottom)
        {
            // webview plugin init
            webview.Init(
                cb: (msg) =>
                {
                    parts = msg.Split(':');
                    if (parts.Length < 2)
                    {
                        result = parts[0];
                    }
                    else
                    {
                        function = parts[0];
                        result = parts[1];
                    }

                    textLog.text = "get Callback : " + function + " " + result;

                    // � �Լ��� ��������� Ȯ���ϰ�, �ش� ������ ����� �����մϴ�.
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
                    var js = "";
                    webview.EvaluateJS(js + @"Unity.call('ua=' + navigator.userAgent)");
                }

            );

            // webview plugin ���ÿ��� �ʱ�ȭ

            webview.SetMargins(left, top, right, bottom);
            webview.SetTextZoom(100);  // android only. cf. https://stackoverflow.com/questions/21647641/android-webview-set-font-size-system-default/47017410#47017410
            webview.SetVisibility(true);



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
                {
                    // NOTE: a more complete code that utilizes UnityWebRequest can be found in https://github.com/gree/unity-webview/commit/2a07e82f760a8495aa3a77a23453f384869caba7#diff-4379160fa4c2a287f414c07eb10ee36d
                    var unityWebRequest = UnityWebRequest.Get(src);
                    yield return unityWebRequest.SendWebRequest();
                    result = unityWebRequest.downloadHandler.data;
                }
                else
                {
                    result = System.IO.File.ReadAllBytes(src);
                }
                System.IO.File.WriteAllBytes(dst, result);
                if (ext == ".html")
                {
                    webview.LoadURL("file://" + dst.Replace(" ", "%20"));
                    break;
                }
            }
        }

        public IEnumerator CoroutineCreateMap()
        {
            _javaRetCreateMap = false;
            webview.EvaluateJS($"map.create");
            yield return new WaitUntil(() => _javaRetCreateMap != false);
        }

        public IEnumerator CoroutineCreateView(double centerX, double centerY, int zoomLevel, Projection.Kind projection)
        {
            _javaRetCreateMap = false;
            webview.EvaluateJS($"view.create(" + centerX.ToString() + ", " + centerY.ToString() + ", " + zoomLevel.ToString() + ", " + projection.ToString() + ")");
            yield return new WaitUntil(() => _javaRetCreateView != false);
        }

        public IEnumerator CoroutineSetView(int viewNativeId)
        {
            _javaRetSetView = false;
            webview.EvaluateJS($"map.set_view(" + viewNativeId.ToString() + ")");
            yield return new WaitUntil(() => _javaRetSetView != false);
        }

    }

}