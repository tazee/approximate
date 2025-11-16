//
// Mesh Context from Modo.
// This class contains triangulated polygons and the veritices converted from Modo mesh.
//
#pragma once

#include <lxsdk/lx_log.hpp>
#include <lxsdk/lx_mesh.hpp>
#include <lxsdk/lx_value.hpp>
#include <lxsdk/lxu_math.hpp>
#include <lxsdk/lxvmath.h>
#include <lxsdk/lxu_matrix.hpp>
#include <lxsdk/lxu_quaternion.hpp>

#include <vector>
#include <unordered_set>

#include "util.hpp"
#include "triangulate.hpp"

struct CVerx;
struct CEdge;
struct CTriangle;
struct CFace;
struct CPart;

typedef std::shared_ptr<CVerx>     CVerxID;
typedef std::shared_ptr<CEdge>     CEdgeID;
typedef std::shared_ptr<CTriangle> CTriangleID;
typedef std::shared_ptr<CPart>     CPartID;

struct CVerx
{
    CTriangleID                 tri;
    LXtPointID                  vrt;
    unsigned                    vrt_index;
    unsigned                    index;
    unsigned                    part;
    LXtVector                   pos;        // vertex position
    LXtVector                   new_pos;       // new vertex position
    void*                       userData;   // any working data
    LXtMarkMode                 marks;      // marks for working
    std::vector<CEdgeID>        edge;       // connecting edges
    std::vector<CTriangleID>    tris;       // connecting triangles
    bool                        collapsed;  // vertex collapsed flag 
};

struct CEdge
{
    CVerxID                     v0, v1;  // vertex 1,2
    std::vector<CTriangleID>    tris;       // connecting triangles
    bool                        collapsed;  // edge collapsed flag
};

struct CTriangle
{
    LXtPolygonID                pol;
    int                         pol_index;
    unsigned                    index;
    unsigned                    part;
    unsigned                    proxy;
    CVerxID                     v0, v1, v2;
    CEdgeID                     edge;      // an edge
    bool                        deleted;   // triangle deleted flag
    bool                        updated;   // triangle updated flag
};

struct CFace
{
    unsigned                    part;  // part index
    std::vector<CTriangleID>    tris = {};  // triangles of the face
};

struct CPart
{
    unsigned                    index;
    bool                        no_source = false;
    std::vector<CTriangleID>    tris = {};  // triangles of the part
    std::vector<CVerxID>        vrts = {};  // vertices of the triangles
};

struct CMesh
{
    CMesh()
    {
        CLxUser_MeshService mesh_svc;
        m_pick      = mesh_svc.SetMode(LXsMARK_SELECT);
        m_mark_done = mesh_svc.SetMode(LXsMARK_USER_0);
        m_mark_seam = mesh_svc.SetMode(LXsMARK_USER_1);
        m_mark_hide = mesh_svc.SetMode(LXsMARK_HIDE);
        m_mark_lock = mesh_svc.SetMode(LXsMARK_LOCK);
    }

    LxResult AddPolygon(LXtPolygonID pol)
    {
        CFace face;
        m_faces[pol] = face;
        return LXe_OK;
    }

    LxResult AddTriangle(LXtPolygonID pol, LXtPointID v0, LXtPointID v1, LXtPointID v2)
    {
        m_triangles.push_back(std::make_shared<CTriangle>());
        CTriangleID tri = m_triangles.back();
        tri->index = static_cast<int>(m_triangles.size()-1);

        CVerxID dv[3];
        dv[0] = AddVertex(v0, pol, tri);
        dv[1] = AddVertex(v1, pol, tri);
        dv[2] = AddVertex(v2, pol, tri);

        CLxUser_Edge edge;
        edge.fromMesh(m_mesh);

        if (pol)
        {
            CFace& face = m_faces[pol];
            face.tris.push_back(tri);
        }

        tri->v0   = dv[0];
        tri->v1   = dv[1];
        tri->v2   = dv[2];
        tri->pol  = pol;
        tri->updated = false;
        tri->deleted = false;
        tri->proxy = 0;

        AddEdge(dv[0], dv[1], tri);
        AddEdge(dv[1], dv[2], tri);
        AddEdge(dv[2], dv[0], tri);
        return LXe_OK;
    }

