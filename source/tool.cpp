//
// Variational Shape Approximation (VSA) tool and tool operator
//

#include "tool.hpp"

/*
 * On create we add our one tool attribute. We also allocate a vector type
 * and select mode mask.
 */
CTool::CTool()
{
    static const LXtTextValueHint approximate_mode[] = {
        { CApproximate::APPROXIMATION, "approximation" },
        { CApproximate::SEGMENTATION, "segmentation" },
        { 0, "=approximate_mode" }, 0
    };
    static const LXtTextValueHint approximate_segment[] = {
        { CApproximate::POLY_MATR, "material" },
        { CApproximate::POLY_PART, "part" },
        { CApproximate::EDGE_SSET, "sset" },
        { 0, "=approximate_segment" }, 0
    };

    CLxUser_PacketService sPkt;
    CLxUser_MeshService   sMesh;

    dyna_Add(ATTRs_MODE, LXsTYPE_INTEGER);
    dyna_SetHint(ATTRa_MODE, approximate_mode);

    dyna_Add(ATTRs_PROXIES, LXsTYPE_INTEGER);

    dyna_Add(ATTRs_ITERATION, LXsTYPE_INTEGER);

    dyna_Add(ATTRs_NEWMESH, LXsTYPE_BOOLEAN);

    dyna_Add(ATTRs_SEGMENT, LXsTYPE_INTEGER);
    dyna_SetHint(ATTRa_SEGMENT, approximate_segment);

    dyna_Add(ATTRs_SSET, LXsTYPE_STRING);

    dyna_Add(ATTRs_SETCOLOR, LXsTYPE_BOOLEAN);

    tool_Reset();

    sPkt.NewVectorType(LXsCATEGORY_TOOL, v_type);
    sPkt.AddPacket(v_type, LXsP_TOOL_VIEW_EVENT, LXfVT_GET);
    sPkt.AddPacket(v_type, LXsP_TOOL_SCREEN_EVENT, LXfVT_GET);
    sPkt.AddPacket(v_type, LXsP_TOOL_FALLOFF, LXfVT_GET);
    sPkt.AddPacket(v_type, LXsP_TOOL_SUBJECT2, LXfVT_GET);
    sPkt.AddPacket(v_type, LXsP_TOOL_INPUT_EVENT, LXfVT_GET);
	sPkt.AddPacket (v_type, LXsP_TOOL_EVENTTRANS,  LXfVT_GET);
	sPkt.AddPacket (v_type, LXsP_TOOL_ACTCENTER,   LXfVT_GET);

    offset_view = sPkt.GetOffset(LXsCATEGORY_TOOL, LXsP_TOOL_VIEW_EVENT);
    offset_screen = sPkt.GetOffset(LXsCATEGORY_TOOL, LXsP_TOOL_SCREEN_EVENT);
    offset_falloff = sPkt.GetOffset(LXsCATEGORY_TOOL, LXsP_TOOL_FALLOFF);
    offset_subject = sPkt.GetOffset(LXsCATEGORY_TOOL, LXsP_TOOL_SUBJECT2);
    offset_input = sPkt.GetOffset(LXsCATEGORY_TOOL, LXsP_TOOL_INPUT_EVENT);
	offset_event  = sPkt.GetOffset (LXsCATEGORY_TOOL, LXsP_TOOL_EVENTTRANS);
	offset_center = sPkt.GetOffset (LXsCATEGORY_TOOL, LXsP_TOOL_ACTCENTER);
    mode_select = sMesh.SetMode("select");

    m_iteration0 = 0;
}

/*
 * Reset sets the attributes back to defaults.
 */
void CTool::tool_Reset()
{
    CApproximate    vsa;
    dyna_Value(ATTRa_MODE).SetInt(vsa.m_mode);
    dyna_Value(ATTRa_PROXIES).SetInt(vsa.m_proxies);
    dyna_Value(ATTRa_ITERATION).SetInt(vsa.m_iteration);
    dyna_Value(ATTRa_NEWMESH).SetInt(1);
    dyna_Value(ATTRa_SEGMENT).SetInt(vsa.m_segment);
    dyna_Value(ATTRa_SETCOLOR).SetInt(vsa.m_set_color);
    dyna_Value(ATTRa_SSET).SetString(vsa.m_sset.c_str());
}

/*
 * Boilerplate methods that identify this as an action (state altering) tool.
 */
LXtObjectID CTool::tool_VectorType()
{
    return v_type.m_loc;  // peek method; does not add-ref
}

