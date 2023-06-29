import './style.css';
import { Map, View } from 'ol';
import TileLayer from 'ol/layer/Tile';
import XYZ from 'ol/source/XYZ';


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

function get_zoom_level(viewID) {
  return 10;
};

function set_zoom_level(viewID, zoomlv){
  const view = map.getView();
  view.setZoom(zoomlv);
};