    LxResult AddEdge(CVerxID v0, CVerxID v1, CTriangleID tri)
    {
        // Check if the edge already exists
        for (auto& edge : v0->edge)
        {
            if ((edge->v0 == v0 && edge->v1 == v1) || (edge->v0 == v1 && edge->v1 == v0))
            {
                edge->tris.push_back(tri);
                return LXe_OK;
            }
        }

        // Create a new edge
        m_edges.push_back(std::make_shared<CEdge>());
        CEdgeID edge = m_edges.back();
        edge->v0 = v0;
        edge->v1 = v1;
        edge->collapsed = false;
        edge->tris.push_back(tri);

        v0->edge.push_back(edge);
        v1->edge.push_back(edge);
        return LXe_OK;
    }

    LXtPolygonID TracePolygon(LXtPointID vrt, LXtPolygonID pol, int shift)
    {
        CLxUser_Edge edge;
        edge.fromMesh(m_mesh);

        CLxUser_Polygon poly;
        poly.fromMesh(m_mesh);

        LXtPolygonID pol1;
        LXtPointID   vrt1;
        unsigned int npol, nvert;

        m_poly.Select(pol);
        m_poly.VertexCount(&nvert);

        for (auto i = 0u; i < nvert; i++)
        {
            m_poly.VertexByIndex(i, &vrt1);
            if (vrt1 == vrt)
            {
                m_poly.VertexByIndex((i + shift + nvert) % nvert, &vrt1);
                edge.SelectEndpoints(vrt, vrt1);
                edge.PolygonCount(&npol);
                for (auto j = 0u; j < npol; j++)
                {
                    edge.PolygonByIndex(j, &pol1);
                    poly.Select(pol1);
                    if (poly.TestMarks(m_mark_hide) == LXe_TRUE)
                        continue;
                    if (poly.TestMarks(m_mark_lock) == LXe_TRUE)
                        continue;
                    if (pol1 != pol)
                    {
                        return pol1;
                    }
                }
                return nullptr;
            }
        }
        return nullptr;
    }

    // Find and get the CTriangle with the given vrt and pol.
    CTriangleID FetchTriangle(LXtPointID vrt, LXtPolygonID pol)
    {
        LXtPolygonID pol0 = pol;
        unsigned int npol;

        m_vert.Select(vrt);
        m_vert.PolygonCount(&npol);

        if (m_faces.find(pol) != m_faces.end())
        {
            CFace& face = m_faces[pol];
            for (auto& tri : face.tris)
            {
                if (tri->v0->vrt == vrt || tri->v1->vrt == vrt || tri->v2->vrt == vrt)
                    return tri;
            }
        }
        npol--;

        pol = pol0;
        while (npol > 0)
        {
            pol = TracePolygon(vrt, pol, +1);
            if (!pol)
                break;
            if (pol == pol0)
                break;
            if (m_faces.find(pol) != m_faces.end())
            {
                CFace& face = m_faces[pol];
                for (auto& tri : face.tris)
                {
                    if (tri->v0->vrt == vrt || tri->v1->vrt == vrt || tri->v2->vrt == vrt)
                        return tri;
                }
            }
            npol--;
        }

        pol = pol0;
        while (npol > 0)
        {
            pol = TracePolygon(vrt, pol, -1);
            if (!pol)
                break;
            if (pol == pol0)
                break;
            if (m_faces.find(pol) != m_faces.end())
            {
                CFace& face = m_faces[pol];
                for (auto& tri : face.tris)
                {
                    if (tri->v0->vrt == vrt || tri->v1->vrt == vrt || tri->v2->vrt == vrt)
                        return tri;
                }
            }
            npol--;
        }

        return nullptr;
    }

    CVerxID FetchVertex(LXtPointID vrt)
    {
        m_vert.Select(vrt);
        unsigned count;
        m_vert.PolygonCount(&count);
        for (auto i = 0u; i < count; i++)
        {
            LXtPolygonID pol;
            m_vert.PolygonByIndex(i, &pol);
            CTriangleID ref = FetchTriangle(vrt, pol);
            if (ref)
            {
                if (ref->v0->vrt == vrt)
                    return ref->v0;
                if (ref->v1->vrt == vrt)
                    return ref->v1;
                if (ref->v2->vrt == vrt)
                    return ref->v2;
            }
        }
        return nullptr;
    }