const char* CTool::tool_Order()
{
    return LXs_ORD_ACTR;
}

LXtID4 CTool::tool_Task()
{
    return LXi_TASK_ACTR;
}

LxResult CTool::tool_GetOp(void** ppvObj, unsigned flags)
{
    CLxSpawner<CToolOp> spawner(SRVNAME_TOOLOP);
    CToolOp*            toolop = spawner.Alloc(ppvObj);

	if (!toolop)
	{
		return LXe_FAILED;
	}

    dyna_Value(ATTRa_MODE).GetInt(&toolop->m_mode);
    dyna_Value(ATTRa_PROXIES).GetInt(&toolop->m_proxies);
    dyna_Value(ATTRa_ITERATION).GetInt(&toolop->m_iteration);
    dyna_Value(ATTRa_NEWMESH).GetInt(&toolop->m_new_mesh);
    dyna_Value(ATTRa_SEGMENT).GetInt(&toolop->m_segment);
    dyna_Value(ATTRa_SETCOLOR).GetInt(&toolop->m_set_color);
    dyna_Value(ATTRa_SSET).GetString(toolop->m_sset);

    toolop->offset_view = offset_view;
    toolop->offset_screen = offset_screen;
    toolop->offset_falloff = offset_falloff;
    toolop->offset_subject = offset_subject;
    toolop->offset_input = offset_input;

	return LXe_OK;
}

LXtTagInfoDesc CTool::descInfo[] =
{
	{LXsTOOL_PMODEL, "."},
	{LXsTOOL_USETOOLOP, "."},
	{LXsPMODEL_SELECTIONTYPES, LXsSELOP_TYPE_POLYGON},
	{0}

};

/*
 * We employ the simplest possible tool model -- default hauling. We indicate
 * that we want to haul one attribute, we name the attribute, and we implement
 * Initialize() which is what to do when the tool activates or re-activates.
 * In this case set the axis to the current value.
 */
unsigned CTool::tmod_Flags()
{
    return LXfTMOD_I0_INPUT | LXfTMOD_DRAW_3D;
}

LxResult CTool::tmod_Enable(ILxUnknownID obj)
{
    CLxUser_Message msg(obj);
    unsigned int primary_index = 0;

    if (TestPolygon(primary_index) == false)
    {
        msg.SetCode(LXe_CMD_DISABLED);
        msg.SetMessage(SRVNAME_TOOL, "NoPolygon", 0);
        return LXe_DISABLED;
    }

	CLxUser_LayerService _lyr_svc;
	CLxSceneSelection	 scene_sel;
	CLxUser_Scene		 scene;

	scene_sel.Get (scene);
	if (scene.test ())
		_lyr_svc.SetScene (scene);

	if (_lyr_svc.IsProcedural (primary_index) == LXe_TRUE)
	{
        msg.SetCode(LXe_CMD_DISABLED);
        msg.SetMessage(SRVNAME_TOOL, "proceduralMesh", 0);
        return LXe_DISABLED;
	}
    return LXe_OK;
}

LxResult CTool::tmod_Down(ILxUnknownID vts, ILxUnknownID adjust)
{
	CLxUser_AdjustTool	 at (adjust);
	CLxUser_VectorStack	 vec (vts);
	LXpToolActionCenter* acen = (LXpToolActionCenter *) vec.Read (offset_center);
	LXpToolInputEvent*   ipkt = (LXpToolInputEvent *) vec.Read (offset_input);

    dyna_Value(ATTRa_ITERATION).GetInt(&m_iteration0);

    return LXe_TRUE;
}

void CTool::tmod_Move(ILxUnknownID vts, ILxUnknownID adjust)
{
    CLxUser_AdjustTool at(adjust);
    CLxUser_VectorStack vec(vts);
    LXpToolScreenEvent*  spak = static_cast<LXpToolScreenEvent*>(vec.Read(offset_screen));

    double delta = spak->cx - spak->px;
    int iteration = m_iteration0 + static_cast<int>(delta * 0.1);
    if (iteration < 0)
        iteration = 0;
    at.SetInt(ATTRa_ITERATION, iteration);
}

void CTool::tmod_Up(ILxUnknownID vts, ILxUnknownID adjust)
{
    m_iteration0 = 0;
}

void CTool::atrui_UIHints2(unsigned int index, CLxUser_UIHints& hints)
{
    switch (index)
    {   
        case ATTRa_PROXIES:
        case ATTRa_ITERATION:
            hints.MinInt(0);
            break;
    }
}

