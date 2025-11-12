//
// Approximate class using CGAL Triangulated Surface Mesh Approximation.
//
#include "lxsdk/lxresult.h"
#include "lxsdk/lxvmath.h"
#include <lxsdk/lx_mesh.hpp>
#include <lxsdk/lx_value.hpp>
#include <lxsdk/lx_layer.hpp>
#include <lxsdk/lx_action.hpp>
#include <lxsdk/lxu_math.hpp>
#include <lxsdk/lxu_matrix.hpp>
#include <lxsdk/lxu_select.hpp>

#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <cstdlib>  // rand(), RAND_MAX
#include <ctime>    // time()

//#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
//#include <CGAL/Surface_mesh.h>

#include <CGAL/Surface_mesh_approximation/approximate_triangle_mesh.h>
 
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>

#include "approximate.hpp"
#include "triangulate.hpp"

//
// Mesh Approximation class.
//

//typedef CGAL::Exact_predicates_inexact_constructions_kernel     Kernel;
//typedef Kernel::Point_3                                         Point_3;
//typedef CGAL::Surface_mesh<Point_3>                             Surface_mesh;
typedef boost::graph_traits<Surface_mesh>::face_descriptor      face_descriptor;
typedef Surface_mesh::Property_map<face_descriptor, std::size_t> Face_proxy_pmap;
typedef std::size_t                                             cluster_id_t;
typedef boost::property_map<Surface_mesh, boost::vertex_point_t>::type Vertex_point_map;
 
namespace VSA = CGAL::Surface_mesh_approximation;
namespace PMP = CGAL::Polygon_mesh_processing;

//
// Convert the internal CApproximate mesh representation to a CGAL Surface_mesh.
//
static void ConvertToCGALMesh(Surface_mesh& out_mesh, std::map<Surface_mesh::Edge_index, bool>& constrained_edges, CApproximate* context)
{
    CMesh& cmesh = context->m_cmesh;

    std::unordered_map<CVerxID, Surface_mesh::Vertex_index> vertex_map;

    for (auto& v : cmesh.m_vertices)
    {
        Point_3 p(v->pos[0], v->pos[1], v->pos[2]);
        Surface_mesh::Vertex_index vi = out_mesh.add_vertex(p);
        vertex_map[v] = vi;
    }

    for (auto& tri : cmesh.m_triangles)
    {
        Surface_mesh::Vertex_index v0 = vertex_map[tri->v0];
        Surface_mesh::Vertex_index v1 = vertex_map[tri->v1];
        Surface_mesh::Vertex_index v2 = vertex_map[tri->v2];
        out_mesh.add_face(v0, v1, v2);
    }

    CLxUser_Edge    uedge;
    uedge.fromMesh(context->m_mesh);

    CLxUser_Polygon upoly0, upoly1;
    upoly0.fromMesh(context->m_mesh);
    upoly1.fromMesh(context->m_mesh);

    printf("Total edges in CGAL mesh: %lu\n", static_cast<unsigned long>(out_mesh.number_of_edges()));
    for (auto e : out_mesh.edges())
    {
        // ここでは全てのエッジを非制約エッジとする例
        constrained_edges[e] = false;
        auto he = out_mesh.halfedge(e);
        auto i0 = out_mesh.source(he);
        auto i1 = out_mesh.target(he);
        auto v0 = cmesh.m_vertices[i0]->vrt;
        auto v1 = cmesh.m_vertices[i1]->vrt;
        uedge.SelectEndpoints(v0, v1);
        if (uedge.TestMarks(context->m_mark_lock) == LXe_TRUE)
        {
            constrained_edges[e] = true;
            continue;
        }
        if (context->m_preserveBoundary)
        {
            if (uedge.IsBorder() == LXe_TRUE)
            {
                constrained_edges[e] = true;
                continue;
            }
        }
        if (context->m_preserveMaterial)
        {
            unsigned int count;
            uedge.PolygonCount(&count);
            if (count != 2)
                continue;
            LXtPolygonID pol;
            uedge.PolygonByIndex(0, &pol);
            upoly0.Select(pol);
            uedge.PolygonByIndex(1, &pol);
            upoly1.Select(pol);
            CLxUser_StringTag tag0, tag1;
            tag0.set(upoly0);
            tag1.set(upoly1);
            const char *mat0 = tag0.Value(LXi_PTAG_MATR);
            const char *mat1 = tag1.Value(LXi_PTAG_MATR);
            if (std::string(mat0) != std::string(mat1))
            {
                constrained_edges[e] = true;
                continue;
            }
        }
    }
}

