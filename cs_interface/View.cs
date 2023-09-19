using System;
using UnityEngine;
using static UnityEngine.EventSystems.EventTrigger;

namespace SMARTGIS
{
	public class View
	{
		private int nativeId = 0;
		private BrowserManager browserManager;
		private double minZoomLevel = 0;
		private double maxZoomLevel = 28;
		private Projection.Kind projKind;

		public View(BrowserManager browserManager, double centerX, double centerY, int zoomLevel, Projection.Kind projection)
		{
			// mapview 는 추후 browserManager로 수정 
			this.browserManager = browserManager;
			this.projKind = projection;

		}

		public int GetNativeId()
		{
			return nativeId;
		}

		public int GetZoomLevel()
        {
            int zoomLevel = 0;

            browserManager.GetMonoBehaviour().StartCoroutine(browserManager.CoroutineCreateMap(centerX, centerY, zoomLevel, projection));

			return browserManager._javaRetViewGetZoomLevel;
		}
    }
}

