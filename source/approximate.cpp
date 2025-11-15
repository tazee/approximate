//
// Approximate class using CGAL Triangulated Surface Mesh Approximation.
//

#include <CGAL/Surface_mesh_approximation/approximate_triangle_mesh.h>
 
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>

#include "approximate.hpp"
#include "triangulate.hpp"

//
// Mesh Approximation class.
//
typedef boost::graph_traits<Surface_mesh>::face_descriptor      face_descriptor;
typedef Surface_mesh::Property_map<face_descriptor, std::size_t> Face_proxy_pmap;
typedef std::size_t                                             cluster_id_t;
typedef boost::property_map<Surface_mesh, boost::vertex_point_t>::type Vertex_point_map;
 
namespace VSA = CGAL::Surface_mesh_approximation;
namespace PMP = CGAL::Polygon_mesh_processing;

static std::vector<std::array<float,3>> g_proxy_color;

static void UpdateProxyColors(size_t proxy_count)
{
    static std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    if (g_proxy_color.size() < proxy_count)
    {
        g_proxy_color.reserve(proxy_count);
        while (g_proxy_color.size() < proxy_count) {
            std::array<float,3> color = { dist(rng), dist(rng), dist(rng) };
            g_proxy_color.push_back(color);
        }
    }
}

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
        auto f = out_mesh.add_face(v0, v1, v2);
        if (f == Surface_mesh::null_face()) {
            std::cerr << "warning: add_face failed for vertices\n";
            continue;
        }
    }

    CLxUser_Edge    uedge;
    uedge.fromMesh(context->m_mesh);

    CLxUser_Polygon upoly0, upoly1;
    upoly0.fromMesh(context->m_mesh);
    upoly1.fromMesh(context->m_mesh);
}

