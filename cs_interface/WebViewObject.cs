/*
 * Copyright (C) 2011 Keijiro Takahashi
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

using UnityEngine;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.Runtime.InteropServices;
using UnityEngine.Android;
using System.Threading;
using System.Threading.Tasks;

using Callback = System.Action<string>;

[System.Serializable]
public class ResultData
{
    public long callbackId;
    public string result;
}


public class WebViewObject : MonoBehaviour
{
    Callback onJS;
    Callback onError;
    Callback onHttpError;
    Callback onStarted;
    Callback onLoaded;
    Callback onHooked;
    Callback onCookies;
    bool visibility;
    bool alertDialogEnabled;
    bool scrollBounceEnabled;
    int mMarginLeft;
    int mMarginTop;
    int mMarginRight;
    int mMarginBottom;
    bool mMarginRelative;
    float mMarginLeftComputed;
    float mMarginTopComputed;
    float mMarginRightComputed;
    float mMarginBottomComputed;
    bool mMarginRelativeComputed;

    AndroidJavaObject webView;
    
    bool mVisibility;
    int mKeyboardVisibleHeight;
    int mWindowVisibleDisplayFrameHeight;
    float mResumedTimestamp;

#if UNITYWEBVIEW_ANDROID_ENABLE_NAVIGATOR_ONLINE
    float androidNetworkReachabilityCheckT0 = -1.0f;
    NetworkReachability? androidNetworkReachability0 = null;
#endif
    
    void OnApplicationPause(bool paused)
    {
        if (webView == null)
            return;
        if (!paused && mKeyboardVisibleHeight > 0)
        {
            webView.Call("SetVisibility", false);
            mResumedTimestamp = Time.realtimeSinceStartup;
        }
        webView.Call("OnApplicationPause", paused);
    }

    void Update()
    {
        if (webView == null)
            return;
#if UNITYWEBVIEW_ANDROID_ENABLE_NAVIGATOR_ONLINE
        var t = Time.time;
        if (t - 1.0f >= androidNetworkReachabilityCheckT0)
        {
            androidNetworkReachabilityCheckT0 = t;
            var androidNetworkReachability = Application.internetReachability;
            if (androidNetworkReachability0 != androidNetworkReachability)
            {
                androidNetworkReachability0 = androidNetworkReachability;
                webView.Call("SetNetworkAvailable", androidNetworkReachability != NetworkReachability.NotReachable);
            }
        }
#endif
        if (mResumedTimestamp != 0.0f && Time.realtimeSinceStartup - mResumedTimestamp > 0.5f)
        {
            mResumedTimestamp = 0.0f;
            webView.Call("SetVisibility", mVisibility);
        }
        for (;;) {
            if (webView == null)
                break;
            var s = webView.Call<String>("GetMessage");
            if (s == null)
                break;
            var i = s.IndexOf(':', 0);
            if (i == -1)
                continue;
            switch (s.Substring(0, i)) {
            case "CallFromJS":
                CallFromJS(s.Substring(i + 1));
                break;
            case "CallOnError":
                CallOnError(s.Substring(i + 1));
                break;
            case "CallOnHttpError":
                CallOnHttpError(s.Substring(i + 1));
                break;
            case "CallOnLoaded":
                CallOnLoaded(s.Substring(i + 1));
                break;
            case "CallOnStarted":
                CallOnStarted(s.Substring(i + 1));
                break;
            case "CallOnHooked":
                CallOnHooked(s.Substring(i + 1));
                break;
            case "CallOnCookies":
                CallOnCookies(s.Substring(i + 1));
                break;
            case "SetKeyboardVisible":
                SetKeyboardVisible(s.Substring(i + 1));
                break;
            case "RequestFileChooserPermissions":
                RequestFileChooserPermissions();
                break;
            }
        }
    }

    /// Called from Java native plugin to set when the keyboard is opened
    public void SetKeyboardVisible(string keyboardVisibleHeight)
    {
        if (BottomAdjustmentDisabled())
        {
            return;
        }
        var keyboardVisibleHeight0 = mKeyboardVisibleHeight;
        var keyboardVisibleHeight1 = Int32.Parse(keyboardVisibleHeight);
        if (keyboardVisibleHeight0 != keyboardVisibleHeight1)
        {
            mKeyboardVisibleHeight = keyboardVisibleHeight1;
            SetMargins(mMarginLeft, mMarginTop, mMarginRight, mMarginBottom, mMarginRelative);
        }
    }
    
    /// Called from Java native plugin to request permissions for the file chooser.
    public void RequestFileChooserPermissions()
    {
        var permissions = new List<string>();
        if (!Permission.HasUserAuthorizedPermission(Permission.ExternalStorageRead))
        {
            permissions.Add(Permission.ExternalStorageRead);
        }
        if (!Permission.HasUserAuthorizedPermission(Permission.ExternalStorageWrite))
        {
            permissions.Add(Permission.ExternalStorageWrite);
        }
        if (!Permission.HasUserAuthorizedPermission(Permission.Camera))
        {
            permissions.Add(Permission.Camera);
        }
        if (permissions.Count > 0)
        {
#if UNITY_2020_2_OR_NEWER
            var grantedCount = 0;
            var deniedCount = 0;
            var callbacks = new PermissionCallbacks();
            callbacks.PermissionGranted += (permission) =>
            {
                grantedCount++;
                if (grantedCount + deniedCount == permissions.Count)
                {
                    webView.Call("OnRequestFileChooserPermissionsResult", grantedCount == permissions.Count);
                }
            };
            callbacks.PermissionDenied += (permission) =>
            {
                deniedCount++;
                if (grantedCount + deniedCount == permissions.Count)
                {
                    webView.Call("OnRequestFileChooserPermissionsResult", grantedCount == permissions.Count);
                }
            };
            callbacks.PermissionDeniedAndDontAskAgain += (permission) =>
            {
                deniedCount++;
                if (grantedCount + deniedCount == permissions.Count)
                {
                    webView.Call("OnRequestFileChooserPermissionsResult", grantedCount == permissions.Count);
                }
            };
            Permission.RequestUserPermissions(permissions.ToArray(), callbacks);
#else
            StartCoroutine(RequestFileChooserPermissionsCoroutine(permissions.ToArray()));
#endif
        }
        else
        {
            webView.Call("OnRequestFileChooserPermissionsResult", true);
        }
    }

#if UNITY_2020_2_OR_NEWER
#else
    int mRequestPermissionPhase;

    IEnumerator RequestFileChooserPermissionsCoroutine(string[] permissions)
    {
        foreach (var permission in permissions)
        {
            mRequestPermissionPhase = 0;
            Permission.RequestUserPermission(permission);
            // waiting permission dialog that may not be opened.
            for (var i = 0; i < 8 && mRequestPermissionPhase == 0; i++)
            {
                yield return new WaitForSeconds(0.25f);
            }
            if (mRequestPermissionPhase == 0)
            {
                // permission dialog was not opened.
                continue;
            }
            while (mRequestPermissionPhase == 1)
            {
                yield return new WaitForSeconds(0.3f);
            }
        }
        yield return new WaitForSeconds(0.3f);
        var granted = 0;
        foreach (var permission in permissions)
        {
            if (Permission.HasUserAuthorizedPermission(permission))
            {
                granted++;
            }
        }
        webView.Call("OnRequestFileChooserPermissionsResult", granted == permissions.Length);
    }

    void OnApplicationFocus(bool hasFocus)
    {
        if (hasFocus)
        {
            if (mRequestPermissionPhase == 1)
            {
                mRequestPermissionPhase = 2;
            }
        }
        else
        {
            if (mRequestPermissionPhase == 0)
            {
                mRequestPermissionPhase = 1;
            }
        }
    }
#endif

    public int AdjustBottomMargin(int bottom)
    {
        if (BottomAdjustmentDisabled())
        {
            return bottom;
        }
        else if (mKeyboardVisibleHeight <= 0)
        {
            return bottom;
        }
        else
        {
            int keyboardHeight = 0;
            using(AndroidJavaClass UnityClass = new AndroidJavaClass("com.unity3d.player.UnityPlayer"))
            {
                AndroidJavaObject View = UnityClass.GetStatic<AndroidJavaObject>("currentActivity").Get<AndroidJavaObject>("mUnityPlayer").Call<AndroidJavaObject>("getView");
                using(AndroidJavaObject Rct = new AndroidJavaObject("android.graphics.Rect"))
                {
                    View.Call("getWindowVisibleDisplayFrame", Rct);
                    keyboardHeight = mWindowVisibleDisplayFrameHeight - Rct.Call<int>("height");
                }
            }
            return (bottom > keyboardHeight) ? bottom : keyboardHeight;
        }
    }

    private bool BottomAdjustmentDisabled()
    {
        return
            !Screen.fullScreen
            || ((Screen.autorotateToLandscapeLeft || Screen.autorotateToLandscapeRight)
                && (Screen.autorotateToPortrait || Screen.autorotateToPortraitUpsideDown));
    }


    void Awake()
    {
        alertDialogEnabled = true;
        scrollBounceEnabled = true;
        mMarginLeftComputed = -9999;
        mMarginTopComputed = -9999;
        mMarginRightComputed = -9999;
        mMarginBottomComputed = -9999;
    }

    public bool IsKeyboardVisible
    {
        get
        {
            return mKeyboardVisibleHeight > 0;
        }
    }


    public static bool IsWebViewAvailable()
    {
        return (new AndroidJavaObject("net.gree.unitywebview.CWebViewPlugin")).CallStatic<bool>("IsWebViewAvailable");
    }

    public void Init(
        Callback cb = null,
        Callback err = null,
        Callback httpErr = null,
        Callback ld = null,
        Callback started = null,
        Callback hooked = null,
        Callback cookies = null,
        bool transparent = false,
        bool zoom = true,
        string ua = "",
        int radius = 0,
        // android
        int androidForceDarkMode = 0,  // 0: follow system setting, 1: force dark off, 2: force dark on
        // ios
        bool enableWKWebView = true,
        int  wkContentMode = 0,  // 0: recommended, 1: mobile, 2: desktop
        bool wkAllowsLinkPreview = true,
        bool wkAllowsBackForwardNavigationGestures = true,
        // editor
        bool separated = false)
    {
        onJS = cb;
        onError = err;
        onHttpError = httpErr;
        onStarted = started;
        onLoaded = ld;
        onHooked = hooked;
        onCookies = cookies;

        webView = new AndroidJavaObject("net.gree.unitywebview.CWebViewPlugin");
        webView.Call("Init", name, transparent, zoom, androidForceDarkMode, ua, radius);

        using(AndroidJavaClass UnityClass = new AndroidJavaClass("com.unity3d.player.UnityPlayer"))
        {
            AndroidJavaObject View = UnityClass.GetStatic<AndroidJavaObject>("currentActivity").Get<AndroidJavaObject>("mUnityPlayer").Call<AndroidJavaObject>("getView");
            using(AndroidJavaObject Rct = new AndroidJavaObject("android.graphics.Rect"))
            {
                View.Call("getWindowVisibleDisplayFrame", Rct);
                mWindowVisibleDisplayFrameHeight = Rct.Call<int>("height");
            }
        }
    }

    protected virtual void OnDestroy()
    {
#if UNITY_WEBGL
#if !UNITY_EDITOR
        _gree_unity_webview_destroy(name);
#endif
#elif UNITY_WEBPLAYER
        Application.ExternalCall("unityWebView.destroy", name);
#elif UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN || UNITY_EDITOR_LINUX
        //TODO: UNSUPPORTED
#elif UNITY_EDITOR_OSX || UNITY_STANDALONE_OSX
        if (webView == IntPtr.Zero)
            return;
        _CWebViewPlugin_Destroy(webView);
        webView = IntPtr.Zero;
        Destroy(texture);
#elif UNITY_IPHONE
        if (webView == IntPtr.Zero)
            return;
        _CWebViewPlugin_Destroy(webView);
        webView = IntPtr.Zero;
#elif UNITY_ANDROID
        if (webView == null)
            return;
        webView.Call("Destroy");
        webView = null;
#endif
    }

    public void Pause()
    {
#if UNITY_WEBPLAYER || UNITY_WEBGL
        //TODO: UNSUPPORTED
#elif UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN || UNITY_EDITOR_LINUX
        //TODO: UNSUPPORTED
#elif UNITY_EDITOR_OSX || UNITY_STANDALONE_OSX
        //TODO: UNSUPPORTED
#elif UNITY_IPHONE
        //TODO: UNSUPPORTED
#elif UNITY_ANDROID
        if (webView == null)
            return;
        webView.Call("Pause");
#endif
    }

    public void Resume()
    {
#if UNITY_WEBPLAYER || UNITY_WEBGL
        //TODO: UNSUPPORTED
#elif UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN || UNITY_EDITOR_LINUX
        //TODO: UNSUPPORTED
#elif UNITY_EDITOR_OSX || UNITY_STANDALONE_OSX
        //TODO: UNSUPPORTED
#elif UNITY_IPHONE
        //TODO: UNSUPPORTED
#elif UNITY_ANDROID
        if (webView == null)
            return;
        webView.Call("Resume");
#endif
    }

    // Use this function instead of SetMargins to easily set up a centered window
    // NOTE: for historical reasons, `center` means the lower left corner and positive y values extend up.
    public void SetCenterPositionWithScale(Vector2 center, Vector2 scale)
    {
        float left = (Screen.width - scale.x) / 2.0f + center.x;
        float right = Screen.width - (left + scale.x);
        float bottom = (Screen.height - scale.y) / 2.0f + center.y;
        float top = Screen.height - (bottom + scale.y);
        SetMargins((int)left, (int)top, (int)right, (int)bottom);
    }

    public void SetMargins(int left, int top, int right, int bottom, bool relative = false)
    {
        if (webView == null)
            return;

        mMarginLeft = left;
        mMarginTop = top;
        mMarginRight = right;
        mMarginBottom = bottom;
        mMarginRelative = relative;

        float ml, mt, mr, mb;

        if (relative)
        {
            float w = (float)Screen.width;
            float h = (float)Screen.height;
            int iw = Screen.currentResolution.width;
            int ih = Screen.currentResolution.height;
            ml = left / w * iw;
            mt = top / h * ih;
            mr = right / w * iw;
            mb = AdjustBottomMargin((int)(bottom / h * ih));
        }
        else
        {
            ml = left;
            mt = top;
            mr = right;
            mb = AdjustBottomMargin(bottom);
        }

        bool r = relative;

        if (ml == mMarginLeftComputed
            && mt == mMarginTopComputed
            && mr == mMarginRightComputed
            && mb == mMarginBottomComputed
            && r == mMarginRelativeComputed)
        {
            return;
        }
        mMarginLeftComputed = ml;
        mMarginTopComputed = mt;
        mMarginRightComputed = mr;
        mMarginBottomComputed = mb;
        mMarginRelativeComputed = r;

        webView.Call("SetMargins", (int)ml, (int)mt, (int)mr, (int)mb);
    }

    public void SetVisibility(bool v)
    {
        if (webView == null)
            return;
        mVisibility = v;
        webView.Call("SetVisibility", v);
        visibility = v;
    }

    public bool GetVisibility()
    {
        return visibility;
    }

    public void SetScrollbarsVisibility(bool v)
    {
        if (webView == null)
            return;
        webView.Call("SetScrollbarsVisibility", v);
    }

    public void SetInteractionEnabled(bool enabled)
    {
        if (webView == null)
            return;
        webView.Call("SetInteractionEnabled", enabled);
    }

    public void SetAlertDialogEnabled(bool e)
    {
        if (webView == null)
            return;
        webView.Call("SetAlertDialogEnabled", e);
        alertDialogEnabled = e;
    }

    public bool GetAlertDialogEnabled()
    {
        return alertDialogEnabled;
    }

    public void SetScrollBounceEnabled(bool e)
    {
        scrollBounceEnabled = e;
    }

    public bool GetScrollBounceEnabled()
    {
        return scrollBounceEnabled;
    }

    public void SetCameraAccess(bool allowed)
    {
        if (webView == null)
            return;
        webView.Call("SetCameraAccess", allowed);
    }

    public void SetMicrophoneAccess(bool allowed)
    {
        if (webView == null)
            return;
        webView.Call("SetMicrophoneAccess", allowed);
    }

    public bool SetURLPattern(string allowPattern, string denyPattern, string hookPattern)
    {
        if (webView == null)
            return false;
        return webView.Call<bool>("SetURLPattern", allowPattern, denyPattern, hookPattern);
    }

    public void LoadURL(string url)
    {
        if (string.IsNullOrEmpty(url))
            return;
        if (webView == null)
            return;
        webView.Call("LoadURL", url);
    }

    public void LoadHTML(string html, string baseUrl)
    {
        if (string.IsNullOrEmpty(html))
            return;
        if (string.IsNullOrEmpty(baseUrl))
            baseUrl = "";
        if (webView == null)
            return;
        webView.Call("LoadHTML", html, baseUrl);
    }

    public void EvaluateJS(string js)
    {
        if (webView == null)
            return;
        webView.Call("EvaluateJS", js);
    }

    public int Progress()
    {
        if (webView == null)
            return 0;
        return webView.Get<int>("progress");
    }

    public bool CanGoBack()
    {
        if (webView == null)
            return false;
        return webView.Get<bool>("canGoBack");
    }

    public bool CanGoForward()
    {
        if (webView == null)
            return false;
        return webView.Get<bool>("canGoForward");
    }

    public void GoBack()
    {
        if (webView == null)
            return;
        webView.Call("GoBack");
    }

    public void GoForward()
    {
        if (webView == null)
            return;
        webView.Call("GoForward");
    }

    public void Reload()
    {
        if (webView == null)
            return;
        webView.Call("Reload");
    }

    public void CallOnError(string error)
    {
        if (onError != null)
        {
            onError(error);
        }
    }

    public void CallOnHttpError(string error)
    {
        if (onHttpError != null)
        {
            onHttpError(error);
        }
    }

    public void CallOnStarted(string url)
    {
        if (onStarted != null)
        {
            onStarted(url);
        }
    }

    public void CallOnLoaded(string url)
    {
        if (onLoaded != null)
        {
            onLoaded(url);
        }
    }

    public void CallFromJS(string message)
    {
        if (onJS != null)
        {
            onJS(message);
        }
    }

    public void CallOnHooked(string message)
    {
        if (onHooked != null)
        {
            onHooked(message);
        }
    }

    public void CallOnCookies(string cookies)
    {
        if (onCookies != null)
        {
            onCookies(cookies);
        }
    }

    public void AddCustomHeader(string headerKey, string headerValue)
    {
        if (webView == null)
            return;
        webView.Call("AddCustomHeader", headerKey, headerValue);
    }

    public string GetCustomHeaderValue(string headerKey)
    {
        if (webView == null)
            return null;
        return webView.Call<string>("GetCustomHeaderValue", headerKey);
    }

    public void RemoveCustomHeader(string headerKey)
    {
        if (webView == null)
            return;
        webView.Call("RemoveCustomHeader", headerKey);
    }

    public void ClearCustomHeader()
    {
        if (webView == null)
            return;
        webView.Call("ClearCustomHeader");
    }

    public void ClearCookies()
    {
        if (webView == null)
            return;
        webView.Call("ClearCookies");
    }


    public void SaveCookies()
    {
        if (webView == null)
            return;
        webView.Call("SaveCookies");
    }


    public void GetCookies(string url)
    {
        if (webView == null)
            return;
        webView.Call("GetCookies", url);
    }

    public void SetBasicAuthInfo(string userName, string password)
    {
        if (webView == null)
            return;
        webView.Call("SetBasicAuthInfo", userName, password);
    }

    public void ClearCache(bool includeDiskFiles)
    {
        if (webView == null)
            return;
        webView.Call("ClearCache", includeDiskFiles);

    }


    public void SetTextZoom(int textZoom)
    {
        if (webView == null)
            return;
        webView.Call("SetTextZoom", textZoom);
    }

    // Additional Code for SMART GIS
    private static ConcurrentDictionary<long, TaskCompletionSource<string>> CallbackTasks =
        new ConcurrentDictionary<long, TaskCompletionSource<string>>();

    private static long lastId = 0;

    private JavascriptResponseCallback callback;
    private class JavascriptResponseCallback : AndroidJavaProxy
    {
        private bool completed;
        private string result;
        public JavascriptResponseCallback() : base("net.gree.unitywebview.IResponseCallBack") { }

        void OnJavascriptResult(string result)
        {
            ResultData resultData = JsonUtility.FromJson<ResultData>(result);

            long callbackId = resultData.callbackId;
            string taskResult = resultData.result;

            if (CallbackTasks.TryGetValue(callbackId, out var tcs))
            {
                tcs.SetResult(taskResult);
                CallbackTasks.TryRemove(callbackId, out _);
            }
        }
    }

    private void Start()
    {
        callback = new JavascriptResponseCallback();
        webView = new AndroidJavaObject("net.gree.unitywebview.CWebViewPlugin");
    }

    public async Task<string> ExecuteJavaScript(string javascriptCode)
    {
        long callbackId = Interlocked.Increment(ref lastId);
        var taskCompletionSource = new TaskCompletionSource<string>();
        CallbackTasks.TryAdd(callbackId, taskCompletionSource);

        webView.Call("EvaluateJS", callbackId, callback);

        return await taskCompletionSource.Task;
    }


    public void OnJavascriptResult(string result)
    {
        ResultData resultData = JsonUtility.FromJson<ResultData>(result);

        long callbackId = resultData.callbackId;
        string taskResult = resultData.result;

        if(CallbackTasks.TryGetValue(callbackId, out var tcs))
        {
            tcs.SetResult(taskResult);
            CallbackTasks.TryRemove(callbackId, out _);
        }
    }


}
