import './style.css';
import { Map, View } from 'ol';
import TileLayer from 'ol/layer/Tile';
import XYZ from 'ol/source/XYZ';
import VectorLayer from 'ol/layer/Vector';
import VectorSource from 'ol/source/Vector';
import Feature from 'ol/Feature';
import Point from 'ol/geom/Point';
import {Circle, Fill, Stroke, Style} from 'ol/style';


const map = new Map({
  target: 'map',
  layers: [
    new TileLayer({
      source: new XYZ({
        url: 'file:///data/data/com.DefaultCompany.android_build_test_webviewUI/files/map_data/{z}/{x}/{y}.png', // XYZ 타일의 경로를 지정
      }),
    }),
  ],
  view: new View({
    center: [0, 0],
    zoom: 2,
  }),
});

/*
function get_zoom_level(viewID) {
  console.log("get_zoom_level is called");
  return 10;
};

function set_zoom_level(viewID, zoomlv){
  console.log("set_zoom_level is called");
  console.log(zoomlv);
  const view = map.getView();
  view.setZoom(zoomlv);
};*/

map.on('click', function(evt) {
  var [x, y] = evt.coordinate;
  var str = `click:${x},${y}`;
  console.log(str);
  Unity.call(str);
})

window.init = function() {
  console.log("init is called");
  Unity.call("init:");
}

window.setZoomLevel = function(zoomlv) {
  console.log("setZoomLevel is called : " + zoomlv);
  const view = map.getView();
  view.setZoom(zoomlv);
  Unity.call("setZoomLevel:");
};

window.setCenterLonLat = function(coordx, coordy){
  console.log("setCenterLonLat is called : " + coordx + " " + coordy);
  const view = map.getView();
  view.setCenter([coordx, coordy]);
  Unity.call("setCenterLonLat:");
};

window.addSymbol = function(coordx, coordy, path, fid){
  console.log("addSymbol is called : " + coordx + " " + coordy);
  const view = map.getView();

  var feature = new Feature({
    geometry : new Point([coordx, coordy]),
    UID: fid
  });

  feature.setStyle(
    new Style({
      image: new Circle({
        radius: 7,
        fill: new Fill({color:'black'}),
        stroke: new Stroke({color: [0,0,0,0]})
      })
    })
  );

  var vectorLayer = new VectorLayer({
    source: new VectorSource({
      features: [feature]
    })
  });

  map.addLayer(vectorLayer);

  Unity.call("addSymbol:");
}

window.updateSymbol = function(coordx, coordy, fid){
  var layers = map.getLayers().getArray();
  var targetLayer = null;

  for(var i = 0; i < layers.length; i++){
    var layer = layers[i];
    if(layer instanceof VectorLayer){
      var features = layer.getSource().getFeatures();
      for(var j = 0; j < features.length; j++){
        var feature = features[j];
        if(feature.get('UID') === fid){
          feature.getGeometry().setCoordinates([coordx, coordy]);
        }
      }
    }
  }

  Unity.call("updateSymbol:");
}



window.removeSymbol = function(fid){
  var layers = map.getLayers().getArray();
  var targetLayer = null;

  for(var i = 0; i < layers.length; i++){
    var layer = layers[i];
    if(layer instanceof VectorLayer){
      var features = layer.getSource().getFeatures();
      for(var j = 0; j < features.length; j++){
        var feature = features[j];
        if(feature.get('UID') === fid){
          layer.getSource().removeFeature(feature);
        }
      }
    }
  }

  Unity.call("removeSymbol:");
}



/*
window.convertLonLatToPixel = function(coordx, coordy){
  console.log("convertLonLatToPixel is called : " + coordx + " " + coordy);
  Unity.call("convertLonLatToPixel:1,1");
}

window.convertPixelToLonLat = function(pixelx, pixely){
  console.log("convertPixelToLonLat is called : " + pixelx + " " + pixely);
  Unity.call("convertPixelToLonLat:0,0");
}
*/