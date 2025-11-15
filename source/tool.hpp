//
// Variational Shape Approximation (VSA) tool and tool operator
//

#pragma once

#include <lxsdk/lxu_attributes.hpp>
#include <lxsdk/lxu_select.hpp>
#include <lxsdk/lxu_attributes.hpp>

#include <lxsdk/lx_layer.hpp>
#include <lxsdk/lx_mesh.hpp>
#include <lxsdk/lx_log.hpp>
#include <lxsdk/lx_plugin.hpp>
#include <lxsdk/lx_seltypes.hpp>
#include <lxsdk/lx_tool.hpp>
#include <lxsdk/lx_toolui.hpp>
#include <lxsdk/lx_layer.hpp>
#include <lxsdk/lx_vector.hpp>
#include <lxsdk/lx_pmodel.hpp>
#include <lxsdk/lx_vmodel.hpp>
#include <lxsdk/lx_channelui.hpp>
#include <lxsdk/lx_draw.hpp>
#include <lxsdk/lx_handles.hpp>

#include <lxsdk/lx_value.hpp>
#include <lxsdk/lx_select.hpp>
#include <lxsdk/lx_seltypes.hpp>

#include "approximate.hpp"

using namespace lx_err;

const char* SRVNAME_TOOL = "tool.approximate";
const char* SRVNAME_TOOLOP = "toolop.approximate";

#define ATTRs_MODE      "mode"
#define ATTRs_PROXIES   "proxies"
#define ATTRs_ITERATION "iteration"
#define ATTRs_NEWMESH   "newMesh"
#define ATTRs_SEGMENT   "segment"
#define ATTRs_SSET      "sset"
#define ATTRs_SETCOLOR  "setColor"

#define ATTRa_MODE      0
#define ATTRa_PROXIES   1
#define ATTRa_ITERATION 2
#define ATTRa_NEWMESH   3
#define ATTRa_SEGMENT   4
#define ATTRa_SSET      5
#define ATTRa_SETCOLOR  6

#ifndef LXx_OVERRIDE
#define LXx_OVERRIDE override
#endif

//
// The Tool Operation is evaluated by the procedural modeling system.
//
class CToolOp : public CLxImpl_ToolOperation
{
	public:
        // ToolOperation Interface
		LxResult    top_Evaluate(ILxUnknownID vts)  LXx_OVERRIDE;

        CLxUser_FalloffPacket falloff;
        CLxUser_Subject2Packet subject;

        unsigned offset_view;
        unsigned offset_screen;
        unsigned offset_falloff;
        unsigned offset_subject;
        unsigned offset_input;

        unsigned m_flags;

        int    m_mode;      // Approximation mode
        int    m_proxies;
        int    m_iteration;
        int    m_segment;
        int    m_new_mesh;
        int    m_set_color;
        std::string m_sset;
};

/*
 * Straight-skeleton tool operator. Basic tool and tool model methods are defined here. The
 * attributes interface is inherited from the utility class.
 */

class CTool : public CLxImpl_Tool, public CLxImpl_ToolModel, public CLxDynamicAttributes, public CLxImpl_ChannelUI
{
public:
    CTool();

    void        tool_Reset() LXx_OVERRIDE;
    LXtObjectID tool_VectorType() LXx_OVERRIDE;
    const char* tool_Order() LXx_OVERRIDE;
    LXtID4      tool_Task() LXx_OVERRIDE;
	LxResult	tool_GetOp(void **ppvObj, unsigned flags) LXx_OVERRIDE;

    unsigned    tmod_Flags() LXx_OVERRIDE;
	void		tmod_Initialize (ILxUnknownID vts, ILxUnknownID adjust, unsigned flags) LXx_OVERRIDE;
    LxResult    tmod_Enable(ILxUnknownID obj) LXx_OVERRIDE;
    LxResult    tmod_Down(ILxUnknownID vts, ILxUnknownID adjust) LXx_OVERRIDE;
    void        tmod_Move(ILxUnknownID vts, ILxUnknownID adjust) LXx_OVERRIDE;
    void        tmod_Up(ILxUnknownID vts, ILxUnknownID adjust) LXx_OVERRIDE;

    using CLxDynamicAttributes::atrui_UIHints;  // to distinguish from the overloaded version in CLxImpl_AttributesUI

    void        atrui_UIHints2(unsigned int index, CLxUser_UIHints& hints) LXx_OVERRIDE;
    LxResult	atrui_DisableMsg (unsigned int index, ILxUnknownID msg) LXx_OVERRIDE;

    LxResult    cui_Enabled           (const char *channelName, ILxUnknownID msg, ILxUnknownID item, ILxUnknownID read)	LXx_OVERRIDE;
    LxResult    cui_DependencyCount   (const char *channelName, unsigned *count) LXx_OVERRIDE;
    LxResult    cui_DependencyByIndex (const char *channelName, unsigned index, LXtItemType *depItemType, const char **depChannelName) LXx_OVERRIDE;

    bool TestPolygon(unsigned int& primary_index);

    CLxUser_LogService   s_log;
    CLxUser_LayerService s_layer;
    CLxUser_VectorType   v_type;
    CLxUser_SelectionService s_sel;

    unsigned offset_view;
    unsigned offset_screen;
    unsigned offset_falloff;
    unsigned offset_subject;
    unsigned offset_input;
    unsigned offset_event;
	unsigned offset_center;
    unsigned mode_select;
	
	LXtItemType m_itemType;

    static LXtTagInfoDesc descInfo[];
    int    m_proxies0;
};