    CVerxID AddVertex(LXtPointID vrt, LXtPolygonID pol, CTriangleID tri)
    {
        if (!pol)
        {
            return FetchVertex(vrt);
        }
        //printf("AddVertex vrt (%p) pol (%p)", vrt, pol);
        CTriangleID ref = FetchTriangle(vrt, pol);
        if (ref)
        {
            CVerxID dv = nullptr;

            if (ref->v0->vrt == vrt)
                dv = ref->v0;
            if (ref->v1->vrt == vrt)
                dv = ref->v1;
            if (ref->v2->vrt == vrt)
                dv = ref->v2;

            if (dv)
            {
                dv->tris.push_back(tri);
                return dv;
            }
        }

        m_vertices.push_back(std::make_shared<CVerx>());
        CVerxID dv = m_vertices.back();

        dv->tris.push_back(tri);
        dv->vrt   = vrt;
        dv->tri   = tri;
        dv->index = static_cast<unsigned>(m_vertices.size()-1);
        dv->marks = LXiMARK_ANY;
        dv->collapsed = false;
        m_vert.Select(vrt);
        LXtFVector pos;
        m_vert.Pos(pos);
        LXx_VCPY(dv->pos, pos);
        LXx_VCPY(dv->new_pos, pos);
        m_vert.Index(&dv->vrt_index);
        return dv;
    }

    // Visitor to build triangles from polygons
    //
    class TripleFaceVisitor : public CLxImpl_AbstractVisitor
    {
    public:
        LxResult Evaluate()
        {
            unsigned nvert;
            m_poly.VertexCount(&nvert);
            if (nvert < 3)
                return LXe_OK;

            m_poly.SetMarks(m_mark_done);

            LXtID4 type;
            m_poly.Type(&type);
            if ((type != LXiPTYP_FACE) && (type != LXiPTYP_PSUB) && (type != LXiPTYP_SUBD))
                return LXe_OK;

            m_context->AddPolygon(m_poly.ID());

            std::vector<LXtPointID> points;

            LXtPointID v0, v1, v2;
            if (nvert == 3)
            {
                m_poly.VertexByIndex(0, &v0);
                m_poly.VertexByIndex(1, &v1);
                m_poly.VertexByIndex(2, &v2);
                m_context->AddTriangle(m_poly.ID(), v0, v1, v2);
            }
            else if (MeshUtil::PolygonFixedVertexList(m_mesh, m_poly, points))
            {
                bool done = false;
                if (nvert > 4)
                {
                    LXtVector norm;
                    //MeshUtil::VertexListNormal(m_mesh, points, norm);
                    m_poly.Normal(norm);
                    AxisPlane axisPlane(norm);
                    std::vector<std::vector<LXtPointID>> tris;
                    CTriangulate ctri(m_mesh);
                    LxResult result = LXe_OK;
                    result = ctri.ConstraintDelaunay(axisPlane, points, tris);
                    if (result == LXe_OK)
                    {
                        for (auto& vert : tris)
                        {
                            m_context->AddTriangle(m_poly.ID(), vert[0], vert[1], vert[2]);
                        }
                        done = true;
                    }
                }
                if (!done)
                {
                    v0 = points[0];
                    for (auto i = 1u; i < points.size()-1; i++)
                    {
                        v1 = points[i];
                        v2 = points[i+1];
                        m_context->AddTriangle(m_poly.ID(), v0, v1, v2);
                    }
                }
            }
            else
            {
                unsigned count;
                m_poly.GenerateTriangles(&count);
                for (auto i = 0u; i < count; i++)
                {
                    m_poly.TriangleByIndex(i, &v0, &v1, &v2);
                    m_context->AddTriangle(m_poly.ID(), v0, v1, v2);
                }
            }
            return LXe_OK;
        }

        CLxUser_Mesh    m_mesh;
        CLxUser_Polygon m_poly;
        CLxUser_Point   m_vert;
        LXtMarkMode     m_mark_done;
        struct CMesh*  m_context;
    };