static void ConvertToCGALMesh(Surface_mesh& out_mesh, CPartID part)
{
    std::unordered_map<CVerxID, Surface_mesh::Vertex_index> vertex_map;
    vertex_map.reserve(part->vrts.size());

    for (auto& v : part->vrts)
    {
        Point_3 p(v->pos[0], v->pos[1], v->pos[2]);
        Surface_mesh::Vertex_index vi = out_mesh.add_vertex(p);
        vertex_map[v] = vi;
    }

    for (auto& tri : part->tris)
    {
        auto it0 = vertex_map.find(tri->v0);
        auto it1 = vertex_map.find(tri->v1);
        auto it2 = vertex_map.find(tri->v2);
        if (it0 == vertex_map.end() || it1 == vertex_map.end() || it2 == vertex_map.end()) {
            std::cerr << "warning: triangle references missing vertex\n";
            continue;
        }
        auto f = out_mesh.add_face(it0->second, it1->second, it2->second);
        if (f == Surface_mesh::null_face()) {
            std::cerr << "warning: add_face failed\n";
            continue;
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

    poly.fromMesh(mesh);
    vert.fromMesh(mesh);

    std::unordered_map<Surface_mesh::Vertex_index, LXtPointID> v_to_point;
    v_to_point.reserve(surface_mesh.number_of_vertices());

    for (auto v : surface_mesh.vertices())
    {
        Point_3 p = surface_mesh.point(v);
        LXtPointID new_vrt;
        LXtVector pos;
        LXx_VSET3(pos, p.x(), p.y(), p.z());
        vert.New(pos, &new_vrt);
        v_to_point[v] = new_vrt;
    }

    for (auto f : surface_mesh.faces())
    {
        std::vector<LXtPointID> tri;
        for (auto v : CGAL::vertices_around_face(surface_mesh.halfedge(f), surface_mesh))
        {
            tri.push_back(v_to_point[v]);
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
LxResult CApproximate::ApproximatePart(CLxUser_Mesh& base_mesh, CPartID part)
{
    std::cout << "*** part: " << part->index << " tris : " << part->tris.size() << " vrts : " << part->vrts.size() << std::endl;

    Surface_mesh surface_mesh;
    ConvertToCGALMesh(surface_mesh, part);
    //PrintCGALMesh(surface_mesh);

    auto fpxmap = surface_mesh.add_property_map<face_descriptor, cluster_id_t>("f:proxy_id", 0).first;

    // The output will be an indexed triangle mesh
    std::vector<Point_3> anchors;
    std::vector<std::array<std::size_t, 3> > triangles;

    std::cout << "is_valid: " << CGAL::is_valid(surface_mesh) << std::endl;
    PMP::orient_to_bound_a_volume(surface_mesh);

    // free function interface with named parameters
    if (m_mode == CApproximate::APPROXIMATION)
    {
        bool is_manifold = VSA::approximate_triangle_mesh(surface_mesh,
                    CGAL::parameters::verbose_level(VSA::MAIN_STEPS).
                                    max_number_of_proxies(m_proxies).
                                    number_of_iterations(m_iteration). // number of relaxation iterations after 
                                    anchors(std::back_inserter(anchors)). // anchor points
                                    triangles(std::back_inserter(triangles))); // indexed triangles

        std::cout << "#is_manifold: " << is_manifold << std::endl;
        std::cout << "#anchor points: " << anchors.size() << std::endl;
        std::cout << "#triangles: " << triangles.size() << std::endl;

        // convert from soup to surface mesh
        Surface_mesh output;
        PMP::orient_polygon_soup(anchors, triangles);
        PMP::polygon_soup_to_polygon_mesh(anchors, triangles, output);
        if (CGAL::is_closed(output) && (!PMP::is_outward_oriented(output)))
            PMP::reverse_face_orientations(output);
        //PrintCGALMesh (output);
        m_outputs.push_back(output);
    }
    else if (m_mode == CApproximate::SEGMENTATION)
    {
        bool is_manifold = VSA::approximate_triangle_mesh(surface_mesh,
                    CGAL::parameters::verbose_level(VSA::MAIN_STEPS).
                                    max_number_of_proxies(m_proxies).
                                    number_of_iterations(m_iteration). // number of relaxation iterations after 
                                    face_proxy_map(fpxmap)); // indexed triangles

        std::cout << "#is_manifold: " << is_manifold << std::endl;

        unsigned proxy_count = 0;
        for (auto f : surface_mesh.faces())
        {
            //std::cout << "face : " << f << " proxy : " << fpxmap[f] << std::endl;
            part->tris[f]->proxy = fpxmap[f];
            if (part->tris[f]->proxy > proxy_count)
                proxy_count = part->tris[f]->proxy;
        }
        std::vector<CTriangleID> proxy_source;
        proxy_count ++;
        proxy_source.resize(proxy_count);
        for (auto tri : part->tris)
        {
            proxy_source[tri->proxy] = tri;
            //std::cout << "tri : " << tri->index << " proxy : " << tri->proxy << std::endl;
        }
        m_proxy_sources.push_back(proxy_source);
        std::cout << "proxy_count : " << proxy_count << std::endl;
    }
    else
    {
        return LXe_FAILED;
    }

    return LXe_OK;
}

//
// Variational Shape Approximation
//
LxResult CApproximate::ApproximateMesh(CLxUser_Mesh& base_mesh)
{
    BuildMesh(base_mesh);

    m_proxy_sources.clear();
    m_outputs.clear();

    for (auto part : m_cmesh.m_parts)
    {
        auto result = ApproximatePart(base_mesh, part);
        if (result != LXe_OK)
            continue;
    }

    return LXe_OK;
}

//
// Write the approximation output to the given mesh.
//
LxResult CApproximate::WriteApproximation(CLxUser_Mesh& edit_mesh)
{
    for (auto& output : m_outputs)
    {
        ConvertFromCGALMesh(output, edit_mesh);
    }
    return LXe_OK;
}

//
// Write the segmentation result to the given mesh as polygon tags or edge selection set.
//
LxResult CApproximate::WriteSegmentations(CLxUser_Mesh& edit_mesh)
{
    if (m_proxy_sources.size() == 0)
        return LXe_FAILED;

    CLxUser_MeshService mesh_svc;

    m_mesh.set(edit_mesh);

    if (m_segment == Segmentation::EDGE_SSET)
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
        if (m_segment == Segmentation::POLY_MATR)
            type = LXi_PTAG_MATR;
        else
            type = LXi_PTAG_PART;

        for (auto& face : m_cmesh.m_faces)
        {
            CTriangleID tri = face.second.tris[0];
            m_poly.Select(face.first);
            std::string tag = std::to_string(tri->proxy);
            if (m_proxy_sources.size() > 1)
                tag = std::to_string(face.second.part) + "-" + tag;
            polyTag.set(m_poly);
            polyTag.Set(type, tag.c_str());
        }
    }
    if (m_set_color)
    {
        m_poly.fromMesh(m_mesh);
        m_vmap.fromMesh(m_mesh);

        LXtMeshMapID    vmap_id = nullptr;
        if (LXx_FAIL(m_vmap.SelectByName(LXi_VMAP_RGB, "Segment")))
            m_vmap.New(LXi_VMAP_RGB, "Segment", &vmap_id);
        else
            vmap_id = m_vmap.ID();
        auto proxy_count = m_proxy_sources[0].size();
        for (auto i = 1u; i < m_proxy_sources.size(); i++)
        {
            if (m_proxy_sources[i].size() > proxy_count)
                proxy_count = m_proxy_sources[i].size();
        }
        UpdateProxyColors(proxy_count);

        for (auto& face : m_cmesh.m_faces)
        {
            CTriangleID tri = face.second.tris[0];
            m_poly.Select(face.first);
            unsigned count;
            float color[3];
            for (auto i = 0u; i < 3; ++i)
                color[i] = g_proxy_color[tri->proxy][i];
            m_poly.VertexCount(&count);
            for (auto i = 0u; i < count; i++)
            {
                LXtPointID pntID;
                m_poly.VertexByIndex(i, &pntID);
                m_poly.SetMapValue(pntID, vmap_id, color);
            }
        }
    }
    return LXe_OK;
}