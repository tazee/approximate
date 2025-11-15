//
// Approximate class using CGAL Triangulated Surface Mesh Approximation.
//
#pragma once

#include <lxsdk/lx_log.hpp>
#include <lxsdk/lx_mesh.hpp>
#include <lxsdk/lx_value.hpp>
#include <lxsdk/lxu_math.hpp>
#include <lxsdk/lxvmath.h>
#include <lxsdk/lxu_matrix.hpp>
#include <lxsdk/lxu_quaternion.hpp>
#include <lxsdk/lxresult.h>
#include <lxsdk/lx_layer.hpp>
#include <lxsdk/lx_action.hpp>
#include <lxsdk/lxu_select.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/segment.hpp>

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>

typedef CGAL::Exact_predicates_inexact_constructions_kernel     Kernel;
typedef Kernel::Point_3                                         Point_3;
typedef CGAL::Surface_mesh<Point_3>                             Surface_mesh;

#include <vector>
#include <unordered_set>
#include <random>
#include <iostream>

#include "util.hpp"
#include "cmesh.hpp"

struct CApproximate
{
    enum Mode : int
    {
        APPROXIMATION = 0,
        SEGMENTATION,
    };
    enum Segmentation : int
    {
        POLY_MATR = 0,
        POLY_PART,
        EDGE_SSET,
    };

    // source mesh context
    CMesh m_cmesh;

    // CGAL surface mesh
    std::vector<Surface_mesh> m_outputs;

    std::vector<std::vector<CTriangleID>> m_proxy_sources;

    unsigned m_part;    // current connecting part

    CLxUser_Mesh        m_mesh;
    CLxUser_Edge        m_edge;
    CLxUser_Polygon     m_poly;
    CLxUser_Point       m_vert;
    CLxUser_MeshMap     m_vmap;
    CLxUser_PolygonEdit m_poledit;
    CLxUser_LogService  s_log;
    CLxUser_MeshService s_mesh;

    LXtMarkMode m_pick;
    LXtMarkMode m_mark_done;
    LXtMarkMode m_mark_seam;
    LXtMarkMode m_mark_hide;
    LXtMarkMode m_mark_lock;

    int    m_mode;      // Approximation mode
    int    m_proxies;
    int    m_iteration;
    int    m_preserveBoundary;
    int    m_preserveMaterial;

    int    m_segment;
    int    m_new_mesh;
    int    m_set_color;

    std::string m_sset;

    CApproximate()
    {
        m_mode  = Mode::SEGMENTATION;
        m_proxies = 20;
        m_iteration = 20;
        m_segment = Segmentation::POLY_MATR;
        m_new_mesh = 1;
        m_set_color = 1;
        m_sset = "Segment";

        CLxUser_MeshService mesh_svc;
        m_pick      = mesh_svc.SetMode(LXsMARK_SELECT);
        m_mark_done = mesh_svc.SetMode(LXsMARK_USER_0);
        m_mark_seam = mesh_svc.SetMode(LXsMARK_USER_1);
        m_mark_hide = mesh_svc.SetMode(LXsMARK_HIDE);
        m_mark_lock = mesh_svc.SetMode(LXsMARK_LOCK);
    }

    //
    // Variational Shape Approximation
    //
    LxResult BuildMesh (CLxUser_Mesh& base_mesh);
    LxResult ApproximateMesh (CLxUser_Mesh& base_mesh);
    LxResult ApproximatePart (CLxUser_Mesh& base_mesh, CPartID part);
    LxResult WriteSegmentations (CLxUser_Mesh& edit_mesh);
    LxResult WriteApproximation (CLxUser_Mesh& edit_mesh);
};