    class PartFaceVisitor : public CLxImpl_AbstractVisitor
    {
    public:
        LxResult Evaluate()
        {
            CLxUser_LogService   s_log;
            unsigned nvert;
            m_poly.VertexCount(&nvert);
            if (nvert < 3)
                return LXe_OK;

            LXtID4 type;
            m_poly.Type(&type);
            if ((type != LXiPTYP_FACE) && (type != LXiPTYP_PSUB) && (type != LXiPTYP_SUBD))
                return LXe_OK;

            if (m_poly.TestMarks(m_mark_done) == LXe_TRUE)
                return LXe_OK;

            m_context->m_parts.push_back(std::make_shared<CPart>());
            CPartID part = m_context->m_parts.back();

            part->index = static_cast<unsigned>(m_context->m_parts.size() - 1);

            CLxUser_Polygon poly, poly1;
            poly.fromMesh(m_mesh);
            poly1.fromMesh(m_mesh);
            CLxUser_Edge edge;
            edge.fromMesh(m_mesh);

            std::vector<LXtPolygonID> stack;
            LXtPolygonID              pol = m_poly.ID();
            stack.push_back(pol);
            m_poly.SetMarks(m_context->m_mark_done);

            while (!stack.empty())
            {
                pol = stack.back();
                stack.pop_back();
                poly.Select(pol);
                CFace& face = m_context->m_faces[pol];
                face.part   = part->index;
                for(auto& tri : face.tris)
                {
                    tri->part = part->index;
                    part->tris.push_back(tri);
                    tri->v0->part = part->index;
                    tri->v1->part = part->index;
                    tri->v2->part = part->index;
                }
                unsigned int nvert = 0u, npol = 0u;
                poly.VertexCount(&nvert);
                for (auto i = 0u; i < nvert; i++)
                {
                    LXtPointID v0{}, v1{};
                    poly.VertexByIndex(i, &v0);
                    poly.VertexByIndex((i + 1) % nvert, &v1);
                    edge.SelectEndpoints(v0, v1);
                    edge.PolygonCount(&npol);
                    for (auto j = 0u; j < npol; j++)
                    {
                        LXtPolygonID pol1;
                        edge.PolygonByIndex(j, &pol1);
                        poly1.Select(pol1);
                        if (poly1.TestMarks(m_mark_done) == LXe_TRUE)
                            continue;
                        poly1.SetMarks(m_mark_done);
                        if (poly1.TestMarks(m_context->m_mark_hide) == LXe_TRUE)
                            continue;
                        if (poly1.TestMarks(m_context->m_mark_lock) == LXe_TRUE)
                            continue;
                        stack.push_back(pol1);
                    }
                }
            }

            return LXe_OK;
        }

        CLxUser_Mesh    m_mesh;
        CLxUser_Polygon m_poly;
        CLxUser_Point   m_vert;
        LXtMarkMode     m_mark_done;
        struct CMesh*   m_context;
    };

    //
    // Build internal mesh representation
    //
    LxResult BuildMesh(CLxUser_Mesh& base_mesh)
    {
        CLxUser_MeshService mesh_svc;
        TripleFaceVisitor triFace;

        m_mesh.set(base_mesh);
        m_poly.fromMesh(m_mesh);
        m_vert.fromMesh(m_mesh);
        m_vmap.fromMesh(m_mesh);

        // triagulate surface polygons.
        triFace.m_mesh = m_mesh;
        triFace.m_poly.fromMesh(m_mesh);
        triFace.m_vert.fromMesh(m_mesh);
        triFace.m_mark_done = mesh_svc.ClearMode(LXsMARK_USER_0);
        triFace.m_context = this;
        triFace.m_poly.Enum(&triFace, m_pick);

        // divides polygons into parts.
        PartFaceVisitor partFace;
        partFace.m_mesh = m_mesh;
        partFace.m_poly.fromMesh(m_mesh);
        partFace.m_vert.fromMesh(m_mesh);
        partFace.m_mark_done = mesh_svc.SetMode(LXsMARK_USER_0);
        partFace.m_context = this;
        partFace.m_poly.Enum(&partFace, m_pick);

        for (auto& v : m_vertices)
        {
            m_parts[v->part]->vrts.push_back(v);
        }
        printf("Build mesh with %zu vertices %zu triangles %zu parts\n", m_vertices.size(), m_triangles.size(), m_parts.size());
        return LXe_OK;
    }

