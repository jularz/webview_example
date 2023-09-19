using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Networking;
using static UnityEngine.EventSystems.EventTrigger;

namespace SMARTGIS
{
    public class MapView
    {
        BrowserManager browserManager;
        private View view;
        
        // Initialize
        public MapView(MonoBehaviour monoBehaviour)
        {
            browserManager = new BrowserManager(monoBehaviour);
            
        }

        ~MapView()
        {

        }

        public void Init(int left, int top, int right, int bottom)
        {
            // 1. create Browser
            browserManager.InitBrowser(left, top, right, bottom);

            // 2. create map object @ openlayers
            browserManager.GetMonoBehaviour().StartCoroutine(browserManager.CoroutineCreateMap());
        }

        public View CreateView(double centerX, double centerY, int zoomLevel, Projection.Kind projection)
        {
            browserManager.GetMonoBehaviour().StartCoroutine(browserManager.CoroutineCreateView(centerX, centerY, zoomLevel, projection));
            this.view = new View(browserManager, centerX, centerY, zoomLevel, projection);

            return this.view;
        }
        public Layer CreateLayer()
        {
            monoBehaviour.StartCoroutine(CoroutineCreateLayer(centerX, centerY, zoomLevel, projection));
            Layer layer = new Layer();

            return layer;
        }

        public View GetView()
        {
            return this.view;
        }

        public ReturnCode SetView(View view)
        {
            this.view = view;
            monoBehaviour.StartCoroutine(CoroutineSetView(view.GetNativeId()));

            return ReturnCode.OK;
        }

        public ReturnCode SetLayerGroup(LayerGroup layerGroup)
        {
            this.layerGroup = layerGroup;
            monoBehaviour.StartCoroutine(CoroutineSetLayerGroup(layerGroup.GetNativeId()));

            return ReturnCode.OK;
        }


        /*test code 
        int ret_value = 0;

        // Start is called before the first frame update
        public int return_value(MonoBehaviour mono)
        {
            int result;

            mono.StartCoroutine(WaitForSecondsAndReturn());

            result = ret_value;

            return result;
        }

        private IEnumerator WaitForSecondsAndReturn()
        {
            ret_value = 10;
            yield return new WaitForSeconds(3f);
        }
        */


    }

}