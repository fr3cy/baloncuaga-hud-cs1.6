#pragma once
#include "../imgui/imgui_internal.h"
#include <unordered_map>

template < typename T >
inline T& anim_obj( const char* id, int seed, T arg ) {
    // Match widget identity, including the window and every PushID scope.
    // Repeated hidden labels in tables must not share animation state.
    ImGuiWindow* window = GImGui ? GImGui->CurrentWindow : nullptr;
    ImGuiID scope = window ? window->IDStack.back() : 0;
    ImGuiID im_id = ImHashStr( id, 0, scope );
    if ( seed ) im_id = ImHashData( &seed, sizeof( seed ), im_id );

	static std::unordered_map< ImGuiID, T > map;
	auto result = map.find( im_id );

	if ( result == map.end( ) ) {
		map.insert( { im_id, arg } );
		result = map.find( im_id );
	}

	return result->second;
}

template < typename T >
inline T animate( T v, T min, T max, bool state, float speed = 14.f ) {
	return ImLerp( v, state ? max : min, ImMin(1.f,speed * ImGui::GetIO( ).DeltaTime) );
}

inline ImColor col_anim( ImColor inactive, ImColor active, float anim ) {
	return ImGui::ColorConvertFloat4ToU32( ImLerp( inactive.Value, active.Value, anim ) );
}