    //
    // Write internal mesh representation back to edit mesh
    //
    LxResult WriteMesh(CLxUser_Mesh& out_mesh)
    {
        //printf("Writing mesh with %zu vertices and %zu triangles\n", m_vertices.size(), m_triangles.size());
        std::vector<LXtPointID> point_ids(m_vertices.size());
        m_vert.fromMesh(out_mesh);
        for (auto& v : m_vertices)
        {
            if (v->collapsed)
                point_ids[v->index] = nullptr;
            else
            {
                LXtPointID new_vrt;
                LXtVector pos;
                LXx_VCPY(pos, v->new_pos);
                m_vert.New(pos, &new_vrt);
                point_ids[v->index] = new_vrt;
            }
        }
        m_poly.fromMesh(out_mesh);

        for (auto& tri : m_triangles)
        {
            if (tri->deleted)
                continue;

            unsigned int rev = 0;
            LXtPointID points[3];
            points[0] = point_ids[tri->v0->index];
            points[1] = point_ids[tri->v1->index];
            points[2] = point_ids[tri->v2->index];

            LXtPolygonID new_pol;
            m_poly.New(LXiPTYP_FACE, points, 3, rev, &new_pol);
        }
        return LXe_OK;
    }

    CEdgeID FetchEdge(CVerxID v0, CVerxID v1)
    {
        for (auto& edge : v0->edge)
        {
            if ((edge->v0 == v0 && edge->v1 == v1) || (edge->v0 == v1 && edge->v1 == v0))
            {
                return edge;
            }
        }
        return nullptr;
    }

    //
    // Collapse the edge given by vertex end indices. Merge v1 to v0 when forward is true,
    // otherwise merge v0 to v1.
    //
    LxResult CollapseEdge(int verx0_index, int verx1_index, bool forward)
    {
        CVerxID v0 = m_vertices[verx0_index];
        CVerxID v1 = m_vertices[verx1_index];

        CEdgeID target_edge = FetchEdge(v0, v1);
        if (!target_edge)
            return LXe_FAILED;

        if (forward)
        {
            // Collapse v1 into v0
            for (auto& tri : v1->tris)
            {
                if (tri->v0 == v1)
                    tri->v0 = v0;
                if (tri->v1 == v1)
                    tri->v1 = v0;
                if (tri->v2 == v1)
                    tri->v2 = v0;
                tri->updated = true;
                v0->tris.push_back(tri);
            }
            for (auto& edge : v1->edge)
            {
                if (edge->v0 == v1)
                    edge->v0 = v0;
                if (edge->v1 == v1)
                    edge->v1 = v0;
                CEdgeID edge1 = FetchEdge(edge->v0, edge->v1);
                if (edge1 != nullptr && edge1 != edge)
                    edge->collapsed = true;
                else
                    v0->edge.push_back(edge);
            }
            v1->collapsed = true;
        }
        else
        {
            // Collapse v0 into v1
            for (auto& tri : v0->tris)
            {
                if (tri->v0 == v0)
                    tri->v0 = v1;
                if (tri->v1 == v0)
                    tri->v1 = v1;
                if (tri->v2 == v0)
                    tri->v2 = v1;
                tri->updated = true;
                v1->tris.push_back(tri);
            }
            for (auto& edge : v0->edge)
            {
                if (edge->v0 == v0)
                    edge->v0 = v1;
                if (edge->v1 == v0)
                    edge->v1 = v1;
                CEdgeID edge1 = FetchEdge(edge->v0, edge->v1);
                if (edge1 != nullptr && edge1 != edge)
                    edge->collapsed = true;
                else
                    v1->edge.push_back(edge);
            }
            v0->collapsed = true;
        }
        target_edge->collapsed = true;
        for (auto& tri : target_edge->tris)
        {
            tri->deleted = true;
            if (forward)
            {
                v1->tris.erase(std::remove(v1->tris.begin(), v1->tris.end(), tri), v1->tris.end());
            }
            else
            {
                v0->tris.erase(std::remove(v0->tris.begin(), v0->tris.end(), tri), v0->tris.end());
            }
            tri->deleted = true;
        }
        return LXe_OK;
    }