LxResult CTool::atrui_DisableMsg (unsigned int index, ILxUnknownID msg)
{
    CLxUser_Message		 message (msg);

    int mode;
    int segment;
    dyna_Value(ATTRa_MODE).GetInt(&mode);
    dyna_Value(ATTRa_SEGMENT).GetInt(&segment);

    switch (index) {
        case ATTRa_SEGMENT:
            if (mode != CApproximate::SEGMENTATION)
            {
                message.SetCode (LXe_DISABLED);
                message.SetMessage ("tool.approximate", "OnlySegmentation", 0);
                return LXe_DISABLED;
            }
            break;
        case ATTRa_SSET:
            if (segment != CApproximate::EDGE_SSET)
            {
                message.SetCode (LXe_DISABLED);
                message.SetMessage ("tool.approximate", "OnlyEdgeSset", 0);
                return LXe_DISABLED;
            }
            break;
        case ATTRa_SETCOLOR:
            if ((segment != CApproximate::POLY_MATR) && (segment != CApproximate::POLY_PART))
            {
                message.SetCode (LXe_DISABLED);
                message.SetMessage ("tool.approximate", "OnlyPolyTags", 0);
                return LXe_DISABLED;
            }
            break;
        case ATTRa_NEWMESH:
            if (mode != CApproximate::APPROXIMATION)
            {
                message.SetCode (LXe_DISABLED);
                message.SetMessage ("tool.approximate", "OnlyApproximation", 0);
                return LXe_DISABLED;
            }
            break;
    }
    return LXe_OK;
}

bool CTool::TestPolygon(unsigned int& primary_index)
{
    /*
     * Start the scan in read-only mode.
     */
    CLxUser_LayerScan scan;
    CLxUser_Mesh      mesh;
    unsigned          i, n, count;
    bool              ok = false;

    primary_index = 0;

    s_layer.BeginScan(LXf_LAYERSCAN_ACTIVE | LXf_LAYERSCAN_MARKPOLYS, scan);

    /*
     * Count the polygons in all mesh layers.
     */
    if (scan)
    {
        n = scan.NumLayers();
        for (i = 0; i < n; i++)
        {
            scan.BaseMeshByIndex(i, mesh);
            mesh.PolygonCount(&count);
            if (count > 0)
            {
                ok = true;
                primary_index = i;
                break;
            }
        }
        scan.Apply();
    }

    /*
     * Return false if there is no polygons in any active layers.
     */
    return ok;
}

LxResult CTool::cui_Enabled (const char *channelName, ILxUnknownID msg_obj, ILxUnknownID item_obj, ILxUnknownID read_obj)
{
	CLxUser_Item	 	 item (item_obj);
	CLxUser_ChannelRead	 chan_read (read_obj);

    std::string name(channelName);

	if (name == ATTRs_SEGMENT)
    {
        if ((chan_read.IValue (item, ATTRs_MODE) != CApproximate::SEGMENTATION))
		    return LXe_CMD_DISABLED;
    }
	else if (name == ATTRs_SSET)
    {
        if ((chan_read.IValue (item, ATTRs_MODE) != CApproximate::SEGMENTATION))
		    return LXe_CMD_DISABLED;
        if ((chan_read.IValue (item, ATTRs_SEGMENT) != CApproximate::EDGE_SSET))
		    return LXe_CMD_DISABLED;
    }
	else if (name == ATTRs_SETCOLOR)
    {
        if ((chan_read.IValue (item, ATTRs_MODE) != CApproximate::SEGMENTATION))
		    return LXe_CMD_DISABLED;
        if ((chan_read.IValue (item, ATTRs_SEGMENT) == CApproximate::EDGE_SSET))
		    return LXe_CMD_DISABLED;
    }
	else if (name == ATTRs_NEWMESH)
    {
        if ((chan_read.IValue (item, ATTRs_MODE) != CApproximate::APPROXIMATION))
		    return LXe_CMD_DISABLED;
    }
	return LXe_OK;
}

LxResult CTool::cui_DependencyCount (const char *channelName, unsigned *count)
{
	count[0] = 0;

	if (std::string(channelName) == ATTRs_SEGMENT)
		count[0] = 1;
	else if (std::string(channelName) == ATTRs_SSET)
		count[0] = 2;
	else if (std::string(channelName) == ATTRs_SETCOLOR)
		count[0] = 2;
	else if (std::string(channelName) == ATTRs_NEWMESH)
		count[0] = 1;
	return LXe_OK;
}

