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

    MapView mapview;
    

    private int symbolID = -1;


    public Button buttonInit;  // 버튼에 대한 참조
    public Button buttonSetZoom;  // 버튼에 대한 참조
    public Button buttonSetCenter;  // 버튼에 대한 참조
    public Button buttonAddSymbol;
    public Button buttonUpdateSymbol;
    public Button buttonRemoveSymbol;

    public Button buttonVisibility;

    public Text textLog;
    public Text textPosition;


    void OnButtonInitClick()
    {
        // 버튼이 클릭되면 convert_lonlat 코루틴을 시작합니다.
        mapview.init(5, 100, 5, Screen.height / 2);
    }

    void OnButtonSetZoomClick()
    {
        // 버튼이 클릭되면 convert_lonlat 코루틴을 시작합니다.
        mapview.setZoomLevel(15);
    }

    void OnButtonSetCenterClick()
    {
        // 버튼이 클릭되면 convert_lonlat 코루틴을 시작합니다.
        mapview.setCenter(last_clicked_x, last_clicked_y);
    }

    void OnButtonAddSymbolClick()
    {
        // 버튼이 클릭되면 convert_lonlat 코루틴을 시작합니다.
        mapview.addSymbol(symbolID);
    }

    void OnButtonUpdateSymbolClick()
    {
        // 버튼이 클릭되면 convert_lonlat 코루틴을 시작합니다.
        mapview.updateSymbol(symbolID);
    }

    void OnButtonRemoveSymbolClick()
    {
        // 버튼이 클릭되면 convert_lonlat 코루틴을 시작합니다.
        mapview.removeSymbol(symbolID);
    }

    void OnButtonVisibilityClick()
    {
        mapview.setVisibility(true);
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


    void Start()
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

        buttonVisibility.onClick.AddListener(OnButtonVisibilityClick);

        mapview = new MapView(this);

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
