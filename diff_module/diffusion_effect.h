#pragma once

#include "scene/resources/compositor.h"

class DiffusionEffect : public CompositorEffect {
	GDCLASS(DiffusionEffect, CompositorEffect);

protected:
	static void _bind_methods();

public:
	DiffusionEffect();

	void _render_callback(int p_effect_callback_type, const RenderData *p_render_data, RID &p_color, RID &p_depth, RID &p_motion, RID &p_normal_roughness);

	// Method to manually trigger diffusion generation from GDScript
	void trigger_diffusion();

private:
	void _run_lightning_with_depth(const RID &p_depth, int p_width, int p_height, float p_z_near, float p_z_far);
	bool should_trigger_diffusion = false;
};
