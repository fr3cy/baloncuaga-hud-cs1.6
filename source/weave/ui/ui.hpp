#pragma once
#include <windows.h>
#include <algorithm>
#include <cfloat>
#include <cmath>

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "../imgui/imgui.h"
#include "../imgui/imgui_internal.h"
#include "animations.hpp"
#include "hashes.hpp"
#include "bytes.hpp"

#include <string>
#include <vector>
#include <functional>


using namespace ImGui;

#define font( n ) ImGui::GetIO( ).Fonts->Fonts[ n ]

#define ALPHA    ( ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_NoBorder )
#define NO_ALPHA ( ImGuiColorEditFlags_NoTooltip    | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_NoBorder )

struct tab_t {
	const char* m_icon;
	const char* m_name;

	int cur_subtab = 0;
	int next_subtab = 0;
	std::vector< const char* > m_subtabs;
	std::vector< std::function< void( ) > > pages;

	tab_t( const char* icon, const char* name, std::vector< const char* > subtabs ) {
		m_icon = icon;
		m_name = name;
		m_subtabs = subtabs;
	}
};

namespace ui {
	inline ImVec2 size{ 670, 478 };

	inline float anim = 0.f;
	inline float anim_dst = 1.f;
	inline float content_anim = 0.f;
	inline float content_anim_dst = 1.f;
	void handle_anims( );

	// Set by MenuTheme::apply while the Aurora theme draws; widgets add glow and gradients.
	inline bool aurora = false;
    inline bool carbonMoon = false;
    inline ImFont *menuTitleFont = nullptr;
	inline ImU32 aurora_glow = 0;
	// Colour with style alpha applied and an extra alpha multiplier.
	inline ImU32 fade( ImU32 col, float alpha ) {
		ImVec4 c = ImGui::ColorConvertU32ToFloat4( col );
		c.w *= alpha;
		return ImGui::GetColorU32( c );
	}

	void render_page( );
	void add_page( int tab_idx, std::function< void( ) > c );

	void rotate_start( );
    ImVec2 rotation_center( );
    void rotate_end( float rad, ImVec2 center );

	inline int cur_tab = 0;
	inline int next_tab = 0;
	inline std::vector<tab_t> tabs = {
        {ICON_FA_BRUSH,"Models",{"Knife","HE grenade","Flashbang","Smoke"}},
        {ICON_FA_CROSSHAIRS,"Camera",{}},
        {ICON_FA_CROSSHAIRS,"Crosshair",{}},
        {ICON_FA_EYE,"HUD",{"Display","Chat","Radar"}},
        {ICON_FA_GEAR,"Scroll trainer",{}},
        {ICON_FA_FILE,"Chat history",{}},
        {ICON_FA_GEAR,"Settings",{}},
        {ICON_FA_FILE,"Misc",{}},
        {ICON_FA_FILE,"Stats",{}}
    };
	void tab( int num );
	void subtab( int num, int tab_num );

	void begin_child( const char* name, ImVec2 size );
	void end_child( );

	bool color( const char* name, float col[4], bool alpha = true );
}