    //
    // Apply the triangle mesh into the give edit mesh. The edit mesh must be an instanced mesh from
    // the base mesh used for BuildMesh(). This function uses the source polygons as possible when 
    // are not updated. And it also reuses existing vertices from base mesh as possible.
    //
    LxResult ApplyMesh(CLxUser_Mesh& edit_mesh, bool triple)
    {
        m_mesh.set(edit_mesh);
        m_poly.fromMesh(m_mesh);
        m_vert.fromMesh(m_mesh);
        m_vmap.fromMesh(m_mesh);

        printf("ApplyMesh mesh with %zu vertices and %zu triangles\n", m_vertices.size(), m_triangles.size());
        for (auto& v : m_vertices)
        {
            m_vert.Select(v->vrt);
            if (v->collapsed)
                m_vert.Remove();
            else
                m_vert.SetPos(v->new_pos);
        }

        if (triple)
        {
            for (auto& tri : m_triangles)
            {
                if (tri->deleted)
                    continue;

                unsigned int rev = 0;
                LXtPointID point_ids[3];
                point_ids[0] = tri->v0->vrt;
                point_ids[1] = tri->v1->vrt;
                point_ids[2] = tri->v2->vrt;

                LXtPolygonID new_pol;
                m_poly.NewProto(LXiPTYP_FACE, point_ids, 3, rev, &new_pol);
            }
            for (auto& face : m_faces)
            {
                m_poly.Select(face.first);
                m_poly.Remove();
            }
        }
        else
        {
            for (auto& face : m_faces)
            {
                unsigned int rev = 0;
                std::vector<LXtPointID> points = {};
                GetPointsFromFace(face.second, points);
                m_poly.Select(face.first);
                if (points.size() < 3)
                    m_poly.Remove();

                else if (FaceIsUpdated(face.second) == true)
                {
                    m_poly.SetMarks(m_mark_done);
                    m_poly.SetVertexList(points.data(), static_cast<unsigned>(points.size()), rev);
                }
            }
        }
        return LXe_OK;
    }


    bool FaceIsUpdated(CFace& face)
    {
        for (auto& tri : face.tris)
        {
            if (tri->updated || tri->deleted)
                return true;
        }
        return false;
    }

    //
    // Get vertex list for the given face.
    //
    LxResult GetPointsFromFace(CFace& face, std::vector<LXtPointID>& points)
    {
        points.clear();
        for (auto& tri : face.tris)
        {
            if (tri->deleted)
                continue;
            if (points.empty())
            {
                points.push_back(tri->v0->vrt);
                points.push_back(tri->v1->vrt);
                points.push_back(tri->v2->vrt);
            }
            else
            {
                for (auto i = 0u; i < points.size(); i++)
                {
                    LXtPointID vrt = nullptr;
                    auto j = (i + 1) % points.size();
                    if (points[i] == tri->v1->vrt && points[j] == tri->v0->vrt)
                    {
                        vrt = tri->v2->vrt;
                    }
                    else if (points[i] == tri->v2->vrt && points[j] == tri->v1->vrt)
                    {
                        vrt = tri->v0->vrt;
                    }
                    else if (points[i] == tri->v0->vrt && points[j] == tri->v2->vrt)
                    {
                        vrt = tri->v1->vrt;
                    }
                    if (vrt != nullptr)
                    {
                        points.insert(points.begin() + j, vrt);
                        break;
                    }
                }
            }
        }
        if (points.size() > 2)
        {
            if (points.front() == points.back())
                points.pop_back();
        }
        return LXe_OK;
    }

    void Clear()
    {
        m_vertices.clear();
        m_edges.clear();
        m_triangles.clear();
        m_faces.clear();
        m_parts.clear();
    }

    LxResult Remove(CLxUser_Mesh& edit_mesh)
    {
        m_mesh.set(edit_mesh);
        m_poly.fromMesh(m_mesh);
        m_vert.fromMesh(m_mesh);

        LxResult result = LXe_OK;

        for (auto& vert : m_vertices)
        {
            m_vert.Select(vert->vrt);
            unsigned count;
            m_vert.PolygonCount(&count);
            unsigned nsel = 0;
            for (auto i = 0u; i < count; i++)
            {
                LXtPolygonID pol;
                m_vert.PolygonByIndex(i, &pol);
                for (auto tri : vert->tris)
                {
                    if (tri->pol == pol)
                    {
                        nsel ++;
                        break;
                    }
                }
            }
            if (nsel == count)
            {
                result = m_vert.Remove();
                if (result != LXe_OK)
                    return result;
            }
        }
        for (auto [pol, face] : m_faces)
        {
            m_poly.Select(pol);
            result = m_poly.Remove();
            if (result != LXe_OK)
                return result;
        }

        return LXe_OK;
    }


    std::vector<CEdgeID>     m_edges;
    std::vector<CVerxID>     m_vertices;
    std::vector<CTriangleID> m_triangles;
    std::vector<CPartID>     m_parts;

    std::unordered_map<LXtPolygonID, CFace> m_faces;

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
};