static void PrintCGALMesh(Surface_mesh& mesh)
{
    std::cout << "Vertices:" << std::endl;
    for (auto v : mesh.vertices())
    {
        Point_3 p = mesh.point(v);
        std::cout << "  Vertex " << v << ": (" << p.x() << ", " << p.y() << ", " << p.z() << ")" << std::endl;
    }

    std::cout << "Faces:" << std::endl;
    for (auto f : mesh.faces())
    {
        std::cout << "  Face " << f << ":";
        for (auto v : CGAL::vertices_around_face(mesh.halfedge(f), mesh))
        {
            std::cout << " " << v;
        }
        std::cout << std::endl;
    }
    for (auto e : mesh.edges())
    {
        auto he = mesh.halfedge(e);
        auto v0 = mesh.source(he);
        auto v1 = mesh.target(he);
        std::cout << "  Edge " << e << ": (" << v0 << " - " << v1 << ")" << std::endl;
    }
}

static void ConvertFromCGALMesh(Surface_mesh& surface_mesh, CLxUser_Mesh& mesh)
{
    CLxUser_Polygon     poly;
    CLxUser_Point       vert;

    std::vector<LXtPointID> points;
    poly.fromMesh(mesh);
    vert.fromMesh(mesh);
    for (auto v : surface_mesh.vertices())
    {
        Point_3 p = surface_mesh.point(v);
        LXtPointID new_vrt;
        LXtVector pos;
        LXx_VSET3(pos, p.x(), p.y(), p.z());
        vert.New(pos, &new_vrt);
        points.push_back(new_vrt); 
    }

    std::vector<LXtPointID> tri;
    for (auto f : surface_mesh.faces())
    {
        tri.clear();
        for (auto v : CGAL::vertices_around_face(surface_mesh.halfedge(f), surface_mesh))
        {
            tri.push_back(points[v]);
        }
        LXtPolygonID new_pol;
        poly.New(LXiPTYP_FACE, tri.data(), tri.size(), 0, &new_pol);
    }
}

// Build internal mesh representation
//
LxResult CApproximate::BuildMesh(CLxUser_Mesh& base_mesh)
{
    m_mesh.set(base_mesh);
    m_poly.fromMesh(m_mesh);
    m_vert.fromMesh(m_mesh);
    m_vmap.fromMesh(m_mesh);
    return m_cmesh.BuildMesh(base_mesh);
}


//
// Variational Shape Approximation
//
LxResult CApproximate::ApproximateMesh(CLxUser_Mesh& base_mesh)
{
    BuildMesh(base_mesh);

    Surface_mesh surface_mesh;
    std::map<Surface_mesh::Edge_index, bool> constrained_edges;
    ConvertToCGALMesh(surface_mesh, constrained_edges, this);
    //PrintCGALMesh(surface_mesh);

    auto fpxmap = surface_mesh.add_property_map<face_descriptor, cluster_id_t>("f:proxy_id", 0).first;

    // The output will be an indexed triangle mesh
    std::vector<Point_3> anchors;
    std::vector<std::array<std::size_t, 3> > triangles;

    // free function interface with named parameters
    bool is_manifold = VSA::approximate_triangle_mesh(surface_mesh,
                CGAL::parameters::verbose_level(VSA::MAIN_STEPS).
                                max_number_of_proxies(m_proxies).
                                number_of_iterations(m_iteration). // number of relaxation iterations after 
                                anchors(std::back_inserter(anchors)). // anchor points
                                triangles(std::back_inserter(triangles)).
                                face_proxy_map(fpxmap)); // indexed triangles

    std::cout << "#is_manifold: " << is_manifold << std::endl;
    std::cout << "#anchor points: " << anchors.size() << std::endl;
    std::cout << "#triangles: " << triangles.size() << std::endl;

    //Surface_mesh output;
    m_output.clear();
    m_proxy_source.clear();
 
    if (is_manifold) {
        //std::cout << "oriented, 2-manifold output." << std::endl;

        // convert from soup to surface mesh
        PMP::orient_polygon_soup(anchors, triangles);
        PMP::polygon_soup_to_polygon_mesh(anchors, triangles, m_output);
        if (CGAL::is_closed(m_output) && (!PMP::is_outward_oriented(m_output)))
            PMP::reverse_face_orientations(m_output);
        //PrintCGALMesh (m_output);

        unsigned proxy_count = 0;
        for (auto f : surface_mesh.faces())
        {
            //std::cout << "face : " << f << " proxy : " << fpxmap[f] << std::endl;
            m_cmesh.m_triangles[f]->proxy = fpxmap[f];
            if (m_cmesh.m_triangles[f]->proxy > proxy_count)
                proxy_count = m_cmesh.m_triangles[f]->proxy;
        }
        proxy_count ++;
        m_proxy_source.resize(proxy_count);
        for (auto tri : m_cmesh.m_triangles)
        {
            m_proxy_source[tri->proxy] = tri;
            //std::cout << "tri : " << tri->index << " proxy : " << tri->proxy << std::endl;
        }
        std::cout << "proxy_count : " << proxy_count << std::endl;
    }

    return LXe_OK;
}

