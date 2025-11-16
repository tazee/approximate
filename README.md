# Mesh Approximation tools for Modo plug-in
This is a Modo Plug-in kit to approximate surface polygons by [CGAL](https://cgal.org) Triangulated Surface Mesh Approximation.<br><br>
Triangulated Surface Mesh Approximation package implements the Variational Shape Approximation method to approximate an input surface triangle mesh by a simpler surface triangle mesh. The algorithm proceeds by iterative clustering of triangles, the clustering process being seeded randomly, incrementally or hierarchically.

This kit contains a direct modeling tool and a procedural mesh operator for Modo macOS and Windows. These tools support two operation modes. **Approximation** mode outputs an approximated surface trinagle mesh constructed by generating a surface triangle mesh which approximates the clusters. **Segmentation** mode outputs segments of input source polygons into polygon tags, edge selection set and color vertex map. These segmentation data is useful for UV unwrap workflow. The boundaries of an edge selection set or polygon tag can be used as seams for UV unwrapping.


<div align="left">
<img src="images/bimba.png" style='max-height: 480px; object-fit: contain'/>
</div>

<div align="left">
<img src="images/fandisk.png" style='max-height: 480px; object-fit: contain'/>
</div>
Left: source mesh, Middle: color-coded segments, Right: approximated mesh.

<div align="left">
<img src="images/unwrap.png" style='max-height: 505px; object-fit: contain'/>
</div>
Unwrap segments by UV unwrap tool using Part tag boundaries from Segmentation mode.

## Installing
- Download lpk from releases. Drag and drop into your Modo viewport. If you're upgrading, delete previous version.

## How to use Approximation tool
- The tool version of Approximation can be launched from **Approximate** button on **Polygon** tab of **Model** ToolBar on left.
- The procedural mesh operator version is available on Mesh Operator viewport. That is categorized in Polygon tab.
<div align="left">
<img src="images/UI.png" style='max-height: 620px; object-fit: contain'/>
</div>

## Basic Usage with the direct tool
- Activate **Approximate** button on **Polygon** tab of **Model** Modo toolbar.
- Hauling 3D view by LMB to increase **Max of Proxies**.
- Try other options on the tool properties panel.

## Tool Properties<br>
### Approximation mode<br>
**Approximation** mode constructs triangles by Variational Shape Approximation algorithm. This outputs the triangles into new mesh when **New Mesh** option is enabled, otherwise it updates the source mesh. The constructed triangles don't store mesh properties from the source mesh. So, the output triangles do not contain polygon tags, vertex maps and other properties.<br>

### Segmentation mode<br>
**Segmentation** mode mode outputs the clusters into which the original polygons have been divided by calculation to the polygon tag or vertex map. When **Segmentaion** is **Material Tag** or **Part Tag**, the cluster identifer is set as polygon tag. The cluster identifier is set as a string with a sequential number starting from 0. If the original mesh consists of multiple parts, it will be a combination of the part number and the sequential number of the cluster, such as "0-0". **Set Color** outputs the clusters as random colors in RGB color vertex map<br>

### Max of Proxies<br>
**Max of Proxies** is the maximum number of proxies needed to approximate the geometry and determines when to stop adding proxies. This does not specify the total number of triangles to be output.<br>

### Iteration<br>
**Iteration** is the number of clustering interation to minimize the clustering error. 

## Dependencies

- LXSDK  
This kit requires Modo SDK (Modo 16.1v8 or later). Download and build LXSDK and set you LXSDK path to LXSDK_PATH in CMakeLists.txt in triagulate.
- CGAL library 6.0.1 (https://github.com/cgal/cgal)  
This also requires CGAL library. Download and CGAL from below and set the include and library path to CMakeLists.txt in triagulate.
- Boost 1.87.0 (https://www.boost.org/)
- GMP 6.3.0 (https://gmplib.org/)
- MPFR 4.2.1 (https://https://www.mpfr.org/)

If they are not set, the project files are downloaded automatically via FetchContent of cmake

## License

```
This software is based part on CGAL (The Computational Geometry Algorithms Library):
Licensed under the GPL-3.0 license.
https://cgal.org
```