LxResult CTool::cui_DependencyByIndex (const char *channelName, unsigned index, LXtItemType *depItemType, const char **depChannel)
{
	depItemType[0] = m_itemType;
	
	if (std::string(channelName) == ATTRs_SEGMENT)
	{
		depChannel[0] = ATTRs_MODE;
		return LXe_OK;
	}
	else if (std::string(channelName) == ATTRs_SSET)
	{
        if (index == 0)
        {
            depChannel[0] = ATTRs_MODE;
            return LXe_OK;
        }
        else if (index == 1)
        {
            depChannel[0] = ATTRs_SEGMENT;
            return LXe_OK;
        }
		return LXe_OK;
	}
	else if (std::string(channelName) == ATTRs_SETCOLOR)
	{
        if (index == 0)
        {
            depChannel[0] = ATTRs_MODE;
            return LXe_OK;
        }
        else if (index == 1)
        {
            depChannel[0] = ATTRs_SEGMENT;
            return LXe_OK;
        }
		return LXe_OK;
	}
	else if (std::string(channelName) == ATTRs_NEWMESH)
	{
		depChannel[0] = ATTRs_MODE;
		return LXe_OK;
	}
	return LXe_OUTOFBOUNDS;
}

/*
 * Tool evaluation uses layer scan interface to walk through all the active
 * meshes and visit all the selected polygons.
 */
LxResult CToolOp::top_Evaluate(ILxUnknownID vts)
{
    CLxUser_VectorStack vec(vts);

    /*
     * Start the scan in edit mode.
     */
    CLxUser_LayerScan  scan;
    CLxUser_Mesh       base_mesh, edit_mesh;
    CApproximate       vsa;

    if ((m_iteration == 0) || (m_proxies == 0))
        return LXe_OK;

    if (vec.ReadObject(offset_subject, subject) == false)
        return LXe_FAILED;
    if (vec.ReadObject(offset_falloff, falloff) == false)
        return LXe_FAILED;

    CLxUser_MeshService   s_mesh;

    LXtMarkMode pick = s_mesh.SetMode(LXsMARK_SELECT);

    subject.BeginScan(LXf_LAYERSCAN_EDIT_POLYS, scan);

    vsa.m_iteration = m_iteration;
    vsa.m_proxies = m_proxies;
    vsa.m_mode = m_mode;
    vsa.m_segment = m_segment;
    vsa.m_set_color = m_set_color;
    vsa.m_sset = m_sset;
    vsa.m_new_mesh = m_new_mesh;

    auto n = scan.NumLayers();
    for (auto i = 0u; i < n; i++)
    {
        scan.BaseMeshByIndex(i, base_mesh);
        scan.EditMeshByIndex(i, edit_mesh);

        vsa.ApproximateMesh(base_mesh);

        if (vsa.m_mode == CApproximate::APPROXIMATION)
        {
            if (vsa.m_new_mesh)
            {
                CLxUser_Mesh new_mesh;
                MeshUtil::NewMesh(new_mesh);
                vsa.WriteApproximation(new_mesh);
            }
            else
            {
                edit_mesh.Clear();
                vsa.WriteApproximation(edit_mesh);
            }
        }
        else if (vsa.m_mode == CApproximate::SEGMENTATION)
        {
            vsa.WriteSegmentations(edit_mesh);
        }


        scan.SetMeshChange(i, LXf_MESHEDIT_GEOMETRY);
    }

    scan.Apply();
    return LXe_OK;
}


/*
 * Export tool server.
 */
void initialize()
{
    CLxGenericPolymorph* srv;

    srv = new CLxPolymorph<CTool>;
    srv->AddInterface(new CLxIfc_Tool<CTool>);
    srv->AddInterface(new CLxIfc_ToolModel<CTool>);
    srv->AddInterface(new CLxIfc_Attributes<CTool>);
    srv->AddInterface(new CLxIfc_AttributesUI<CTool>);
    srv->AddInterface(new CLxIfc_ChannelUI<CTool>);
    srv->AddInterface(new CLxIfc_StaticDesc<CTool>);
    thisModule.AddServer(SRVNAME_TOOL, srv);

    srv = new CLxPolymorph<CToolOp>;
    srv->AddInterface(new CLxIfc_ToolOperation<CToolOp>);
    lx::AddSpawner(SRVNAME_TOOLOP, srv);
}