//
// Write the approximation output to the given mesh.
//
LxResult CApproximate::WriteApproximation(CLxUser_Mesh& edit_mesh)
{
    ConvertFromCGALMesh(m_output, edit_mesh);
    return LXe_OK;
}

//
// Write the segmentation result to the given mesh as polygon tags or edge selection set.
//
LxResult CApproximate::WriteSegmentations(CLxUser_Mesh& edit_mesh)
{
    CLxUser_MeshService mesh_svc;

    m_mesh.set(edit_mesh);

    if (m_segment == CApproximate::EDGE_SSET)
    {
        m_edge.fromMesh(m_mesh);
        m_vmap.fromMesh(m_mesh);

        LXtMeshMapID    vmap_id;
        if (LXx_FAIL(m_vmap.SelectByName(LXi_VMAP_EPCK, m_sset.c_str())))
            m_vmap.New(LXi_VMAP_EPCK, m_sset.c_str(), &vmap_id);
        else
            vmap_id = m_vmap.ID();

        for (auto& edge : m_cmesh.m_edges)
        {
            if (edge->tris.size() == 2)
            {
                if (edge->tris[0]->proxy != edge->tris[1]->proxy)
                {
                    float value = 1.0;
                    m_edge.SelectEndpoints(edge->v0->vrt, edge->v1->vrt);
                    m_edge.SetMapValue(vmap_id, &value);
                }
            }
        }
    }
    else
    {
        m_poly.fromMesh(m_mesh);
        m_vert.fromMesh(m_mesh);
        m_vmap.fromMesh(m_mesh);
    
        CLxUser_StringTag polyTag;
        LXtID4 type;
        if (m_segment == CApproximate::POLY_MATR)
            type = LXi_PTAG_MATR;
        else
            type = LXi_PTAG_PART;

        LXtMeshMapID    vmap_id = nullptr;
        std::vector<std::vector<float>> proxy_color;
        if (m_set_color)
        {
            if (LXx_FAIL(m_vmap.SelectByName(LXi_VMAP_RGB, "Segment")))
                m_vmap.New(LXi_VMAP_RGB, "Segment", &vmap_id);
            else
                vmap_id = m_vmap.ID();
            std::srand(static_cast<unsigned int>(std::time(nullptr)));
            auto proxy_count = m_proxy_source.size();
            proxy_color.resize(proxy_count);
            for (auto i = 0u; i < proxy_count; i++)
            {
                proxy_color[i].resize(3);
                for (auto j = 0u; j < 3; j++)
                {
                    proxy_color[i][j] = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
                }
            }
        }
        for (auto& face : m_cmesh.m_faces)
        {
            CTriangleID tri = face.second.tris[0];
            m_poly.Select(face.first);
            std::string tag = std::to_string(tri->proxy);
            polyTag.set(m_poly);
            polyTag.Set(type, tag.c_str());
            if (m_set_color)
            {
                unsigned count;
                float color[3];
                for (auto i = 0u; i < 3; ++i)
                    color[i] = proxy_color[tri->proxy][i];
                m_poly.VertexCount(&count);
                for (auto i = 0u; i < count; i++)
                {
                    LXtPointID pntID;
                    m_poly.VertexByIndex(i, &pntID);
                    m_poly.SetMapValue(pntID, vmap_id, color);
                }
            }
        }
    }
    return LXe_OK;
}